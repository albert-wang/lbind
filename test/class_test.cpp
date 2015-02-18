#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include "fixtures.hpp"
#include "binddsl.hpp"

namespace
{
	struct ConstructFixture
	{
		static int constructs;
		static int destructs;
		static int copies;
		static int moves;
		static int assigns;

		ConstructFixture()
		{
			constructs = 0;
			destructs = 0;
			copies = 0;
			moves = 0;
			assigns = 0;
		}
	};

	int ConstructFixture::constructs;
	int ConstructFixture::destructs;
	int ConstructFixture::copies;
	int ConstructFixture::moves;
	int ConstructFixture::assigns;

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
			ConstructFixture::constructs++;
		}

		~Storage()
		{
			ConstructFixture::destructs++;
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

using namespace LBind;
BOOST_AUTO_TEST_CASE(basic_classes)
{
	ConstructFixture c;

	{
		StateFixture f;
		module(f.state)
			.class_<Storage<std::string>>("Storage")
				.constructor<std::string>()
				.def("get", &Storage<std::string>::get)
			.endclass()
		.end();

		std::string script = "a = Storage('Hello!')";
		BOOST_CHECK(!dostring(f, script));

		Storage<std::string> val = cast<Storage<std::string>>(globals(f.state)["a"]);
		BOOST_CHECK_EQUAL(val.get(), "Hello!");
	}

	BOOST_CHECK_EQUAL(c.constructs, 1);
	BOOST_CHECK_EQUAL(c.destructs, 1);
}

BOOST_AUTO_TEST_CASE(nested_classes)
{
	ConstructFixture c;

	{
		StateFixture f;
		module(f.state)
			.class_<Storage<std::string>>("String")
				.constructor<std::string>()
				.def("get", &Storage<std::string>::get)
			.endclass()
			.class_<Storage<Storage<std::string>>>("Storage")
				.constructor<Storage<std::string>>()
				.def("get", &Storage<Storage<std::string>>::get)
			.endclass()
		.end();

		std::string script = "a = String('Hello!'); b = Storage(a)";
		BOOST_CHECK(!dostring(f, script));

		Object g = globals(f.state);

		Storage<std::string> str = cast<Storage<std::string>>(g["a"]);
		Storage<Storage<std::string>> val = cast<Storage<Storage<std::string>>>(g["b"]);

		BOOST_CHECK_EQUAL(str.get(), "Hello!");
		BOOST_CHECK_EQUAL(val.get().get(), "Hello!");

		script = "aa = a:get(); bb = b:get():get()";
		BOOST_CHECK(!dostring(f, script));

		std::string aa = cast<std::string>(g["aa"]);
		std::string bb = cast<std::string>(g["bb"]);

		BOOST_CHECK_EQUAL(aa, "Hello!");
		BOOST_CHECK_EQUAL(bb, "Hello!");
	}

	BOOST_CHECK_EQUAL(c.constructs, 2);
}

BOOST_AUTO_TEST_CASE(basic_value)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.def_readwrite("value", &Storage<int>::stored)
		.endclass()
	.end();

	std::string script = "a = Int(42); a.value = 5 + a.value; val = a.value;";
	BOOST_CHECK(!dostring(f, script));

	Object g = globals(f.state);
	int i = cast<int>(g["val"]);

	BOOST_CHECK_EQUAL(i, 47);
}