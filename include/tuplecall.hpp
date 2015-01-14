#include <boost/fusion/include/at_c.hpp>
#include <boost/preprocessor/iteration/local.hpp>
#include "traits.hpp"
#include "convert.hpp"

namespace LBind
{
	template<typename F, int arity, bool isMember>
	struct TupleCall
	{
	};

	template<typename F>
	struct TupleCall<F, 0, false>
	{
		typedef FunctionTraits<F> Traits;

		static typename Traits::result_type apply(F f, boost::fusion::vector0<> args)
		{
			return f();
		}
	};

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#define FORWARD_ARGS(z, n, d) BOOST_PP_COMMA_IF(BOOST_PP_SUB(n, d)) std::forward<typename boost::remove_reference<typename result_of::at_c<parameter_tuple, n>::type>::type>(Convert<typename boost::remove_cv<typename boost::remove_reference<typename result_of::at_c<parameter_tuple, n>::type>::type>::type>::forward(std::move(at_c<n>(args))))

#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename F> 								\
	struct TupleCall<F, n, false> 						\
	{													\
		typedef FunctionTraits<F> Traits;				\
		typedef typename Traits::parameter_tuple parameter_tuple; \
		typedef typename Traits::lua_tuple lua_tuple;   \
		template<BOOST_PP_ENUM_PARAMS(n, typename T)>	\
		static typename Traits::result_type apply(F f, boost::fusion::BOOST_PP_CAT(vector, n)<BOOST_PP_ENUM_PARAMS(n, T)>&& args) \
		{												\
			using namespace boost::fusion;				\
			return f(BOOST_PP_REPEAT(n, FORWARD_ARGS, 0)); \
		}												\
	};													\

#include BOOST_PP_LOCAL_ITERATE()
#undef BOOST_PP_LOCAL_MACRO
#undef BOOST_PP_LOCAL_LIMITS

#define BOOST_PP_LOCAL_MACRO(n) \
	template<typename F> 								\
	struct TupleCall<F, n, true> 						\
	{													\
		typedef FunctionTraits<F> Traits;				\
		typedef typename Traits::parameter_tuple parameter_tuple; \
		typedef typename Traits::lua_tuple lua_tuple;   \
		template<BOOST_PP_ENUM_PARAMS(n, typename T)>	\
		static typename Traits::result_type apply(F f, boost::fusion::BOOST_PP_CAT(vector, n)<BOOST_PP_ENUM_PARAMS(n, T)>&& args) \
		{												\
			using namespace boost::fusion;				\
			return (at_c<0>(args)->*(f))(BOOST_PP_REPEAT_FROM_TO(1, n, FORWARD_ARGS, 1)); \
		}												\
	};													\

#define BOOST_PP_LOCAL_LIMITS (1, 10)
#include BOOST_PP_LOCAL_ITERATE()
#undef BOOST_PP_LOCAL_MACRO
#undef BOOST_PP_LOCAL_LIMITS
#undef FORWARD_ARGS
}
