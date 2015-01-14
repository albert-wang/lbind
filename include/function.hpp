#pragma once

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

		struct FunctionBase
		{
			virtual int call(lua_State * state) = 0;

			static int apply(lua_State * l)
			{
				int ind = lua_upvalueindex(1);
				FunctionBase * b = static_cast<FunctionBase *>(lua_touserdata(l, ind));

				int result = b->call(l);
				if (result < 0)
				{
					//TODO: HANDLE ERROR
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

				if (argc != boost::function_types::function_arity<F>::value)
				{
					//Could not invoke properly, try another overload.
					return -1;
				}

				typename FunctionTraits<F>::lua_tuple args;
				//Pull arguments from lua
				PullFromLua pull(state);
				for_each(args, pull);

				if (pull.failed)
				{
					return -1;
				}

				//Now we have a value that we need to push back to lua
				auto val = TupleCall<F,
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

				if (argc != boost::function_types::function_arity<F>::value)
				{
					//Could not invoke properly, try another overload.
					return -1;
				}

				typename FunctionTraits<F>::lua_tuple args;
				//Pull arguments from lua
				PullFromLua pull(state);
				for_each(args, pull);

				if (pull.failed)
				{
					return -1;
				}

				TupleCall<F,
					boost::function_types::function_arity<F>::value,
					boost::function_types::is_member_function_pointer<F>::value
				>::apply(callable, std::move(args));

				return 0;
			}

			F callable;
		};

		template<typename F>
		FunctionBase * createFunction(F f)
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
	void registerFunction(lua_State * state, const char * name, F f)
	{
		using namespace Detail;
		Detail::FunctionBase * base = createFunction(f);

		lua_pushlightuserdata(state, base);
		lua_pushcclosure(state, &FunctionBase::apply, 1);
		lua_setglobal(state, name);
	}
}