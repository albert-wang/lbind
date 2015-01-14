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

#include <utility>
#include <iostream>
#include <cxxabi.h>

#include "lua.hpp"

#include "traits.hpp"
#include "tuplecall.hpp"
#include "convert.hpp"
#include "object.h"
#include "function.hpp"
#include "init.hpp"

struct Foo
{
	Foo(int a)
		:a(a)
	{}

	std::string bar(LBind::Object i, double b)
	{
		std::cout << LBind::cast<std::string>(i) << " " << b << "\n";
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

/*
struct OverloadedFunction
{
	std::vector<CallableBase *> callables;
};

int overloadCall(lua_State * l)
{
	int ind = lua_upvalueindex(1);
	OverloadedFunction * overloads = static_cast<OverloadedFunction *>(lua_touserdata(l, ind));

	for (size_t i = 0; i < overloads->callables.size(); ++i)
	{
		int res = overloads->callables[i]->call(l);
		if (res >= 0)
		{
			return res;
		}
	}

	//Error, no overload found!
	return -1;
}
*/

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
*/

	LBind::registerFunction(state, "testing", &Foo::bar);

	LBind::Object obj = LBind::newtable(state);
	LBind::StackObject stack = obj.push();

	stack[1] = 1.5;
	stack[2] = 1.5;
	stack[3] = 1.5;
	stack[4] = 1.5;

	for (auto it = stack.begin(); it != stack.end(); ++it)
	{
		std::cout << LBind::cast<int>(it.key()) << " : " << LBind::cast<float>(*it) << "\n";
	}

	obj.pop(stack);

	obj[1] = 2.5;

	LBind::Object globals = LBind::globals(state);
	globals["a"] = obj;

	luaL_dofile(state, argv[1]);

	float f = globals["a"];
	std::cout << f << "\n";

	lua_close(state);
}