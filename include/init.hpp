#pragma once
#include "lua.hpp"

namespace lbind
{
	void open(lua_State *);

	void close(lua_State *);
}