#pragma once

#include <boost/function_types/result_type.hpp>
#include <boost/function_types/components.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/is_member_function_pointer.hpp>

#include <boost/mpl/transform.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/as_vector.hpp>

#include <boost/fusion/include/mpl.hpp>
#include <boost/preprocessor/iteration/local.hpp>

#include "convert.hpp"


namespace Detail
{
	template<typename Seq>
	struct ToFusionVector
	{
		typedef typename boost::fusion::result_of::as_vector<Seq>::type type;
	};

	struct ConvertToLuaType
	{
		template<typename T>
		struct apply
		{
			typedef typename Undecorate<T>::type raw;
			typedef typename Convert<raw>::type type;
		};
	};

	template<typename Seq>
	struct ToLuaTypeVector
	{
		typedef typename
			boost::fusion::result_of::as_vector<
				typename boost::mpl::transform<Seq, ConvertToLuaType>::type
			>::type type;
	};

	template<typename T>
	struct ToTuple
	{};

#define BOOST_PP_LOCAL_MACRO(n) \
	template<BOOST_PP_ENUM_PARAMS(n, typename T)> 											\
	struct ToTuple<boost::fusion::vector<BOOST_PP_ENUM_PARAMS(n, T)>> 						\
	{																						\
		typedef boost::fusion::BOOST_PP_CAT(vector, n)<BOOST_PP_ENUM_PARAMS(n, T)> type; 	\
	};

#define BOOST_PP_LOCAL_LIMITS (0, 10)
#include BOOST_PP_LOCAL_ITERATE()
}

template<typename F>
struct FunctionTraits
{
	typedef typename boost::function_types::result_type<F>::type result_type;
	typedef boost::function_types::parameter_types<F> parameter_types;

	typedef typename Detail::ToFusionVector<parameter_types>::type parameter_tuple;
	typedef typename Detail::ToLuaTypeVector<parameter_types>::type lua_tuple;
};
