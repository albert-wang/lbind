#include "binddsl.hpp"
#include "stackcheck.hpp"
#include "exceptions.hpp"

#include <cassert>
#include <iostream>

namespace LBind
{
	Scope module(lua_State * s)
	{
		return Scope(s, LUA_RIDX_GLOBALS, nullptr, "");
	}

	Scope::Scope(lua_State * state, int index, Scope * parent, const std::string& n)
		:parent(parent)
		,name(n)
		,state(state)
		,index(index)
	{}

	Scope Scope::scope(boost::string_ref ns)
	{
		StackCheck check(state, 2, 0);

		//[table]
		int rawt = lua_rawgeti(state, LUA_REGISTRYINDEX, index);
		if (rawt != LUA_TTABLE)
		{
			throw BindingError("Containing scope was not a table?");
		}

		//[table, field <maybe table>]
		int t = lua_getfield(state, -1, ns.data());
		if (t == LUA_TNIL)
		{
			lua_newtable(state);

			//Namespaces have a few reserved valiues,
			// __lbind_namespace is the name of the namespace
			// __lbind_ref is the registry reference that this namespace has.

			lua_pushstring(state, ns.data());
			lua_setfield(state, -2, "__lbind_namespace");

			int ref = luaL_ref(state, LUA_REGISTRYINDEX);

			lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
			lua_pushinteger(state, ref);
			lua_setfield(state, -2, "__lbind_ref");

			lua_pop(state, 1);
			return Scope(state, ref, this, ns.data());
		}
		else if (t == LUA_TTABLE)
		{
			//Maybe the scope already exists?
			lua_getfield(state, -1, "__lbind_namespace");
			const char * name = lua_tostring(state, -1);
			lua_pop(state, 1);

			if (ns != name)
			{
				//Not the expected namespace!
				throw BindingError("Scope was a table in containing scope, but was not the expected namespace.");
			}
			else
			{
				lua_getfield(state, -1, "__lbind_ref");
				int ref = lua_tointeger(state, -1);
				lua_pop(state, 1);

				if (ref == LUA_NOREF)
				{
					ref = luaL_ref(state, LUA_REGISTRYINDEX);

					lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
					lua_pushinteger(state, ref);

					lua_setfield(state, -2, "__lbind_ref");
				}

				return Scope(state, ref, this, ns.data());
			}
		}
		else
		{
			//Not something that we expected.
			throw BindingError("Scope was an object in containing scope, but not a table.");
		}
	}

	Scope& Scope::endscope()
	{
		assert(parent);
		StackCheck check(state, 1, 0);

		lua_rawgeti(state, LUA_REGISTRYINDEX, parent->index);
		lua_rawgeti(state, LUA_REGISTRYINDEX, index);

		//Unset the __lbind_ref value in the current scope
		lua_pushinteger(state, LUA_NOREF);
		lua_setfield(state, -2, "__lbind_ref");

		//Set the namespace into the containing scope
		lua_setfield(state, -2, name.c_str());

		luaL_unref(state, LUA_REGISTRYINDEX, index);
		return *parent;
	}

	void Scope::end()
	{
		assert(!parent);
	}
}