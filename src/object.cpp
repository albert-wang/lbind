#include "object.h"

namespace LBind
{
	Object::Object(lua_State * state, int index)
		:interpreter(state)
		,ind(index)
	{}

	Object::Object()
		:interpreter(nullptr)
		,ind(0)
	{}

	int Object::index() const
	{
		return ind;
	}

	int Object::type() const
	{
		StackCheck check(interpreter, 1, 0);
		lua_rawgeti(interpreter, LUA_REGISTRYINDEX, ind);
		return lua_type(interpreter, -1);
	}

	lua_State * Object::state() const
	{
		return interpreter;
	}

	//Pushes the object at index i onto the stack.
	int Object::stackPush(int i) const
	{
		lua_rawgeti(interpreter, LUA_REGISTRYINDEX, ind);
		return lua_geti(interpreter, -1, i);
	}

	int Object::stackPush(boost::string_ref i) const
	{
		assert(i[i.size()] == 0);
		lua_rawgeti(interpreter, LUA_REGISTRYINDEX, ind);

		return lua_getfield(interpreter, -1, i.data());
	}

	Detail::ObjectProxy<int, Object> Object::operator[](int i)
	{
		return Detail::ObjectProxy<int, Object>(this, i);
	}

	Detail::ObjectProxy<boost::string_ref, Object> Object::operator[](boost::string_ref i)
	{
		return Detail::ObjectProxy<boost::string_ref, Object>(this, i);
	}

	StackObject Object::push()
	{
		lua_rawgeti(interpreter, LUA_REGISTRYINDEX, ind);
		return StackObject(interpreter, -1);
	}

	void Object::pop(StackObject&)
	{
		lua_pop(interpreter, 1);
	}

	Detail::Iterator Object::begin()
	{
		return Detail::Iterator(this);
	}

	Detail::Iterator Object::end()
	{
		return Detail::Iterator();
	}

	StackObject StackObject::fromStack(lua_State * s, int i)
	{
		return StackObject(s, i);
	}

	StackObject::StackObject(lua_State * state, int ind)
		:interpreter(state)
		,canonical(lua_absindex(state, ind))
	{}

	//Pushes the object at index i onto the stack.
	int StackObject::stackPush(int i) const
	{
		return lua_geti(interpreter, canonical, i);
	}

	int StackObject::stackPush(boost::string_ref i) const
	{
		assert(i[i.size()] == 0);
		return lua_getfield(interpreter, canonical, i.data());
	}

	Detail::ObjectProxy<int, StackObject> StackObject::operator[](int i)
	{
		return Detail::ObjectProxy<int, StackObject>(this, i);
	}

	Detail::ObjectProxy<boost::string_ref, StackObject> StackObject::operator[](boost::string_ref i)
	{
		return Detail::ObjectProxy<boost::string_ref, StackObject>(this, i);
	}

	StackObject StackObject::push()
	{
		lua_pushvalue(interpreter, canonical);
		return StackObject(interpreter, -1);
	}

	void StackObject::pop(StackObject&)
	{
		lua_remove(interpreter, canonical);
	}

	int StackObject::type() const
	{
		return lua_type(interpreter, canonical);
	}

	int StackObject::index() const
	{
		return canonical;
	}

	lua_State * StackObject::state() const
	{
		return interpreter;
	}

	Detail::StackIterator StackObject::begin()
	{
		return Detail::StackIterator(this);
	}

	Detail::StackIterator StackObject::end()
	{
		return Detail::StackIterator();
	}

	Object Object::fromStack(lua_State * state, int ind)
	{
		lua_pushvalue(state, ind);
		int ref = luaL_ref(state, LUA_REGISTRYINDEX);

		return Object(state, ref);
	}

	Object newtable(lua_State * state)
	{
		lua_newtable(state);
		int ref = luaL_ref(state, LUA_REGISTRYINDEX);
		return Object(state, ref);
	}

	Object globals(lua_State * state)
	{
		return Object(state, LUA_RIDX_GLOBALS);
	}

	namespace Detail
	{
		Iterator::Iterator(Object * o)
			:o(o)
			,hasNext(1)
			,increments(0)
		{
			lua_pushnil(o->state());
			currentIndex = Object::fromStack(o->state(), -1);

			++(*this);
		}

		Iterator::Iterator()
			:hasNext(0)
			,increments(0)
		{}

		Iterator& Iterator::operator++()
		{
			StackCheck check(o->state(), 2, 0);

			o->push();
			currentIndex.push();

			hasNext = lua_next(o->state(), -2);

			currentIndex = Object::fromStack(o->state(), -2);
			value = Object::fromStack(o->state(), -1);
			increments++;

			return *this;
		}

		Iterator Iterator::operator++(int)
		{
			Iterator it = *this;
			++(*this);
			return it;
		}

		Object Iterator::operator*()
		{
			return value;
		}

		Object Iterator::key()
		{
			return currentIndex;
		}

		bool Iterator::operator==(const Iterator& other)
		{
			return (other.hasNext == 0 && hasNext == 0) || (other.hasNext == 1 && hasNext == 1 && other.increments == increments);
		}

		bool Iterator::operator!=(const Iterator& other)
		{
			return !(*this == other);
		}

		StackIterator::StackIterator(StackObject * o)
			:o(o)
			,currentIndex(0)
			,value(0)
			,hasNext(1)
			,increments(0)
		{
			lua_pushnil(o->state());
			++(*this);
		}

		StackIterator::StackIterator()
			:hasNext(0)
			,increments(0)
		{}

		StackIterator& StackIterator::operator++()
		{
			if (value)
			{
				//Pop value and fake key, keep key
				lua_pop(o->state(), 2);
			}

			hasNext = lua_next(o->state(), o->index());

			//Make a copy of the key, in case we call key() and cast to string.
			lua_pushvalue(o->state(), -2);
			currentIndex = lua_absindex(o->state(), -1);
			value = lua_absindex(o->state(), -2);

			increments++;

			return *this;
		}

		StackIterator StackIterator::operator++(int)
		{
			StackIterator it = *this;
			++(*this);

			return it;
		}

		StackObject StackIterator::operator*()
		{
			return StackObject(o->state(), value);
		}

		StackObject StackIterator::key()
		{
			return StackObject(o->state(), currentIndex);
		}

		bool StackIterator::operator==(const StackIterator& other)
		{
			return (other.hasNext == 0 && hasNext == 0) || (other.hasNext == 1 && hasNext == 1 && other.increments == increments);
		}

		bool StackIterator::operator!=(const StackIterator& other)
		{
			return !(*this == other);
		}
	}
}