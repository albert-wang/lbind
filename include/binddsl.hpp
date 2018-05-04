#pragma once
#include <lua.hpp>
#include <string>
#include <boost/cstdint.hpp>
#include <boost/utility/string_ref.hpp>

#include "classes.hpp"

namespace lbind
{
	/*
		Exposes a simple API to bind functions

		open(state)
			.def("name", function)
			.constant("name", some_constant)
			.scope("namespace")
				.class_<Class>("Class")
					.def("member", &Class::member_function)
					.def_readwrite("a", &Class::read_write_member_variable)
					.def_readonly("b", &Class::read_only_variable)
					.constant("c", some_constant)
				.endclass()
			.endscope()
		.end();
	*/

	class Scope
	{
	public:
		Scope(lua_State * state, int index, Scope * parent, const std::string& n);

		Scope scope(boost::string_ref name);
		Scope& endscope();

		template<typename F>
		Scope& def(boost::string_ref name, F callable)
		{
			registerFunction(state, index, name.data(), callable);
			return *this;
		}

		template<typename U>
		Scope& constant(boost::string_ref name, U value)
		{
			BOOST_STATIC_ASSERT(Convert<U>::is_primitive::value);

			StackCheck check(state, 1, 0);

			lua_rawgeti(state, LUA_REGISTRYINDEX, index);
			Convert<U>::to(state, value);
			lua_setfield(state, -2, name.data());

			return *this;
		}

		template<typename C>
		Detail::ClassRegistrar<C> class_(const char * name)
		{
			return registerClass<C>(state, name, index, this);
		}

		//Used to end a module.
		void end();
	private:
		Scope * parent;
		std::string name;

		lua_State * state;
		int index;
	};


	Scope module(lua_State * s);
}