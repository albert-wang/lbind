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
#include "binddsl.hpp"
#include "init.hpp"

struct Foo
{
	Foo(int a)
		:a(a)
	{}

	std::string bar(lbind::Object a)
	{
		Foo * f = lbind::cast<Foo *>(a);

		std::cout << f->a << "\n";
		return "Dongs";
	}

	void yap(lbind::Object i, double b)
	{
		std::cout << lbind::cast<std::string>(i) << " " << b << " Yap\n";
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

#ifndef UNIT_TESTING
int main(int argc, char * argv[])
{
	lua_State * state = luaL_newstate();
	lbind::open(state);

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
	lbind::module(state)
		.class_<Foo>("Foo")
			.constant("magic", 42)
			.def("bar", &Foo::bar)
			.def_readwrite("a", &Foo::a)
		.endclass()
	.end();

	Foo ff(42);
	lbind::Object obj = lbind::newtable(state);

	obj["kitty"] = 42;
	obj["dog"] = obj["kitty"];
	obj[1] = 2.5;

	lbind::Object globals = lbind::globals(state);
	globals["foo"] = ff;

	if (luaL_dofile(state, argv[1]))
	{
		const char * err = lua_tostring(state, -1);
		std::cout << err << "\n";
	}

	float f = globals["a"];

	lbind::call<void>(globals["add"], 1, 52);
	int res = lbind::call<int>(globals["sub"], 42, 1);
	std::cout << "Result: " << res << "\n";

	lua_close(state);
}
#endif
