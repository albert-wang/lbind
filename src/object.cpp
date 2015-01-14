#include "object.h"

Object::Object(lua_State * state, int index)
	:referencingState(state)
	,ind(index)
{}

Object::Object()
	:referencingState(nullptr)
	,ind(0)
{}

int Object::index() const
{
	return ind;
}

lua_State * Object::state() const
{
	return referencingState;
}