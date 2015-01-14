#pragma once

#include "lua.hpp"
#include <boost/cstdint.hpp>
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <cassert>

#include "object.h"

template<typename T>
struct Undecorate
{
	typedef typename boost::remove_cv<typename boost::remove_reference<T>::type>::type type;
};

template<typename T, typename Enable = void>
struct Convert
{
	//This is what is used to store this value in lua.
	typedef typename Undecorate<T>::type Undecorated;
	typedef Undecorated* type;

	static T&& forward(type t)
	{
		return std::forward<T>(*t);
	}

	static int from(lua_State * state, int index, type& out)
	{
		out = new T(1);
		return 1;
	}

	static int to(lua_State * state, const Undecorated& in)
	{
		//Make a copy
		T * storage = new T(in);
		return to(state, storage);
	}

	static int to(lua_State * state, Undecorated * in)
	{
		//Not taking ownership, just persist the pointer.

		//Do we have a metatable?
		return 0;
	}
};

template<>
struct Convert<Object, void>
{
	typedef Object type;

	static Object&& forward(type&& t)
	{
		return std::move(t);
	}

	static int from(lua_State * state, int index, type& out)
	{
		lua_pushvalue(state, index);
		int ref = luaL_ref(state, LUA_REGISTRYINDEX);
		out = Object(state, ref);
		return 1;
	}

	static int to(lua_State * state, const type& in)
	{
		assert(state = in.state());
		lua_rawgeti(state, LUA_REGISTRYINDEX, in.index());
		return 1;
	}
};

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
		lua_pushstring(state, in.c_str());
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

template<typename T>
T object_cast(const Object& o)
{
	T result;

	Convert<Object>::to(o.state(), o);
	Convert<T>::from(o.state(), -1, result);
	lua_pop(o.state(), 1);

	return std::move(result);
}