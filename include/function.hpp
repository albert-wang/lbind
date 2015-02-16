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

namespace LBind
{
	namespace Detail
	{
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

		template<typename F, bool isVoid>
		struct Function
		{};

		template<typename F>
		struct Function<F, false> : FunctionBase
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
				auto val = TupleCall<F, typename FunctionTraits<F>::result_type, typename FunctionTraits<F>::parameter_types,
					boost::function_types::function_arity<F>::value,
					boost::function_types::is_member_function_pointer<F>::value
				>::apply(callable, std::move(args));

				typedef typename Detail::ConvertToLuaType::template apply<typename boost::function_types::result_type<F>::type>::raw RawType;
				return Convert<RawType>::to(state, val);
			}

			F callable;
		};

		template<typename F>
		struct Function<F, true> : FunctionBase
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

		template<typename F, typename Op, bool isVoid>
		struct FunctionObject
		{};

		template<typename F, typename Op>
		struct FunctionObject<F, Op, false> : FunctionBase
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
				auto val = TupleCall<F, typename FunctionTraits<Op>::result_type, parameter_tuple,
					boost::function_types::function_arity<Op>::value - 1,
					false
				>::apply(callable, std::move(args));

				typedef typename Detail::ConvertToLuaType::template apply<typename boost::function_types::result_type<Op>::type>::raw RawType;
				return Convert<RawType>::to(state, val);
			}

			F callable;
		};

		template<typename F, typename Op>
		struct FunctionObject<F, Op, true> : FunctionBase
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

		template<typename F, typename Op>
		FunctionBase * createFunctionObject(F f, Op op)
		{
			return new FunctionObject<F, Op, boost::is_same<
					void,
					typename boost::function_types::result_type<Op>::type
				>::value
			>(f);
		}

		//Function object overload
		template<typename F>
		FunctionBase * createFunction(F f, typename boost::function_types::result_type<decltype(&F::operator())>::type * = nullptr)
		{
			return createFunctionObject(f, &F::operator());
		}

		//Function overload
		template<typename F>
		FunctionBase * createFunction(F f, typename boost::function_types::result_type<F>::type * = nullptr)
		{
			return new Function<F,
				boost::is_same<
					void,
					typename boost::function_types::result_type<F>::type
				>::value
			>(f);
		}
	}

	template<typename F>
	void pushFunction(lua_State * state, const char * name, F f)
	{
		using namespace Detail;
		Detail::FunctionBase * base = createFunction(f);

		//Does this function already exist?
			//If so, is the function overloaded already?
				//If so, just add a new overload.
			//Alright, make an overloaded function and add the original function and the to-be-registered one in
		//Alright, just create a new one

		lua_pushlightuserdata(state, base);
		lua_pushcclosure(state, &FunctionBase::apply, 1);

		InternalState * internal = Detail::getInternalState(state);
		internal->registerFunction(base);
	}

	template<typename F>
	void registerFunction(lua_State * state, const char * name, F f)
	{
		pushFunction(state, name, f);
		lua_setglobal(state, name);
	}
}