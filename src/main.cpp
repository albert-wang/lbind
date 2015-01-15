#include <boost/function_types/result_type.hpp>
#include <boost/function_types/components.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/is_member_function_pointer.hpp>

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/next_prior.hpp>

#include <boost/utility/string_ref.hpp>

#include <boost/fusion/include/generation.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/at_c.hpp>

#include <utility>
#include <iostream>
#include <cxxabi.h>

#include "lua.hpp"

#include "traits.hpp"
#include "convert.hpp"
#include "object.h"
#include "tuplecall.hpp"
#include "function.hpp"
#include "classes.hpp"
#include "init.hpp"

struct Foo
{
	Foo(int a)
		:a(a)
	{}

	std::string bar(LBind::Object i, double b)
	{
		std::cout << a << " -> " << LBind::cast<std::string>(i) << " " << b << "\n";
		return "Dongs";
	}

	void yap(LBind::Object i, double b)
	{
		std::cout << LBind::cast<std::string>(i) << " " << b << " Yap\n";
	}

	int a;
};

Foo * construct(int a)
{
	return new Foo(a);
}

void basic(int a, int b)
{
	std::cout << " " << a << " " << b << "\n";
}

void basicz()
{
	std::cout << "Zero args \n";
}

struct ClassRepresentation
{
	//Functions are stored in metatables, including constructors.

	//This stores things like properties
};

class ClassRegistrar
{
	static int warn(lua_State *)
	{
		std::cout << "Newindex called!";
		return 0;
	}
public:
	ClassRegistrar(lua_State * state, LBind::StackObject meta, LBind::StackObject classes, const char * name, int * metatableStorage)
		:state(state)
		,metatable(meta)
		,classes(classes)
		,name(name)
		,metatableStorage(metatableStorage)
	{}

	template<typename F>
	ClassRegistrar& def(F f, const char * name)
	{
		assert(metatable.index() == lua_gettop(state));
		LBind::pushFunction(state, name, f);
		lua_setfield(state, -2, name);
		assert(metatable.index() == lua_gettop(state));

		return *this;
	}

	void finish()
	{
		assert(metatable.index() == lua_gettop(state));
		lua_setfield(state, classes.index(), name);

		lua_getfield(state, classes.index(), name);
		*metatableStorage = luaL_ref(state, LUA_REGISTRYINDEX);

		lua_rawgeti(state, LUA_REGISTRYINDEX, *metatableStorage);
		lua_pushvalue(state, -1);
		lua_setfield(state, -2, "__index");

		lua_pushcfunction(state, warn);
		lua_setfield(state, -2, "__newindex");

		lua_pop(state, 1);
	}
private:
	lua_State * state;
	LBind::StackObject metatable;
	LBind::StackObject classes;
	const char * name;
	int * metatableStorage;
};

template<typename T>
ClassRegistrar registerClass(lua_State * s, const char * name)
{
	LBind::StackCheck check(s, 0, 2);

	//Push the class registration table.
	lua_rawgeti(s, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	int res = lua_getfield(s, -1, "__classes");
	if (res == LUA_TNIL)
	{
		//Pop the nil.
		lua_pop(s, 1);

		lua_newtable(s);
		lua_pushvalue(s, -1);

		lua_setfield(s, -3, "__classes");

		res = LUA_TTABLE;
	}

	assert(res == LUA_TTABLE);

	//Stack is now [_G, __classes]
	//Do we have an existing class table?
	res = lua_getfield(s, -1, name);
	if (res == LUA_TNIL)
	{
		lua_pop(s, 1);
		lua_newtable(s);

		res = LUA_TTABLE;

		//This leaks, needs to be stored somewhere
		ClassRepresentation * rep = new ClassRepresentation();
		lua_pushlightuserdata(s, rep);
		lua_setfield(s, -2, "__rep");
	}

	//Stack is [_G, __classes, ClassTable]
	//Remove _G from the stack
	lua_remove(s, -3);

	//Set the __index and __newindex metamethod to this table.

	LBind::Detail::MetatableName<T>::name = name;
	return ClassRegistrar(s, LBind::StackObject::fromStack(s, -1), LBind::StackObject::fromStack(s, -2), name, &LBind::Detail::MetatableName<T>::index);
}

int main(int argc, char * argv[])
{
	lua_State * state = luaL_newstate();
	LBind::open(state);

	luaL_openlibs(state);

/*
	module.class_<Foo>("Foo")	//Creates a metatable called "Foo", which is associated with the class instance Foo
		.constructor<int>()     //Creates a function called Foo globally, which constructs a new instance of Foo,
		.def(&Foo::bar, "bar")  //Create a function in the "Foo" metatable, named bar.
		.def_readwrite(&Foo::a, "a") //Properties?
		.def_readonly(&Foo::b, "b")  //How do you implement these?



	Metatable format
		- all functions are in the metatable.
		- constructors are bound to __call
		- properties are done through a C++ interface hooked into __index and __newindex for get/set
			- If lookup fails, then just rawset to the table.
*/

	registerClass<Foo>(state, "Foo")
		.def(&Foo::bar, "bar")
		.finish();
	;

	Foo ff(42);
	LBind::registerFunction(state, "testing", &Foo::bar);

	LBind::Object obj = LBind::newtable(state);
	LBind::StackObject stack = obj.push();

	stack["kitty"] = 2.5;
	stack["dog"] = stack["kitty"];

	float a = stack["kitty"];

	obj.pop(stack);

	obj[1] = 2.5;

	LBind::Object globals = LBind::globals(state);
	globals["a"] = obj;
	globals["foo"] = &ff;

	if (luaL_dofile(state, argv[1]))
	{
		const char * err = lua_tostring(state, -1);
		std::cout << err << "\n";
	}

	float f = globals["a"];

	LBind::call<void>(globals["add"], 1, 52);
	int res = LBind::call<int>(globals["sub"], 42, 1);
	std::cout << "Result: " << res << "\n";

	lua_close(state);
}