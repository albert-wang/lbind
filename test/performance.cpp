#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <chrono>
#include <fmt/format.h>
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

template<typename CB>
void bench(uint64_t* first, size_t iterations, const char * name, CB&& cb) {
	auto start = std::chrono::high_resolution_clock::now();

	for (size_t i = 0; i < iterations; ++i)
	{
		cb();
	}

	auto end = std::chrono::high_resolution_clock::now();

	uint64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	if (first && *first == 0)
	{
		*first = duration;
	}

	double factor = 1;
	if (*first != 0)
	{
		factor = static_cast<double>(duration) / static_cast<double>(*first);
	}

	std::cout << fmt::format("iter={:12n} name={:24s} time={:12n} factor= {:.2f}", iterations, name, duration, factor) << "\n";
}

using namespace lbind;
BOOST_AUTO_TEST_CASE(chunk_vs_string)
{
	StateFixture f;
	size_t iterations = 100 * 1000;
	std::string script = "a = 1; return a + 1";

	uint64_t fastest = 0;
	bench(&fastest, iterations, "dostring", [&]() {
		BOOST_CHECK(!dostring(f, script));
		int a = lua_tointeger(f.state, -1);
		lua_pop(f.state, 1);
		BOOST_CHECK_EQUAL(a, 2);
	});

	luaL_loadstring(f.state, script.c_str());
	bench(&fastest, iterations, "dochunk", [&]() {
		lua_pushvalue(f.state, -1);
		lua_pcall(f.state, 0, 1, 0);
		int a = lua_tointeger(f.state, -1);
		BOOST_CHECK_EQUAL(a, 2);
		lua_pop(f.state, 1);
	});

	Object fn = compile(f.state, script.c_str());
	bench(&fastest, iterations, "doobject", [&]() {
		int a = call<int>(fn);
		BOOST_CHECK_EQUAL(a, 2);
	});
}

BOOST_AUTO_TEST_CASE(performance_difference)
{
	StateFixture f;
	module(f.state)
		.class_<Storage<int>>("Int")
			.constructor<int>()
			.def("add", &Storage<int>::fluent_add, returns_self)
			.def("addnr", &Storage<int>::add)
		.endclass()
		.def("add", external_add<int>)
		.def("add_i", add_i)
	.end();

	uint64_t fastest = 0;
	bench(&fastest, 1, "a:add", [&]() {
		std::string script = "a = Int(0); for i = 1, 1000 * 1000 do a:add(1) end";
		BOOST_CHECK(!dostring(f, script));
	});

	bench(&fastest, 1, "a:addnr", [&]() {
		std::string script = "a = Int(0); for i = 1, 1000 * 1000 do a:addnr(1) end";
		BOOST_CHECK(!dostring(f, script));
	});

	bench(&fastest, 1, "a+=1", [&]() {
		std::string script = "a = 0; for i = 1, 1000 * 1000 do a = a + 1 end";
		BOOST_CHECK(!dostring(f, script));
	});

	bench(&fastest, 1, "add(a, 1)", [&]() {
		std::string script = "a = Int(0); for i = 1, 1000 * 1000 do add(a, 1) end";
		BOOST_CHECK(!dostring(f, script));
	});

	bench(&fastest, 1, "cadd(a, 1)", [&]() {
		std::string script = "a = 0; for i = 1, 1000 * 1000 do a = add_i(a, 1) end";
		BOOST_CHECK(!dostring(f, script));
	});

	bench(&fastest, 1, "ladd(a, 1)", [&]() {
		std::string script = "function add_g(a, b) return a + b end a = 0; local add_n = add_g; for i = 1, 1000 * 1000 do a = add_n(a, 1) end";
		BOOST_CHECK(!dostring(f, script));
	});

	lua_register(f.state, "add_raw", add_lua);
	bench(&fastest, 1, "raw(a, 1)", [&]() {
		std::string script = "a = 0; for i = 1, 1000 * 1000 do add_raw(a, 1) end";
		BOOST_CHECK(!dostring(f, script));
	});
}
