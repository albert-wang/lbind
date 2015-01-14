#include <boost/function_types/result_type.hpp>
#include <boost/function_types/components.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/function_arity.hpp>
#include <boost/function_types/is_member_function_pointer.hpp>

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/next_prior.hpp>


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

struct Foo
{
	Foo(int a)
		:a(a)
	{}

	std::string bar(Object i, double b)
	{
		std::cout << object_cast<std::string>(i) << " " << b << "\n";
		return "Dongs";
	}

	void yap(Object i, double b)
	{
		std::cout << object_cast<std::string>(i) << " " << b << " Yap\n";
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

struct CallableBase
{
	virtual int call(lua_State * state) = 0;
};

struct PullFromLua
{
	PullFromLua(lua_State * state)
		:state(state)
		,index(1)
		,failed(false)
	{}

	template<typename T>
	void operator()(T& out) const
	{
		int res = Lookup<T>::converter::from(state, index, out);
		if (res < 0)
		{
			//Failure to convert.
			failed = true;
		}

		index += res;
	}

	lua_State * state;
	mutable int index;
	mutable bool failed;
};

template<typename F, bool isVoid>
struct Callable
{};

template<typename F>
struct Callable<F, false> : CallableBase
{
	Callable(F f)
		:function(f)
	{}

	int call(lua_State * state)
	{
		using namespace boost::fusion;
		int argc = lua_gettop(state);

		if (argc != boost::function_types::function_arity<F>::value)
		{
			//Could not invoke properly, try another overload.
			return -1;
		}

		typename FunctionTraits<F>::lua_tuple args;
		//Pull arguments from lua
		PullFromLua pull(state);
		for_each(args, pull);

		if (pull.failed)
		{
			return -1;
		}

		//Now we have a value that we need to push back to lua
		auto val = TupleCall<F,
			boost::function_types::function_arity<F>::value,
			boost::function_types::is_member_function_pointer<F>::value
		>::apply(function, std::move(args));

		typedef typename Detail::ConvertToLuaType::template apply<typename boost::function_types::result_type<F>::type>::raw RawType;
		return Convert<RawType>::to(state, val);
	}

	F function;
};

template<typename F>
struct Callable<F, true> : CallableBase
{
	Callable(F f)
		:function(f)
	{}

	int call(lua_State * state)
	{
		using namespace boost::fusion;
		int argc = lua_gettop(state);

		if (argc != boost::function_types::function_arity<F>::value)
		{
			//Could not invoke properly, try another overload.
			return -1;
		}

		typename FunctionTraits<F>::lua_tuple args;
		//Pull arguments from lua
		PullFromLua pull(state);
		for_each(args, pull);

		if (pull.failed)
		{
			return -1;
		}

		TupleCall<F,
			boost::function_types::function_arity<F>::value,
			boost::function_types::is_member_function_pointer<F>::value
		>::apply(function, std::move(args));

		return 0;
	}

	F function;
};

int callableCall(lua_State * l)
{
	int ind = lua_upvalueindex(1);
	CallableBase * b = static_cast<CallableBase *>(lua_touserdata(l, ind));

	int result = b->call(l);
	if (result < 0)
	{
		//Error!
	}

	return result;
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

template<typename F>
CallableBase * createCallable(F f)
{
	return new Callable<F,
		boost::is_same<
			void,
			typename boost::function_types::result_type<F>::type
		>::value
	>(f);
}

template<typename F>
void bind(lua_State * state, F f, const char * name)
{
	CallableBase * callable = createCallable(f);
	lua_pushlightuserdata(state, callable);
	lua_pushcclosure(state, callableCall, 1);
	lua_setglobal(state, name);
}

int main(int argc, char * argv[])
{
	lua_State * state = luaL_newstate();
	luaL_openlibs(state);

/*
	module.class_<Foo>("Foo")	//Creates a metatable called "Foo", which is associated with the class instance Foo
		.constructor<int>()     //Creates a function called Foo globally, which constructs a new instance of Foo,
		.def(&Foo::bar, "bar")  //Create a function in the "Foo" metatable, named bar.
		.def_readwrite(&Foo::a, "a") //Properties?
		.def_readonly(&Foo::b, "b")  //How do you implement these?
*/
	bind(state, construct, "construct");
	bind(state, &Foo::bar, "testing");

	luaL_dofile(state, argv[1]);
}