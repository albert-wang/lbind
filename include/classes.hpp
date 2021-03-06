#pragma once
#include "lua.hpp"
#include "object.h"
#include "convert.hpp"
#include "function.hpp"
#include "policies.hpp"

namespace lbind
{
	class Scope;

	namespace Detail
	{
		//Ownership semantics are stored in the bottom two bits of non-primitive pointers.
		enum OwnershipTypes
		{
			Unowned = 0x0,
			Owned   = 0x1
		};

		void * ownership(void *, boost::uint8_t vals);
		boost::uint8_t ownership(void *);
		void * ownershipless(void *);

		template<typename T>
		struct Metatables
		{
			static const char * name;
			static int instanceMetatableIndex;
			static int staticMetatableIndex;
		};

		template<typename T>
		const char * Metatables<T>::name = nullptr;

		template<typename T>
		int Metatables<T>::instanceMetatableIndex = 0;

		template<typename T>
		int Metatables<T>::staticMetatableIndex = 0;

		//Basically a runtime version of Metatables
		struct ClassRepresentation
		{
			const int * instanceIndex;
			const int * staticIndex;
			const char * name;
		};

		struct MemberBase
		{
			virtual void push(lua_State * s, void * target) = 0;
			virtual int set(lua_State * s, void * target) = 0;
		};

		template<typename T, typename G>
		struct PropertyReadOnlyMember : public MemberBase
		{
			BOOST_STATIC_ASSERT(boost::is_member_function_pointer<G>::value);

			explicit PropertyReadOnlyMember(G getter)
				:getter(getter)
			{}

			void push(lua_State * s, void * target)
			{
				T * t = static_cast<T *>(target);

				typedef typename FunctionTraits<G>::result_type MemberType;
				Convert<typename Undecorate<MemberType>::type>::to(s, ((t)->*(getter))());
			}

			int set(lua_State * s, void * target)
			{
				//Error!
				lua_pushstring(s, "LBind error: Could not set a read-only value");
				lua_error(s);
				return 0;
			}

			G getter;
		};

		template<typename T, typename G, typename S>
		struct PropertyMember : public MemberBase
		{
			BOOST_STATIC_ASSERT(boost::is_member_function_pointer<G>::value);

			BOOST_STATIC_ASSERT(boost::is_member_function_pointer<S>::value);
			BOOST_STATIC_ASSERT(boost::function_types::function_arity<S>::value == 2);

			PropertyMember(G getter, S setter)
				:getter(getter)
				,setter(setter)
			{}

			void push(lua_State * s, void * target)
			{
				T * t = static_cast<T *>(target);

				typedef typename FunctionTraits<G>::result_type MemberType;
				Convert<typename Undecorate<MemberType>::type>::to(s, ((t)->*(getter))());
			}

			int set(lua_State * s, void * target)
			{
				T * t = static_cast<T *>(target);

				typedef typename boost::fusion::result_of::at_c<typename FunctionTraits<S>::parameter_types, 1>::type MemberType;
				typedef Convert<typename Undecorate<MemberType>::type> Converter;

				typename Converter::type value;

				int consumed = Convert<typename Undecorate<MemberType>::type>::from(s, -1, value);

				((t)->*(setter))(value);
				return consumed;
			}

			G getter;
			S setter;
		};

		template<typename T, typename M>
		struct ReadonlyMember : public MemberBase
		{
			explicit ReadonlyMember(M m)
				:pointer(m)
			{}

			void push(lua_State * s, void * target)
			{
				typedef decltype(static_cast<T *>(target)->*(pointer)) MemberType;
				Convert<typename Undecorate<MemberType>::type>::to(s, static_cast<T *>(target)->*(pointer));
			}

			int set(lua_State * s, void * target)
			{
				//Error!
				lua_pushstring(s, "LBind error: Could not set a read-only value");
				lua_error(s);
				return 0;
			}

			M pointer;
		};

		template<typename T, typename M>
		struct ReadWriteMember : public MemberBase
		{
			explicit ReadWriteMember(M m)
				:pointer(m)
			{}

			void push(lua_State * s, void * target)
			{
				typedef decltype(static_cast<T *>(target)->*(pointer)) MemberType;
				Convert<typename Undecorate<MemberType>::type>::to(s, static_cast<T *>(target)->*(pointer));
			}

			int set(lua_State * s, void * target)
			{
				typedef decltype(static_cast<T *>(target)->*(pointer)) MemberType;
				typedef Convert<typename Undecorate<MemberType>::type> Converter;

				typename Converter::type value;

				int consumed = Convert<typename Undecorate<MemberType>::type>::from(s, -1, value);
				(static_cast<T *>(target)->*(pointer)) = Converter::forward(std::move(value));

				return consumed;
			}

			M pointer;
		};

		template<typename T>
		struct Construct0
		{
			static T * invoke(Ignored*)
			{
				T * result = new T();
				return static_cast<T *>(ownership(result, Owned));
			}
		};

		template<typename R, typename ...T>
		struct Construct
		{
			static R * invoke(Ignored*, T&&... args)
			{
				R * result = new R(std::forward<T>(args)...);
				return static_cast<R *>(ownership(result, Owned));
			}
		};

		template<typename T>
		class ClassRegistrar
		{
			static int warn(lua_State *)
			{
				std::cout << "Newindex called!";
				return 0;
			}

			//GC operations, deletes the object if it is owned by lua.
			static int collect(lua_State * s)
			{
				//Stack is val
				void * userdata = *(void **)lua_touserdata(s, -1);
				if (ownership(userdata) == Owned)
				{
					T * val = static_cast<T *>(ownershipless(userdata));
					delete val;
				}

				return 0;
			}

			// Processes indexing operations.
			static int index(lua_State * s)
			{
				//Stack is [val, key]
				int upvalue = lua_upvalueindex(1);
				ClassRepresentation * rep = static_cast<ClassRepresentation *>(lua_touserdata(s, upvalue));
				lua_rawgeti(s, LUA_REGISTRYINDEX, *rep->instanceIndex);

				//Stack is now [val, key, metatable]
				lua_pushvalue(s, -2);
				int type = lua_rawget(s, -2);

				//Stack is now [val, key, metatable, index-result]
				//Standard access?
				if (type != LUA_TLIGHTUSERDATA)
				{
					//Move the top element to the 3rd element, and pop 3 values.
					lua_rotate(s, -1, 4);
					lua_pop(s, 3);

					return 1;
				}
				else
				{
					//A member access. Get the member description, the base pointer, and then push
					// the value up.
					MemberBase * member = static_cast<MemberBase *>(lua_touserdata(s, -1));
					void * target = ownershipless(*(void **)lua_touserdata(s, -4));

					member->push(s, target);

					//Stack is now [val, key, metatable, index-result, get]. Rotate value to the end of
					//the stack, and pop the useless values.
					lua_rotate(s, -1, 5);
					lua_pop(s, 4);
					return 1;
				}

				//More processing is needed.
				return 0;
			}

			static int newindex(lua_State * s)
			{
				//Stack is [target, key, value]
				int upvalue = lua_upvalueindex(1);
				ClassRepresentation * rep = static_cast<ClassRepresentation *>(lua_touserdata(s, upvalue));
				lua_rawgeti(s, LUA_REGISTRYINDEX, *rep->instanceIndex);

				//Stack is now [target, key, value, metatable]
				lua_pushvalue(s, -3);
				int type = lua_rawget(s, -2);
				//Stack is now [target, key, value, metatable, index-result]
				if (type == LUA_TNIL)
				{
					//Yeah, thats an error.
					lua_pushstring(s, "Attempting to set a value on a C++ class is invalid");
					lua_error(s);

					//Never reached.
					return 0;
				}

				if (type == LUA_TLIGHTUSERDATA)
				{
					MemberBase * member = static_cast<MemberBase *>(lua_touserdata(s, -1));
					void * target = ownershipless(*(void **)lua_touserdata(s, -5));

					lua_pop(s, 2);

					//Stack is now [target, key, value] again
					//This should consume 1 element (value) from the stack.
					int consumed = member->set(s, target);

					lua_pop(s, 2 + consumed);
					return 0;
				}

				return 0;
			}
		public:
			ClassRegistrar(lua_State * state, lbind::StackObject meta, ClassRepresentation * rep, int scopeIndex, Scope * containingScope)
				:state(state)
				,metatable(meta)
				,representation(rep)
				,scopeIndex(scopeIndex)
				,containingScope(containingScope)
			{}

			template<typename U>
			ClassRegistrar& constant(boost::string_ref name, U t)
			{
				BOOST_STATIC_ASSERT(Convert<U>::is_primitive::value);

				assert(metatable.index() == lua_gettop(state));
				Convert<U>::to(state, t);
				lua_setfield(state, -2, name.data());
				assert(metatable.index() == lua_gettop(state));

				return *this;
			}

			ClassRegistrar& constructor()
			{
				boost::fusion::vector<null_policy_t> np;
				constructors.push_back(Detail::createFunction(Construct0<T>::invoke, np));
				return *this;
			}

			template<typename ...AT>
			ClassRegistrar& constructor()
			{
				boost::fusion::vector<null_policy_t> np;
				constructors.push_back(Detail::createFunction(Construct<T, AT...>::invoke, np));
				return *this;
			}

			template<typename F, typename P>
			ClassRegistrar& def(boost::string_ref name, F f, P p)
			{
				assert(metatable.index() == lua_gettop(state));

				boost::fusion::vector<P> policies(p);
				resolveFunctionOverloads(state, name.data(), f, policies);

				assert(metatable.index() == lua_gettop(state));
				return *this;
			}

			template<typename F>
			ClassRegistrar& def(boost::string_ref name, F f)
			{
				return def(name, f, null_policy);
			}

			//This is a member pointer.
			template<typename M>
			ClassRegistrar& def_readonly(boost::string_ref name, M m)
			{
				BOOST_STATIC_ASSERT(boost::is_member_object_pointer<M>::value);
				assert(metatable.index() == lua_gettop(state));

				MemberBase * member = new ReadonlyMember<T, M>(m);
				lua_pushlightuserdata(state, member);
				lua_setfield(state, -2, name.data());

				assert(metatable.index() == lua_gettop(state));
				return *this;
			}

			//This is a member pointer.
			template<typename M>
			ClassRegistrar& def_readwrite(boost::string_ref name, M m)
			{
				BOOST_STATIC_ASSERT(boost::is_member_object_pointer<M>::value);
				assert(metatable.index() == lua_gettop(state));

				MemberBase * member = new ReadWriteMember<T, M>(m);
				lua_pushlightuserdata(state, member);
				lua_setfield(state, -2, name.data());

				assert(metatable.index() == lua_gettop(state));
				return *this;
			}

			template<typename G>
			ClassRegistrar& property(boost::string_ref name, G getter)
			{
				MemberBase * member = new PropertyReadOnlyMember<T, G>(getter);
				lua_pushlightuserdata(state, member);
				lua_setfield(state, -2, name.data());

				return *this;
			}

			template<typename G, typename S>
			ClassRegistrar& property(boost::string_ref name, G getter, S setter)
			{
				MemberBase * member = new PropertyMember<T, G, S>(getter, setter);
				lua_pushlightuserdata(state, member);
				lua_setfield(state, -2, name.data());

				return *this;
			}

			Scope& endclass()
			{
				StackCheck check(state, 1, 0);

				using namespace lbind::Detail;
				assert(metatable.index() == lua_gettop(state));

				Metatables<T>::instanceMetatableIndex = luaL_ref(state, LUA_REGISTRYINDEX);
				lua_rawgeti(state, LUA_REGISTRYINDEX, Metatables<T>::instanceMetatableIndex);

				lua_pushlightuserdata(state, representation);
				lua_pushcclosure(state, &ClassRegistrar<T>::index, 1);
				lua_setfield(state, -2, "__index");

				lua_pushlightuserdata(state, representation);
				lua_pushcclosure(state, &ClassRegistrar<T>::newindex, 1);
				lua_setfield(state, -2, "__newindex");

				lua_pushlightuserdata(state, representation);
				lua_pushcclosure(state, &ClassRegistrar<T>::collect, 1);
				lua_setfield(state, -2, "__gc");

				//Also we need a metatable for __call for constructors.
				if (constructors.size())
				{
					InternalState * internal = Detail::getInternalState(state);

					FunctionBase * ctor = constructors[0];
					if (constructors.size() > 1)
					{
						OverloadedFunction * f = new OverloadedFunction();
						f->canidates = constructors;
						ctor = f;

						internal->registerFunction(f);
					}

					for (size_t i = 0; i < constructors.size(); ++i)
					{
						internal->registerFunction(constructors[i]);
					}

					lua_newtable(state);
					lua_pushlightuserdata(state, ctor);
					lua_pushcclosure(state, &FunctionBase::apply, 1);
					lua_setfield(state, -2, "__call");
					lua_setmetatable(state, -2);
				}

				//Set this in the current scope as the name of the class.
				lua_rawgeti(state, LUA_REGISTRYINDEX, scopeIndex);
				lua_pushvalue(state, -2);
				lua_setfield(state, -2, representation->name);

				return *containingScope;
			}
		private:
			lua_State * state;
			lbind::StackObject metatable;
			ClassRepresentation * representation;

			int scopeIndex;
			Scope * containingScope;

			std::vector<FunctionBase *> constructors;
		};
	}

	template<typename T>
	struct Convert<T, typename boost::enable_if<boost::is_pointer<T>>::type>
	{
		//TODO: Make sure that this is only a level 1 pointer, not 2 or more

		typedef typename Undecorate<T>::type Undecorated;

		//This is what is used to store this value in the argument tuple before pulling the real value from lua.
		typedef Undecorated type;
		typedef boost::false_type is_primitive;

		//Converts the value that we use to store the argument into the actual argument itself.
		//This will be passed to std::forward<>.
		static T&& forward(type t)
		{
			return std::forward<T>(t);
		}

		template<typename U>
		static U&& universal(type&& t)
		{
			return static_cast<U&&>(t);
		}

		//Converts a value at the given index. Must write to out, and return the number of stack objects consumed.
		// THIS IS A VERY SPECIFICALLY WRITTEN FROM METHOD. MAKE SURE THAT YOU UNDERSTAND THE TYPES HERE
		// NB: type is a T*
		static int from(lua_State * state, int index, type& out)
		{
			type* block = static_cast<type*>(lua_touserdata(state, index));
			out = static_cast<type>(Detail::ownershipless(*block));
			return 1;
		}

		//Converts a value to lua, and pushes it onto the stack.
		static int to(lua_State * state, Undecorated in)
		{
			//Push the userdata.
			Undecorated * val = static_cast<Undecorated *>(lua_newuserdata(state, sizeof(in)));
			*val = in;

			//Push the metatable
			lua_rawgeti(state, LUA_REGISTRYINDEX, lbind::Detail::Metatables<typename boost::remove_pointer<Undecorated>::type>::instanceMetatableIndex);

			//Set metatable and return.
			lua_setmetatable(state, -2);

			//Do we have a metatable?
			return 1;
		}
	};

	//This is the default converter, converting user-defined types.
	template<typename T>
	struct Convert<T, typename boost::disable_if<
		boost::mpl::or_<
			boost::is_pointer<T>,
			boost::is_integral<T>,
			boost::is_floating_point<T>
		>>::type>
	{
		typedef typename Undecorate<T>::type Undecorated;

		//This is what is used to store this value in the argument tuple before pulling the real value from lua.
		typedef Undecorated* type;
		typedef boost::false_type is_primitive;

		//Converts the value that we use to store the argument into the actual argument itself.
		//This will be passed to std::forward<>.
		static T& forward(type t)
		{
			return *t;
		}

		template<typename U>
		static U&& universal(type&& t)
		{
			return static_cast<U&&>(*t);
		}

		//Converts a value at the given index. Must write to out, and return the number of stack objects consumed.
		static int from(lua_State * state, int index, type& out)
		{
			type * block = static_cast<type *>(lua_touserdata(state, index));
			out = static_cast<type>(Detail::ownershipless(*block));
			return 1;
		}

		//Converts a value to lua, and pushes it onto the stack.
		static int to(lua_State * state, const Undecorated& in)
		{
			//Make a copy
			T * storage = new T(in);
			storage = static_cast<T *>(Detail::ownership(storage, Detail::Owned));
			return to(state, storage);
		}

		static int to(lua_State * state, Undecorated * in)
		{
			//Push the userdata.
			Undecorated ** val = static_cast<Undecorated **>(lua_newuserdata(state, sizeof(in)));
			*val = in;

			//Push the metatable
			lua_rawgeti(state, LUA_REGISTRYINDEX, lbind::Detail::Metatables<Undecorated>::instanceMetatableIndex);

			//Set metatable and return.
			lua_setmetatable(state, -2);

			//Do we have a metatable?
			return 1;
		}
	};


	template<typename T>
	Detail::ClassRegistrar<T> registerClass(lua_State * s, const char * name, int scopeIndex, Scope * scope)
	{
		lbind::StackCheck check(s, 0, 1);

		//Create a new table.
		lua_newtable(s);

		Detail::ClassRepresentation * rep = new Detail::ClassRepresentation();
		rep->instanceIndex = &Detail::Metatables<T>::instanceMetatableIndex;
		rep->staticIndex = &Detail::Metatables<T>::staticMetatableIndex;
		rep->name = name;

		Detail::Metatables<T>::name = name;
		return Detail::ClassRegistrar<T>(s, lbind::StackObject::fromStack(s, -1), rep, scopeIndex, scope);
	}
}
