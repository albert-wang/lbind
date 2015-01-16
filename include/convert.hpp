#pragma once

#include "lua.hpp"
#include <boost/cstdint.hpp>
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/mpl/or.hpp>
#include <cassert>

#include <iostream>

namespace LBind
{
	template<typename T>
	struct Undecorate
	{
		typedef typename boost::remove_cv<typename boost::remove_reference<T>::type>::type type;
	};

	template<typename T, typename Enable = void>
	struct Convert
	{};

	template<>
	struct Convert<std::string, void>
	{
		typedef std::string type;

		static std::string&& forward(type&& t)
		{
			return std::move(t);
		}

		static int from(lua_State * state, int index, type& out)
		{
			out = lua_tostring(state, index);
			return 1;
		}

		static int to(lua_State * state, const type& in)
		{
			lua_pushlstring(state, in.c_str(), in.size());
			return 1;
		}
	};

	template<>
	struct Convert<boost::string_ref, void>
	{
		typedef std::string type;

		static boost::string_ref forward(type&& t)
		{
			return t;
		}

		static int from(lua_State * state, int index, type& out)
		{
			out = lua_tostring(state, index);
			return 1;
		}

		static int to(lua_State * state, boost::string_ref in)
		{
			lua_pushlstring(state, in.data(), in.size());
			return 1;
		}
	};

	template<typename T>
	struct Convert<T, typename boost::enable_if<boost::is_integral<T>>::type>
	{
		typedef int type;

		static int forward(type t)
		{
			return t;
		}

		static int from(lua_State * state, int index, type& out)
		{
			int success = 0;

			out = static_cast<T>(lua_tointegerx(state, index, &success));
			assert(success == 1);

			return 1;
		}

		static int to(lua_State * state, const type value)
		{
			lua_pushinteger(state, static_cast<boost::int64_t>(value));
			return 1;
		}
	};

	template<typename T>
	struct Convert<T, typename boost::enable_if<boost::is_floating_point<T>>::type>
	{
		typedef T type;

		static T forward(type t)
		{
			return t;
		}

		static int from(lua_State * state, int index, type& out)
		{
			int success = 0;

			out = static_cast<T>(lua_tonumberx(state, index, &success));
			assert(success == 1);

			return 1;
		}

		static int to(lua_State * state, const type out)
		{
			lua_pushnumber(state, static_cast<double>(out));
			return 1;
		}
	};

	template<typename T>
	struct Lookup
	{
		typedef typename boost::remove_pointer<T>::type type;
		typedef Convert<type> converter;
	};
}