#pragma once
#include <boost/fusion/include/at_c.hpp>
#include <boost/preprocessor/iteration/local.hpp>
#include <boost/optional.hpp>
#include "traits.hpp"
#include "convert.hpp"

#include <iostream>

namespace lbind
{
	template<typename F, typename Ret, typename Params, int arity, bool isMember>
	struct TupleCall
	{};

	template<typename F, typename Ret, typename Params>
	struct TupleCall<F, Ret, Params, 0, false>
	{
		static Ret apply(F f, boost::fusion::vector0<> args)
		{
			return f();
		}
	};

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#define FORWARD_ARGS(z, n, d) BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, d)) \
	Convert<typename boost::remove_cv<typename boost::remove_reference<typename result_of::at_c<parameter_tuple, n>::type>::type>::type>::template universal<typename result_of::at_c<parameter_tuple, n>::type>(std::move(at_c<n>(args)))

#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename F, typename Ret, typename Params>	\
	struct TupleCall<F, Ret, Params, n, false>			\
	{													\
		typedef Params parameter_tuple;					\
		template<BOOST_PP_ENUM_PARAMS(n, typename T)>	\
		static Ret apply(F f, boost::fusion::BOOST_PP_CAT(vector, n)<BOOST_PP_ENUM_PARAMS(n, T)>&& args) \
		{												\
			using namespace boost::fusion;				\
			return f(BOOST_PP_REPEAT(n, FORWARD_ARGS, 0)); \
		}												\
	};													\

#include BOOST_PP_LOCAL_ITERATE()
#undef BOOST_PP_LOCAL_MACRO
#undef BOOST_PP_LOCAL_LIMITS

#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename F, typename Ret, typename Params>	\
	struct TupleCall<F, Ret, Params, n, true>			\
	{													\
		typedef Params parameter_tuple;					\
		template<BOOST_PP_ENUM_PARAMS(n, typename T)>	\
		static Ret apply(F f, boost::fusion::BOOST_PP_CAT(vector, n)<BOOST_PP_ENUM_PARAMS(n, T)>&& args) \
		{												\
			using namespace boost::fusion;				\
			return (at_c<0>(args)->*(f))(BOOST_PP_REPEAT_FROM_TO(1, n, FORWARD_ARGS, 1)); \
		}												\
	};													\

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#include BOOST_PP_LOCAL_ITERATE()

	//Also define the C++ -> lua call interface
	namespace Detail
	{
		inline int pcallWrapper(lua_State * s, int nargs, int nresult, int msg)
		{
			int res = lua_pcall(s, nargs, nresult, msg);
			if (res == 0)
			{
				return 0;
			}

			const char * err = lua_tostring(s, -1);
			std::cout << err << "\n";
			return res;
		}
	}

	//These return 0 values.
	template<typename Ret, bool isVoid>
	struct LuaCall
	{};

	template<typename R>
	struct LuaCall<R, true>
	{
		template<typename ...Args>
		static R invoke(Object o, Args&&... a)
		{
			StackCheck check(o.state(), 1, 0);
			o.push();

			(void)std::initializer_list<int>{(lbind::Convert<typename Undecorate<Args>::type>::to(o.state(), a), 1)...};

			Detail::pcallWrapper(o.state(), sizeof...(Args), 1, 0);
		}
	};

	template<typename R>
	struct LuaCall<R, false>
	{
		template <typename... Args>
		static R invoke(Object o, Args &&... a)
		{
			StackCheck check(o.state(), 1, 0);
			o.push();

			(void)std::initializer_list<int>{(lbind::Convert<typename Undecorate<Args>::type>::to(o.state(), a), 1)...};

			Detail::pcallWrapper(o.state(), sizeof...(Args), 1, 0);
			R res;
			Convert<R>::from(o.state(), -1, res);
			return res;
		}
	};

	template<typename R, typename ... T>
	R call(Object o, T&&... args)
	{
		return LuaCall<R, boost::is_same<R, void>::value>::invoke(o, std::move(args)...);
	}

	template <typename R, typename Enable = typename boost::disable_if<boost::is_same<R, void>>::type, typename... T>
	boost::optional<R> conditionalCall(Object o, T &&... args)
	{
		if (o.type() != LUA_TFUNCTION)
		{
			return boost::none;
		}

		return LuaCall<R, boost::is_same<R, void>::value>::invoke(o, std::move(args)...);
	}

	template <typename R, typename Enable = typename boost::enable_if<boost::is_same<R, void>>::type, typename... T>
	void conditionalCall(Object o, T &&... args)
	{
		if (o.type() != LUA_TFUNCTION)
		{
			return;
		}

		return LuaCall<R, boost::is_same<R, void>::value>::invoke(o, std::move(args)...);
	}

	template<typename R, typename... T>
	R * constructor(T&&... args)
	{
		return new R(std::forward<T>(args)...);
	}

#undef BOOST_PP_LOCAL_MACRO
#undef BOOST_PP_LOCAL_LIMITS
#undef FORWARD_ARGS
#undef CONVERT
#undef FORWARD
}
