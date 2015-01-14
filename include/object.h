#pragma once
#include "lua.hpp"

//TODO: THIS LEAKS REFERENCES.
class Object
{
public:
	Object(lua_State * state, int index);
	Object();

	int index() const;
	lua_State * state() const;
private:
	lua_State * referencingState;
	int ind;
};