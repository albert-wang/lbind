#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/fusion/include/at_c.hpp>
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


	template<typename T>
	void external_add(Storage<T> * val, int n)
	{
		val->stored += n;
	}

	template<typename T>
	void external_add_ref(Storage<T>& val, int n)
	{
		val.stored += n;
	}
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

BOOST_AUTO_TEST_CASE(readonly_property)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.property("value", &Storage<int>::get)
		.endclass()
	.end();

	std::string script = "a = Int(42); b = a.value;";
	BOOST_CHECK(!dostring(f, script));

	Object g = globals(f.state);
	int i = cast<int>(g["b"]);

	BOOST_CHECK_EQUAL(i, 42);
}

BOOST_AUTO_TEST_CASE(full_property)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.property("value", &Storage<int>::get, &Storage<int>::set)
		.endclass()
	.end();

	std::string script = "a = Int(42); a.value = 5 + a.value; val = a.value;";
	BOOST_CHECK(!dostring(f, script));

	Object g = globals(f.state);
	int i = cast<int>(g["val"]);

	BOOST_CHECK_EQUAL(i, 47);
}

BOOST_AUTO_TEST_CASE(constant_in_class)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constant("Answer", 42)
			.constant("AnswerString", "42")
		.endclass()
	.end();

	std::string script = "a = Int.Answer; b = Int.AnswerString";
	BOOST_CHECK(!dostring(f, script));

	Object g = globals(f.state);
	int i = cast<int>(g["a"]);
	std::string s = cast<std::string>(g["b"]);

	BOOST_CHECK_EQUAL(i, 42);
	BOOST_CHECK_EQUAL(s, "42");
}

BOOST_AUTO_TEST_CASE(non_member_member_bind)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.def("add", external_add<int>)
		.endclass()
	.end();

	std::string script = "a = Int(100); a:add(42);";
	BOOST_CHECK(!dostring(f, script));

	Storage<int> val = cast<Storage<int>>(globals(f.state)["a"]);
	BOOST_CHECK_EQUAL(val.get(), 142);
}
BOOST_AUTO_TEST_CASE(non_member_member_bind_through_reference)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.def("add", external_add_ref<int>)
		.endclass()
	.end();

	std::string script = "a = Int(100); a:add(42);";
	BOOST_CHECK(!dostring(f, script));

	Storage<int> val = cast<Storage<int>>(globals(f.state)["a"]);
	BOOST_CHECK_EQUAL(val.get(), 142);
}

BOOST_AUTO_TEST_CASE(can_call_reference)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
		.endclass()
		.def("add", external_add_ref<int>)
	.end();

	std::string script = "a = Int(123); add(a, 2)";
	BOOST_CHECK(!dostring(f, script));
	Storage<int> val = cast<Storage<int>>(globals(f.state)["a"]);
	BOOST_CHECK_EQUAL(val.get(), 125);
}