#include "init.hpp"

namespace LBind
{
	void open(lua_State * state)
	{
		//Allocate our storage object, store it in the extra space in lua_State
	}

	void close(lua_State * state)
	{
		//Deallocate our storage object.
	}
}