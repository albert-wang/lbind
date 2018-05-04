#pragma once
#include <boost/function_types/result_type.hpp>
#include <boost/function_types/components.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/is_member_function_pointer.hpp>

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/next_prior.hpp>

#include <boost/utility/string_ref.hpp>

#include <boost/fusion/include/generation.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <boost/fusion/include/pop_front.hpp>

#include <vector>

#include "object.h"
#include "stackcheck.hpp"
#include "tuplecall.hpp"
#include "internal.hpp"
#include "policies.hpp"

namespace lbind
{
	namespace Detail
	{
		template<typename Seq, typename T>
		struct contains : boost::mpl::not_<
			boost::is_same<
				typename boost::fusion::result_of::find<Seq, T>::type,
				typename boost::fusion::result_of::end<Seq>::type
			>
		>
		{};

		struct PullFromLua
		{
			PullFromLua(lua_State * state)
				:state(state)
				,index(1)
				,failed(false)
			{}

			template<typename T>
			void operator()(T& out) const
			{
				if (failed)
				{
					return;
				}

				int res = Lookup<T>::converter::from(state, index, out);
				if (res < 0)
				{
					//Failure to convert.
					failed = true;
				}

				index += res;
			}

			lua_State * state;
			mutable int index;
			mutable bool failed;
		};

		struct OverloadedFunction;
		struct FunctionBase
		{
			virtual ~FunctionBase()
			{}

			virtual int call(lua_State * state) = 0;
			virtual OverloadedFunction * toOverloaded()
			{
				return nullptr;
			}

			static int apply(lua_State * l)
			{
				int ind = lua_upvalueindex(1);
				FunctionBase * b = static_cast<FunctionBase *>(lua_touserdata(l, ind));

				int result = b->call(l);
				if (result < 0)
				{
					int s = lua_gettop(l);

					std::string msg = "No valid overload found for arguments of type: (";
					for (int i = 1; i <= s; ++i)
					{
						if (i - 1)
						{
							msg += ", ";
						}

						msg += lua_typename(l, lua_type(l, i));
					}

					msg += ")";

					lua_pushstring(l, msg.c_str());
					lua_error(l);
				}

				return result;
			}
		};

		template<typename F, bool isVoid, typename Policies>
		struct Function
		{};

		template<typename F, typename Policies>
		struct Function<F, false, Policies> : FunctionBase
		{
			Function(F f)
				:callable(f)
			{}

			int call(lua_State * state)
			{
				using namespace boost::fusion;
				int argc = lua_gettop(state);

				typename FunctionTraits<F>::lua_tuple args;

				//Pull arguments from lua
				PullFromLua pull(state);
				for_each(args, pull);

				if (pull.failed)
				{
					return -1;
				}

				//Now we have a value that we need to push back to lua
				auto&& val = TupleCall<F, typename FunctionTraits<F>::result_type, typename FunctionTraits<F>::parameter_types,
					boost::function_types::function_arity<F>::value,
					boost::function_types::is_member_function_pointer<F>::value
				>::apply(callable, std::move(args));

				if (contains<Policies, returns_self_t>::type::value)
				{
					//That is, this returns the first argument.
					lua_pushvalue(state, 1);
					return 1;
				}
				else if (contains<Policies, ignore_return_t>::type::value)
				{
					return 0;
				}
				else
				{
					typedef typename Detail::ConvertToLuaType::template apply<typename boost::function_types::result_type<F>::type>::raw RawType;
					return Convert<RawType>::to(state, val);
				}
			}

			F callable;
		};

		template<typename F, typename Policies>
		struct Function<F, true, Policies> : FunctionBase
		{
			Function(F f)
				:callable(f)
			{}

			int call(lua_State * state)
			{
				using namespace boost::fusion;
				int argc = lua_gettop(state);

				typename FunctionTraits<F>::lua_tuple args;

				//Pull arguments from lua
				PullFromLua pull(state);
				for_each(args, pull);

				if (pull.failed)
				{
					return -1;
				}

				TupleCall<F, typename FunctionTraits<F>::result_type, typename FunctionTraits<F>::parameter_types,
					boost::function_types::function_arity<F>::value,
					boost::function_types::is_member_function_pointer<F>::value
				>::apply(callable, std::move(args));

				return 0;
			}

			F callable;
		};

		template<typename F, typename Op, bool isVoid, typename Policies>
		struct FunctionObject
		{};

		template<typename F, typename Op, typename Policies>
		struct FunctionObject<F, Op, false, Policies> : FunctionBase
		{
			FunctionObject(F f)
				:callable(f)
			{}

			int call(lua_State * state)
			{
				using namespace boost::fusion;
				int argc = lua_gettop(state);

				typename ToFusionVector<
					typename boost::fusion::result_of::pop_front<typename FunctionTraits<Op>::lua_tuple>::type
				>::type args;

				typedef typename ToFusionVector<
					typename boost::fusion::result_of::pop_front<typename FunctionTraits<Op>::parameter_tuple>::type
				>::type parameter_tuple;

				//Pull arguments from lua
				PullFromLua pull(state);
				for_each(args, pull);

				if (pull.failed)
				{
					return -1;
				}

				//Now we have a value that we need to push back to lua
				auto&& val = TupleCall<F, typename FunctionTraits<Op>::result_type, parameter_tuple,
					boost::function_types::function_arity<Op>::value - 1,
					false
				>::apply(callable, std::move(args));

				typedef typename Detail::ConvertToLuaType::template apply<typename boost::function_types::result_type<Op>::type>::raw RawType;
				return Convert<RawType>::to(state, val);
			}

			F callable;
		};

		template<typename F, typename Op, typename Policies>
		struct FunctionObject<F, Op, true, Policies> : FunctionBase
		{
			FunctionObject(F f)
				:callable(f)
			{}

			int call(lua_State * state)
			{
				using namespace boost::fusion;
				int argc = lua_gettop(state);

				typename ToFusionVector<
					typename boost::fusion::result_of::pop_front<typename FunctionTraits<Op>::lua_tuple>::type
				>::type args;

				typedef typename ToFusionVector<
					typename boost::fusion::result_of::pop_front<typename FunctionTraits<Op>::parameter_tuple>::type
				>::type parameter_tuple;

				//Pull arguments from lua
				PullFromLua pull(state);
				for_each(args, pull);

				if (pull.failed)
				{
					return -1;
				}

				TupleCall<F, typename FunctionTraits<Op>::result_type, parameter_tuple,
					boost::function_types::function_arity<Op>::value - 1,
					false
				>::apply(callable, std::move(args));

				return 0;
			}

			F callable;
		};

		struct OverloadedFunction : public FunctionBase
		{
			OverloadedFunction * toOverloaded()
			{
				return this;
			}

			int call(lua_State * state)
			{
				for (size_t i = 0; i < canidates.size(); ++i)
				{
					int res = canidates[i]->call(state);
					if (res >= 0)
					{
						return res;
					}
				}

				//No valid overloads found!
				return -1;
			}

			std::vector<FunctionBase *> canidates;
		};

		template<typename F, typename Op, typename P>
		FunctionBase * createFunctionObject(F f, Op op, P p)
		{
			return new FunctionObject<F, Op, boost::is_same<
					void,
					typename boost::function_types::result_type<Op>::type
				>::value,
				P
			>(f);
		}

		//Function object overload
		template<typename F, typename P>
		FunctionBase * createFunction(F f, P p, typename boost::function_types::result_type<decltype(&F::operator())>::type * = nullptr)
		{
			return createFunctionObject(f, &F::operator(), p);
		}

		//Function overload
		template<typename F, typename P>
		FunctionBase * createFunction(F f, P p, typename boost::remove_reference<typename boost::function_types::result_type<F>::type>::type * = nullptr)
		{
			return new Function<F,
				boost::is_same<
					void,
					typename boost::function_types::result_type<F>::type
				>::value,
				P
			>(f);
		}
	}

	template<typename F, typename P>
	Detail::FunctionBase * pushFunction(lua_State * state, const char * name, F f, P p)
	{
		using namespace Detail;
		Detail::FunctionBase * base = createFunction(f, p);

		//Does this function already exist?
			//If so, is the function overloaded already?
				//If so, just add a new overload.
			//Alright, make an overloaded function and add the original function and the to-be-registered one in
		//Alright, just create a new one

		lua_pushlightuserdata(state, base);
		lua_pushcclosure(state, &FunctionBase::apply, 1);

		InternalState * internal = Detail::getInternalState(state);
		internal->registerFunction(base);

		return base;
	}

	//Assumes that a table is on top of the stack.
	template<typename F, typename P>
	void resolveFunctionOverloads(lua_State * state, const char * name, F f, P p)
	{
		int type = lua_getfield(state, -1, name);
		if (type == LUA_TFUNCTION)
		{
			//Assume that its one of ours.
			const char * upvalueName = lua_getupvalue(state, -1, 1);

			//Stack is [function, upvalue?]
			if (!upvalueName || upvalueName[0] != '\0')
			{
				//Unknown upvalue.

				//This function already exists, do we really want to overwrite?
				if (upvalueName)
				{
					std::cout << "Upvalue found, but had name " << upvalueName << "\n";
				}
				else
				{
					std::cout << "Upvalue not found\n";
				}
			}
			else
			{
				//This may be an overloaded function.
				Detail::InternalState * internal = Detail::getInternalState(state);
				Detail::FunctionBase * base = static_cast<Detail::FunctionBase *>(lua_touserdata(state, -1));
				Detail::OverloadedFunction * overloaded = base->toOverloaded();

				if (!overloaded)
				{
					overloaded = new Detail::OverloadedFunction();
					overloaded->canidates.push_back(base);

					internal->registerFunction(overloaded);

					lua_pushlightuserdata(state, overloaded);

					//Stack is [function, old_upvalue, new_upvalue]
					lua_setupvalue(state, -3, 1);
					//Stack is [function, old_upvalue]
				}

				Detail::FunctionBase * newFunction = Detail::createFunction(f, p);
				internal->registerFunction(newFunction);

				overloaded->canidates.push_back(newFunction);
			}

			//Pop function and the table
			lua_pop(state, 2);
		}
		else
		{
			lua_pop(state, 1);

			pushFunction(state, name, f, p);
			lua_setfield(state, -2, name);
		}
	}

	//Assumes the table index is a registry index.
	template<typename F>
	void registerFunction(lua_State * state, int tableIndex, const char * name, F f)
	{
		boost::fusion::vector<null_policy_t> np;


		return registerFunction(state, tableIndex, name, f, np);
	}

	template<typename F, typename P>
	void registerFunction(lua_State * state, int tableIndex, const char * name, F f, P p)
	{
		boost::fusion::vector<P> policies(p);

		StackCheck check(state, 1, 0);

		lua_rawgeti(state, LUA_REGISTRYINDEX, tableIndex);
		resolveFunctionOverloads(state, name, f, policies);
	}
}