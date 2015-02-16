#pragma once
#include <boost/fusion/include/at_c.hpp>
#include <boost/preprocessor/iteration/local.hpp>
#include <boost/optional.hpp>
#include "traits.hpp"
#include "convert.hpp"

#include <iostream>

namespace LBind
{
	template<typename F, typename Ret, typename Params, int arity, bool isMember>
	struct TupleCall
	{
	};

	template<typename F, typename Ret, typename Params>
	struct TupleCall<F, Ret, Params, 0, false>
	{
		static Ret apply(F f, boost::fusion::vector0<> args)
		{
			return f();
		}
	};

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#define FORWARD_ARGS(z, n, d) BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, d)) std::forward<typename boost::remove_reference<typename result_of::at_c<parameter_tuple, n>::type>::type>(Convert<typename boost::remove_cv<typename boost::remove_reference<typename result_of::at_c<parameter_tuple, n>::type>::type>::type>::forward(std::move(at_c<n>(args))))

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

#define CONVERT(z, n, d) \
	LBind::Convert<typename Undecorate<BOOST_PP_CAT(T, n)>::type>::to(o.state(), BOOST_PP_CAT(t, n));

#define BOOST_PP_LOCAL_MACRO(n) \
	template<BOOST_PP_ENUM_PARAMS(n, typename T)> 						\
	static R invoke(Object o, BOOST_PP_ENUM_BINARY_PARAMS(n, T, &&t))	\
	{																	\
		StackCheck check(o.state(), 0, 0);								\
		o.push();														\
		BOOST_PP_REPEAT(n, CONVERT, 0)									\
		Detail::pcallWrapper(o.state(), n, 0, 0);						\
	}

	template<typename R>
	struct LuaCall<R, true>
	{
		static R invoke(Object o)
		{
			StackCheck check(o.state(), 0, 0);
			o.push();
			Detail::pcallWrapper(o.state(), 0, 0, 0);
		}

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#include BOOST_PP_LOCAL_ITERATE()
	};

#define BOOST_PP_LOCAL_MACRO(n) \
	template<BOOST_PP_ENUM_PARAMS(n, typename T)> 						\
	static R invoke(Object o, BOOST_PP_ENUM_BINARY_PARAMS(n, T, &&t))	\
	{																	\
		StackCheck check(o.state(), 1, 0);								\
		o.push();														\
		BOOST_PP_REPEAT(n, CONVERT, 0)									\
		Detail::pcallWrapper(o.state(), n, 1, 0);						\
		R res;															\
		Convert<R>::from(o.state(), -1, res);							\
		return res;														\
	}

	template<typename R>
	struct LuaCall<R, false>
	{
		static R invoke(Object o)
		{
			StackCheck check(o.state(), 1, 0);
			o.push();
			Detail::pcallWrapper(o.state(), 0, 1, 0);
			R res;
			return Convert<R>::from(o.state(), -1, res);
		}

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#include BOOST_PP_LOCAL_ITERATE()
	};

#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename R BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, typename T)> \
	R call(Object o BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_BINARY_PARAMS(n, T, &&t)) { return LuaCall<R, boost::is_same<R, void>::value>::invoke(o BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, t)); }

#define BOOST_PP_LOCAL_LIMITS (0, 10)
#include BOOST_PP_LOCAL_ITERATE()

#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename R BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, typename T), typename Enable = typename boost::disable_if<boost::is_same<R, void>>::type> \
	boost::optional<R> conditionalCall(Object o BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_BINARY_PARAMS(n, T, &&t)) \
	{ 																	\
		if (o.type() != LUA_TFUNCTION) { return boost::none; }			\
		return LuaCall<R, boost::is_same<R, void>::value>::invoke(o BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, t)); \
	}

#define BOOST_PP_LOCAL_LIMITS (0, 10)
#include BOOST_PP_LOCAL_ITERATE()

#define BOOST_PP_LOCAL_MACRO(n) \
template<typename R BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, typename T), typename Enable = typename boost::enable_if<boost::is_same<R, void>>::type> \
void conditionalCall(Object o BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_BINARY_PARAMS(n, T, &&t)) \
{ 																	\
	if (o.type() != LUA_TFUNCTION) { return; }						\
	return LuaCall<R, boost::is_same<R, void>::value>::invoke(o BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, t)); \
}

#define BOOST_PP_LOCAL_LIMITS (0, 10)
#include BOOST_PP_LOCAL_ITERATE()

#define FORWARD(z, n, d) \
	std::forward<BOOST_PP_CAT(A, n)>(BOOST_PP_CAT(a, n))

	//Constructor
#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename T BOOST_PP_COMMA_IF(n) BOOST_PP_ENUM_PARAMS(n, typename A)> T * constructor(BOOST_PP_ENUM_BINARY_PARAMS(n, A, &&a)) { return new T(BOOST_PP_REPEAT(n, FORWARD, 0)); }

#undef BOOST_PP_LOCAL_MACRO
#undef BOOST_PP_LOCAL_LIMITS
#undef FORWARD_ARGS
#undef CONVERT
#undef FORWARD
}
