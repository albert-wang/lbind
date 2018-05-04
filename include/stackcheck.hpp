#pragma once
#include "lua.hpp"

namespace lbind
{
	//This ensures the state of the stack.
	struct StackCheck
	{
		StackCheck(lua_State * s, int popped, int netdiff);
		~StackCheck();

		lua_State * s;
		int consumed;
		int provided;
		int previousAmount;
	};

	void stackdump(lua_State *L);
}
