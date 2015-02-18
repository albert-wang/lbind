#pragma once

#include "lua.hpp"
#include <boost/cstdint.hpp>
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/mpl/or.hpp>
#include <cassert>

#include <iostream>
#include "exceptions.hpp"

namespace LBind
{
	template<typename T>
	struct Undecorate
	{
		typedef typename boost::remove_cv<typename boost::remove_reference<T>::type>::type type;
	};

	struct Ignored
	{
		Ignored()
		{
			throw BindingError("Ignored should never be constructed");
		}
	};

	template<typename T, typename Enable = void>
	struct Convert
	{};

	template<>
	struct Convert<Ignored, void>
	{
		typedef Ignored* type;
		typedef boost::true_type is_primitive;

		static Ignored *&& forward(type&& t)
		{
			return std::move(t);
		}

		static int from(lua_State * state, int index, type& out)
		{
			out = nullptr;
			return 1;
		}

		static int to(lua_State * state, const type& in)
		{
			//What? No.
			throw BadCast("Cannot convert an Ignored parameter into lua.");
		}
	};

	template<>
	struct Convert<lua_State, void>
	{
		typedef lua_State* type;
		typedef boost::false_type is_primitive;

		static lua_State *&& forward(type&& t)
		{
			return std::move(t);
		}

		static int from(lua_State * state, int index, type& out)
		{
			//Yeah ...
			out = state;

			//This doesn't consume any stack space.
			return 0;
		}

		static int to(lua_State * state, const type& in)
		{
			//This isn't a thing. Use a wrapping type instead.
			throw BadCast("Cannot convert a lua_State into another lua_State. Use a wrapping type instead.");
		}
	};

	template<>
	struct Convert<const char *, void>
	{
		typedef const char* type;
		typedef boost::true_type is_primitive;

		static const char *&& forward(type&& t)
		{
			return std::move(t);
		}

		static int from(lua_State * state, int index, type& out)
		{
			if (lua_type(state, index) != LUA_TSTRING)
			{
				return -1;
			}

			const char * res = lua_tostring(state, index);
			if (!res)
			{
				return -1;
			}

			out = res;
			return 1;
		}

		static int to(lua_State * state, const type& in)
		{
			lua_pushstring(state, in);
			return 1;
		}
	};

	template<>
	struct Convert<std::string, void>
	{
		typedef std::string type;
		typedef boost::true_type is_primitive;

		static std::string&& forward(type&& t)
		{
			return std::move(t);
		}

		static int from(lua_State * state, int index, type& out)
		{
			if (lua_type(state, index) != LUA_TSTRING)
			{
				return -1;
			}

			const char * res = lua_tostring(state, index);
			if (!res)
			{
				return -1;
			}

			out = res;
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
		typedef boost::true_type is_primitive;

		static boost::string_ref forward(type&& t)
		{
			return t;
		}

		static int from(lua_State * state, int index, type& out)
		{
			if (lua_type(state, index) != LUA_TSTRING)
			{
				return -1;
			}

			const char * res = lua_tostring(state, index);
			if (!res)
			{
				return -1;
			}

			out = res;
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
		typedef boost::true_type is_primitive;

		static int forward(type t)
		{
			return t;
		}

		static int from(lua_State * state, int index, type& out)
		{
			int success = 0;
			auto res = lua_tointegerx(state, index, &success);

#ifdef CHECK_INTEGER_OVERFLOW
			if (res > std::numeric_limits<T>::max())
			{
				throw BadCast("Integer overflow");
			}

			if (res < std::numeric_limits<T>::min())
			{
				throw BadCast("Integer underflow");
			}
#endif

			out = static_cast<T>(res);
			if (success == 0)
			{
				return -1;
			}

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
		typedef boost::true_type is_primitive;

		static T forward(type t)
		{
			return t;
		}

		static int from(lua_State * state, int index, type& out)
		{
			int success = 0;

			out = static_cast<T>(lua_tonumberx(state, index, &success));
			if (success == 0)
			{
				return -1;
			}

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

	//Char pointer lookup specialized.
	//Assumed to be c-string
	template<>
	struct Lookup<const char *>
	{
		typedef const char * type;
		typedef Convert<const char *> converter;
	};
}