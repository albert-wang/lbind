#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <chrono>
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

	std::ostream& operator<<(std::ostream& o, const ConstructFixture& f)
	{
		o << "C: " << f.constructs << " D: " << f.destructs << " CC: " << f.copies << " M: " << f.moves << " A: " << f.assigns;
		return o;
	}

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

	struct MultipleStorage
	{
		MultipleStorage(int a, std::string b, double c)
			:a(a)
			,b(b)
			,c(c)
		{}

		MultipleStorage(int a)
			:a(a)
		{}

		MultipleStorage()
			:a(4)
		{}

		int a;
		std::string b;
		double c;
	};

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

		Storage(const Storage& other)
			:stored(other.stored)
		{
			ConstructFixture::copies++;
		}

		Storage(Storage&& other)
			:stored(other.stored)
		{
			ConstructFixture::moves++;
		}

		Storage& operator=(const Storage& other)
		{
			stored = other.stored;
			ConstructFixture::assigns++;
		}

		const T& get() const
		{
			return stored;
		}

		void set(const T& other)
		{
			stored = other;
		}

		void add(const T& another)
		{
			stored += another;
		}

		Storage& fluent_add(const T& another)
		{
			add(another);
			return *this;
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

	int add_i(int a, int b)
	{
		return a + b;
	}

	int add_lua(lua_State * s)
	{
		double a = lua_tonumber(s, -1);
		double b = lua_tonumber(s, -2);

		lua_pushnumber(s, a + b);
		return 1;
	}

}

using namespace LBind;
BOOST_AUTO_TEST_CASE(chunk_vs_string)
{
	StateFixture f;
	size_t iterations = 2;
	std::string script = "a = 1; return a + 1";

	auto start = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < iterations; ++i)
	{
		BOOST_CHECK(!dostring(f, script));
		int a = lua_tointeger(f.state, -1);
		BOOST_CHECK_EQUAL(a, 2);
	}
	auto end = std::chrono::high_resolution_clock::now();
	auto elapsed = end - start;

	start = std::chrono::high_resolution_clock::now();
	luaL_loadstring(f.state, script.c_str());
	stackdump(f.state);
	for (size_t i = 0; i < iterations; ++i)
	{
		std::cout << lua_gettop(f.state) << "\n";
		lua_pcall(f.state, 0, 0, 0);
		int a = lua_tointeger(f.state, -1);
		BOOST_CHECK_EQUAL(a, 2);
		lua_pop(f.state, 1);
	}
	end = std::chrono::high_resolution_clock::now();
	auto loadElapsed = end - start;

	start = std::chrono::high_resolution_clock::now();
	Object fn = compile(f.state, script.c_str());
	for (size_t i = 0; i < iterations; ++i)
	{
		int a = call<int>(fn);
		BOOST_CHECK_EQUAL(a, 2);
	}
	end = std::chrono::high_resolution_clock::now();
	auto basicobject = end - start;

	std::cout << "100k dostring: " << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << " us\n";
	std::cout << "100k dochunk : " << std::chrono::duration_cast<std::chrono::microseconds>(loadElapsed).count() << " us\n";
	std::cout << "100k object  : " << std::chrono::duration_cast<std::chrono::microseconds>(basicobject).count() << " us\n";
}

BOOST_AUTO_TEST_CASE(performance_difference)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.def("add", &Storage<int>::fluent_add, returns_self)
		.endclass()
		.def("add", external_add<int>)
		.def("add_i", add_i)
	.end();

	std::string script = "a = Int(0); for i = 1, 1000 * 1000 do a:add(1) end";

	auto start = std::chrono::high_resolution_clock::now();
	BOOST_CHECK(!dostring(f, script));
	auto end = std::chrono::high_resolution_clock::now();
	auto elapsed = end - start;

	script = "a = 0; for i = 1, 1000 * 1000 do a = a + 1 end";
	start = std::chrono::high_resolution_clock::now();
	BOOST_CHECK(!dostring(f, script));
	end = std::chrono::high_resolution_clock::now();
	auto scriptelapsed = end - start;

	script = "a = Int(0); for i = 1, 1000 * 1000 do add(a, 1) end";
	start = std::chrono::high_resolution_clock::now();
	BOOST_CHECK(!dostring(f, script));
	end = std::chrono::high_resolution_clock::now();
	auto externadd = end - start;

	script = "a = 0; for i = 1, 1000 * 1000 do a = add_i(a, 1) end";
	start = std::chrono::high_resolution_clock::now();
	BOOST_CHECK(!dostring(f, script));
	end = std::chrono::high_resolution_clock::now();
	auto calladd = end - start;

	script = "function add_g(a, b) return a + b end a = 0; local add_n = add_g; for i = 1, 1000 * 1000 do a = add_n(a, 1) end";
	start = std::chrono::high_resolution_clock::now();
	BOOST_CHECK(!dostring(f, script));
	end = std::chrono::high_resolution_clock::now();
	auto luaadd = end - start;

	lua_register(f.state, "add_raw", add_lua);

	script = "a = 0; for i = 1, 1000 * 1000 do add_raw(a, 1) end";
	start = std::chrono::high_resolution_clock::now();
	BOOST_CHECK(!dostring(f, script));
	end = std::chrono::high_resolution_clock::now();
	auto rawelapsed = end - start;

	std::cout << "1 million    a:add(1): " << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << " us\n";
	std::cout << "1 million      a += 1: " << std::chrono::duration_cast<std::chrono::microseconds>(scriptelapsed).count() << " us\n";
	std::cout << "1 million   add(a, 1): " << std::chrono::duration_cast<std::chrono::microseconds>(externadd).count() << " us\n";
	std::cout << "1 million  cadd(a, 1): " << std::chrono::duration_cast<std::chrono::microseconds>(calladd).count() << " us\n";
	std::cout << "1 million  ladd(a, 1): " << std::chrono::duration_cast<std::chrono::microseconds>(luaadd).count() << " us\n";
	std::cout << "1 million   raw(a, 1): " << std::chrono::duration_cast<std::chrono::microseconds>(rawelapsed).count() << " us\n";
}
