#include "init.hpp"
#include "internal.hpp"

namespace LBind
{
	void open(lua_State * state)
	{
		//Allocate our storage object, store it in the extra space in lua_State
		Detail::InternalState * s = new Detail::InternalState();
		*reinterpret_cast<Detail::InternalState **>(lua_getextraspace(state)) = s;
	}

	void close(lua_State * state)
	{
		//Deallocate our storage object.
		Detail::InternalState * s = *reinterpret_cast<Detail::InternalState **>(lua_getextraspace(state));
		delete s;
	}
}