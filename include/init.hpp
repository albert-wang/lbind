#pragma once
#include "lua.hpp"

namespace LBind
{
	void open(lua_State *);

	void close(lua_State *);
}