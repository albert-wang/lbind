#pragma once
#include "lbind.hpp"
#include <boost/test/unit_test.hpp>

struct StateFixture
{
	inline StateFixture()
	{
		state = luaL_newstate();

		LBind::open(state);
		luaL_openlibs(state);
	}

	inline ~StateFixture()
	{
		lua_close(state);
	}

	lua_State * state;
};

inline const char * dostring(lua_State * state, const char * data)
{
	int e = luaL_dostring(state, data);
	if (e != 0)
	{
		const char * err = lua_tostring(state, -1);
		return err;
	}

	return nullptr;
}