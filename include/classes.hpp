#pragma once
#include "lua.hpp"
#include "object.h"
#include "convert.hpp"
#include "function.hpp"

namespace LBind
{
	namespace Detail
	{
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
				(static_cast<T *>(target)->*(pointer)) = Converter::forward(value);

				return consumed;
			}

			M pointer;
		};


		template<typename T>
		class ClassRegistrar
		{
			static int warn(lua_State *)
			{
				std::cout << "Newindex called!";
				return 0;
			}

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
				if (type != LUA_TLIGHTUSERDATA)
				{
					//Move the top element to the 3rd element
					lua_rotate(s, -1, 4);
					lua_pop(s, 3);

					return 1;
				}
				else
				{
					//A member access.
					MemberBase * member = static_cast<MemberBase *>(lua_touserdata(s, -1));
					void * target = *(void **)lua_touserdata(s, -4);

					member->push(s, target);

					//Stack is now [val, key, metatable, index-result, get]
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
					void * target = *(void **)lua_touserdata(s, -5);

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
			ClassRegistrar(lua_State * state, LBind::StackObject meta, ClassRepresentation * rep)
				:state(state)
				,metatable(meta)
				,representation(rep)
			{}

			template<typename T>
			ClassRegistrar& constant(T t, const char * name, typename boost::enable_if<Convert<T>::is_primitive>::type * v = nullptr)
			{
				assert(metatable.index() == lua_gettop(state));
				Convert<T>::to(state, t);
				lua_setfield(state, -2, name);
				assert(metatable.index() == lua_gettop(state));
			}

			template<typename F>
			ClassRegistrar& def(F f, const char * name)
			{
				assert(metatable.index() == lua_gettop(state));

				LBind::pushFunction(state, name, f);
				lua_setfield(state, -2, name);

				assert(metatable.index() == lua_gettop(state));
				return *this;
			}

			//This is a member function pointer.
			template<typename M>
			ClassRegistrar& def_readonly(M m, const char * name)
			{
				assert(metatable.index() == lua_gettop(state));

				MemberBase * member = new ReadonlyMember<T, M>(m);
				lua_pushlightuserdata(state, member);
				lua_setfield(state, -2, name);

				assert(metatable.index() == lua_gettop(state));
				return *this;
			}

			//This is a member function pointer.
			template<typename M>
			ClassRegistrar& def_readwrite(M m, const char * name)
			{
				assert(metatable.index() == lua_gettop(state));

				MemberBase * member = new ReadWriteMember<T, M>(m);
				lua_pushlightuserdata(state, member);
				lua_setfield(state, -2, name);

				assert(metatable.index() == lua_gettop(state));
				return *this;
			}

			void finish()
			{
				using namespace LBind::Detail;
				assert(metatable.index() == lua_gettop(state));

				Metatables<T>::instanceMetatableIndex = luaL_ref(state, LUA_REGISTRYINDEX);
				lua_rawgeti(state, LUA_REGISTRYINDEX, Metatables<T>::instanceMetatableIndex);

				lua_pushvalue(state, -1);
				lua_pushlightuserdata(state, representation);
				lua_pushcclosure(state, &ClassRegistrar<T>::index, 1);
				lua_setfield(state, -2, "__index");

				lua_pushlightuserdata(state, representation);
				lua_pushcclosure(state, &ClassRegistrar<T>::newindex, 1);
				lua_setfield(state, -2, "__newindex");

				//Set this in the current scope as the name of the class.
				lua_setglobal(state, representation->name);
			}
		private:
			lua_State * state;
			LBind::StackObject metatable;
			ClassRepresentation * representation;
		};
	}

	template<typename T>
	struct Convert<T, typename boost::enable_if<boost::is_pointer<T>>::type>
	{
		typedef typename Undecorate<T>::type Undecorated;

		//This is what is used to store this value in the argument tuple before pulling the real value from lua.
		typedef Undecorated type;
		typedef boost::mpl::false_type is_primitive;

		//Converts the value that we use to store the argument into the actual argument itself.
		//This will be passed to std::forward<>.
		static T&& forward(type t)
		{
			return std::forward<T>(t);
		}

		//Converts a value at the given index. Must write to out, and return the number of stack objects consumed.
		static int from(lua_State * state, int index, type& out)
		{
			type * block = static_cast<type *>(lua_touserdata(state, index));
			out = block;
			return 1;
		}

		//Converts a value to lua, and pushes it onto the stack.
		static int to(lua_State * state, Undecorated in)
		{
			//Push the userdata.
			Undecorated * val = static_cast<Undecorated *>(lua_newuserdata(state, sizeof(in)));
			*val = in;

			//Push the metatable
			lua_rawgeti(state, LUA_REGISTRYINDEX, LBind::Detail::Metatables<typename boost::remove_pointer<Undecorated>::type>::instanceMetatableIndex);

			//Set metatable and return.
			lua_setmetatable(state, -2);

			//Do we have a metatable?
			return 0;
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
		typedef boost::mpl::false_type is_primitive;

		//Converts the value that we use to store the argument into the actual argument itself.
		//This will be passed to std::forward<>.
		static T&& forward(type t)
		{
			return std::forward<T>(*t);
		}

		//Converts a value at the given index. Must write to out, and return the number of stack objects consumed.
		static int from(lua_State * state, int index, type& out)
		{
			type * block = static_cast<type *>(lua_touserdata(state, index));
			out = *block;
			return 1;
		}

		//Converts a value to lua, and pushes it onto the stack.
		static int to(lua_State * state, const Undecorated& in)
		{
			//Make a copy
			T * storage = new T(in);
			return to(state, storage);
		}

		static int to(lua_State * state, Undecorated * in)
		{
			//Push the userdata.
			Undecorated ** val = static_cast<Undecorated **>(lua_newuserdata(state, sizeof(in)));
			*val = in;

			//Push the metatable
			lua_rawgeti(state, LUA_REGISTRYINDEX, LBind::Detail::Metatables<Undecorated>::instanceMetatableIndex);

			//Set metatable and return.
			lua_setmetatable(state, -2);

			//Do we have a metatable?
			return 0;
		}
	};


	template<typename T>
	Detail::ClassRegistrar<T> registerClass(lua_State * s, const char * name)
	{
		LBind::StackCheck check(s, 0, 1);

		//Create a new table.
		lua_newtable(s);

		Detail::ClassRepresentation * rep = new Detail::ClassRepresentation();
		rep->instanceIndex = &Detail::Metatables<T>::instanceMetatableIndex;
		rep->staticIndex = &Detail::Metatables<T>::staticMetatableIndex;
		rep->name = name;

		Detail::Metatables<T>::name = name;
		return Detail::ClassRegistrar<T>(s, LBind::StackObject::fromStack(s, -1), rep);
	}
}