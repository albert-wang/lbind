#define BOOST_TEST_MODULE LBindTest
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include "fixtures.hpp"

namespace
{
	boost::int64_t addone(boost::int64_t a)
	{
		return a + 1;
	}

	float multiply(float a, boost::uint8_t b)
	{
		return a * b;
	}

	void makeglobal(lua_State * state, double d, std::string name)
	{
		lua_pushnumber(state, d);
		lua_setglobal(state, name.c_str());
	}
	void call_something(lua_State * state, const char * name)
	{
		lua_getglobal(state, name);
		lua_pcall(state, 0, 0, 0);
	}

	const char * constant()
	{
		return "Hello, World!";
	}

	std::string add_string(const std::string& a, const std::string& b)
	{
		return a + b;
	}

	float add_float(float a, float b)
	{
		return a + b;
	}

	int add_int(int a, int b)
	{
		return a + b;
	}
}

BOOST_AUTO_TEST_CASE(integer_function)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "addone", addone);
	const char * script = "c = addone(4)";

	BOOST_CHECK(!dostring(f.state, script));

	lua_getglobal(f.state, "c");
	int e = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(e, 5);
}

BOOST_AUTO_TEST_CASE(integer_function_fail_cast)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "addone", addone);
	const char * script = "c = addone(4.2)";
	BOOST_CHECK(dostring(f.state, script));
}

BOOST_AUTO_TEST_CASE(integer_function_correct_cast)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "addone", addone);
	const char * script = "c = addone(4.0)";

	BOOST_CHECK(!dostring(f.state, script));
	lua_getglobal(f.state, "c");
	int e = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(e, 5);
}

BOOST_AUTO_TEST_CASE(integer_limits)
{
	StateFixture f;

	boost::int32_t limit = std::numeric_limits<boost::int32_t>::max() - 1;
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "addone", addone);

	std::string script = "c = addone(" + boost::lexical_cast<std::string>(limit) + ")";

	BOOST_CHECK(!dostring(f.state, script.c_str()));

	lua_getglobal(f.state, "c");
	int e = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(e, std::numeric_limits<boost::int32_t>::max());
}


BOOST_AUTO_TEST_CASE(arbitrary_convertions)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "multiply", multiply);
	std::string script = "c = multiply(42, 2)";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	lua_getglobal(f.state, "c");
	double c = lua_tonumber(f.state, -1);
	BOOST_CHECK_CLOSE(c, 84, 0);
}

BOOST_AUTO_TEST_CASE(arbitrary_convertions_overflow_behavior)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "multiply", multiply);
	std::string script = "c = multiply(1, 270)";

#ifdef CHECK_INTEGER_OVERFLOW
	BOOST_CHECK(dostring(f.state, script.c_str()));
#else
	BOOST_CHECK(!dostring(f.state, script.c_str()));
	lua_getglobal(f.state, "c");
	double c = lua_tonumber(f.state, -1);
	BOOST_CHECK_NE(c, 270);
#endif
}


BOOST_AUTO_TEST_CASE(writing_to_globals_in_function_work)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "makeglobal", makeglobal);
	std::string script = "makeglobal(42, 'c')";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	lua_getglobal(f.state, "c");
	double d = lua_tonumber(f.state, -1);

	BOOST_CHECK_EQUAL(d, 42);
}


BOOST_AUTO_TEST_CASE(nested_function_calls)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "invoke", call_something);
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "multiply", multiply);

	std::string script = "function something() c = multiply(1.5, 200) end\n"
		"invoke('something')";

	BOOST_CHECK(!dostring(f.state, script.c_str()));
	lua_getglobal(f.state, "c");
	double d = lua_tonumber(f.state, -1);

	BOOST_CHECK_EQUAL(d, 300);
}

BOOST_AUTO_TEST_CASE(basic_recursion)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "invoke", call_something);
	std::string script = "c = 10; function recurse() c = c - 1; if c > 0 then invoke('recurse') end end recurse()";

	BOOST_CHECK(!dostring(f.state, script.c_str()));
	lua_getglobal(f.state, "c");

	double d = lua_tonumber(f.state, -1);
	BOOST_CHECK_EQUAL(d, 0);
}

BOOST_AUTO_TEST_CASE(bind_closure)
{
	StateFixture f;

	int count = 0;
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "incr", [&count]()
	{
		count++;
	});

	std::string script = "incr(); incr(); incr();";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	BOOST_CHECK_EQUAL(count, 3);
}

BOOST_AUTO_TEST_CASE(bind_simple_function)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "constant", constant);

	std::string script = "c = constant()";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	lua_getglobal(f.state, "c");
	std::string d = lua_tostring(f.state, -1);
	BOOST_CHECK_EQUAL(d, "Hello, World!");
}

BOOST_AUTO_TEST_CASE(bind_closure_args)
{
	StateFixture f;

	int count = 0;
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "incrby", [&count](size_t a)
	{
		count += a;
		return count;
	});

	std::string script = "incrby(1); incrby(4); c = incrby(0)";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	BOOST_CHECK_EQUAL(count, 5);

	lua_getglobal(f.state, "c");
	int result = lua_tointeger(f.state, -1);

	BOOST_CHECK_EQUAL(count, result);
}

BOOST_AUTO_TEST_CASE(overloaded_functions)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_string);
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_float);

	std::string script = "a = add(2, 5); b = add('a', 'b');";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	lua_getglobal(f.state, "a");
	double result = lua_tonumber(f.state, -1);

	lua_getglobal(f.state, "b");
	std::string strresult = lua_tostring(f.state, -1);

	BOOST_CHECK_EQUAL(result, 7);
	BOOST_CHECK_EQUAL(strresult, "ab");
}

BOOST_AUTO_TEST_CASE(overloaded_function_no_overload_failure)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_string);
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_float);

	std::string script = "a = add(2, 'bcd5');";
	BOOST_CHECK(dostring(f.state, script.c_str()));
}

BOOST_AUTO_TEST_CASE(overloaded_function_no_overload_failure_2)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_int);
	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_float);

	std::string script = "a = add('bw1', 'w2');";
	BOOST_CHECK(dostring(f.state, script.c_str()));
}

//TODO: Not sure if this is desired behavior.
BOOST_AUTO_TEST_CASE(string_to_integer_conversion_is_implicit)
{
	StateFixture f;

	LBind::registerFunction(f.state, LUA_RIDX_GLOBALS, "add", add_int);
	std::string script = "a = add('1', '2');";
	BOOST_CHECK(!dostring(f.state, script.c_str()));

	lua_getglobal(f.state, "a");
	int res = lua_tointeger(f.state, -1);

	BOOST_CHECK_EQUAL(res, 3);
}
/*
//COROUTINES work!
BOOST_AUTO_TEST_CASE(coroutines_work)
{
	using namespace LBind;

	StateFixture f;
	Coroutine thread = cast<Coroutine>(globals(f.state)["th"]);

	//Currently, only very trivial resumes() are supported.
	thread.resume();


	"R(
		funciton WaitFrames(entity, n)
			entity.LogicWaitSteps = n
			coroutine.yield();
		end

		status = coroutine.create(function(A)
			A:EMWaitFrames(self, 30);
			A:EMBroadcastMessage(A.EMBossAdd | 1, 'explode', {})
		end);

		---------

		onMessage_explode = function(self, A)
			A:BInvokeTurret('circle', self.position, {});
			self:die();
		end
	)R"



	thread.resume();
}*/