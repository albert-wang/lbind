#include "stackcheck.hpp"

#include <iostream>
#include <cassert>
#include "lua.hpp"

namespace lbind
{
	StackCheck::StackCheck(lua_State * l, int consumed, int provided)
		:s(l)
		,consumed(consumed)
		,provided(provided)
		,previousAmount(0)
	{
		previousAmount = lua_gettop(l);
	}

	StackCheck::~StackCheck()
	{
		if (consumed != 0)
		{
			lua_pop(s, consumed);
		}

		int now = lua_gettop(s);
		if (now != (previousAmount + provided))
		{
			std::cout << "Stack error: started at " << previousAmount << ", popped: " << consumed << " and expected: " << previousAmount + provided << ", had " << now << "\n";
			assert(now == (previousAmount + provided));
		}
	}

	void stackdump(lua_State *L)
	{
    	int i;
    	int top = lua_gettop(L);
    	for (i = 1; i <= top; i++)
    	{
    		/* repeat for each level */
        	int t = lua_type(L, i);
        	switch (t)
        	{
	        	case LUA_TSTRING:  /* strings */
		            printf("`%s'", lua_tostring(L, i));
		            break;

          		case LUA_TBOOLEAN:  /* booleans */
		            printf(lua_toboolean(L, i) ? "true" : "false");
		            break;

	            case LUA_TNUMBER:  /* numbers */
		            printf("%g", lua_tonumber(L, i));
		            break;

	          	default:  /* other values */
		            printf("%s", lua_typename(L, t));
		            break;
        	}
        	printf("  ");  /* put a separator */
      	}
      	printf("\n");  /* end the listing */
    }
}