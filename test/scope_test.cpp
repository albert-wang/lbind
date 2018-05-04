#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include "fixtures.hpp"

#include "binddsl.hpp"

using namespace lbind;

namespace
{
	template<int i>
	int constant()
	{
		return i;
	}

	template<typename T>
	struct Storage
	{
		explicit Storage(T t)
			:stored(t)
		{
		}

		const T& get() const
		{
			return stored;
		}

		void set(const T& other)
		{
			stored = other;
		}

		void add(const T& another) const
		{
			stored += another;
		}

		T stored;
	};
}

BOOST_AUTO_TEST_CASE(basic_scope)
{
	StateFixture f;

	module(f.state)
		.def("name", constant<42>)
	.end();

	std::string script = "a = name()";
	BOOST_CHECK(!dostring(f, script));

	lua_getglobal(f.state, "a");
	int res = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(res, 42);
}

BOOST_AUTO_TEST_CASE(nested_scope)
{
	StateFixture f;

	module(f.state)
		.scope("ns")
			.def("name", constant<42>)
		.endscope()
	.end();

	std::string script = "a = ns.name()";
	BOOST_CHECK(!dostring(f, script));

	lua_getglobal(f.state, "a");
	int res = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(res, 42);
}

BOOST_AUTO_TEST_CASE(reopen_scope)
{
	StateFixture f;

	module(f.state)
		.scope("ns")
			.def("name", constant<1>)
		.endscope()
		.scope("ns")
			.def("two", constant<2>)
		.endscope()
	.end();

	std::string script = "a = ns.two()";
	BOOST_CHECK(!dostring(f, script));

	lua_getglobal(f.state, "a");
	int res = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(res, 2);
}

BOOST_AUTO_TEST_CASE(reopen_scope_module)
{
	StateFixture f;

	module(f.state)
		.scope("ns")
			.def("name", constant<1>)
		.endscope()
	.end();

	module(f.state)
		.scope("ns")
			.def("two", constant<2>)
		.endscope()
	.end();

	std::string script = "a = ns.two()";
	BOOST_CHECK(!dostring(f, script));

	lua_getglobal(f.state, "a");
	int res = lua_tointeger(f.state, -1);
	BOOST_CHECK_EQUAL(res, 2);
}

BOOST_AUTO_TEST_CASE(register_class)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<std::string>>("Storage")
			.constructor<std::string>()
			.def("get", &Storage<std::string>::get)
		.endclass()
	.end();

	std::string script = "a = Storage('42'); b = a:get()";
	const char * err = dostring(f, script);
	if (err)
	{
		std::cout << err << "\n";
	}
	BOOST_CHECK(!err);

	lua_getglobal(f.state, "b");
	const char * msg = lua_tostring(f.state, -1);

	BOOST_CHECK_EQUAL(msg, "42");
}

BOOST_AUTO_TEST_CASE(register_class_in_scope)
{
	StateFixture f;

	module(f.state)
		.scope("ns")
			.class_<Storage<int>>("Storage")
				.constructor<int>()
				.def("get", &Storage<int>::get)
			.endclass()
		.endscope()
	.end();

	std::string script = "a = ns.Storage(42); b = a:get()";
	BOOST_CHECK(!dostring(f, script));

	lua_getglobal(f.state, "b");
	int v = lua_tointeger(f.state, -1);

	BOOST_CHECK_EQUAL(v, 42);
}

BOOST_AUTO_TEST_CASE(register_constants)
{
	StateFixture f;
	module(f.state)
		.scope("ns")
			.constant("Two", 2)
			.constant("Twos", "2")
		.endscope()
	.end();

	std::string script = "a = ns.Twos";
	BOOST_CHECK(!dostring(f, script));

	lua_getglobal(f.state, "a");
	const char * v = lua_tostring(f.state, -1);
	BOOST_CHECK_EQUAL(v, std::string("2"));
}
