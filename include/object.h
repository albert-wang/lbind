#pragma once
#include "lua.hpp"
#include "convert.hpp"
#include "stackcheck.hpp"

#include <boost/utility/string_ref.hpp>
#include <iostream>

namespace LBind
{
	/*
		This exposes two different forms of object, Object and StackObject.
		When using a StackObject, all operations between Object::push and Object::pop
		must leave the stack below the first call to Object::push untouched.

		Object has no such stack limitations, but is more expensive (it may touch the
		registry on each operation) as a result. For performance reasons, use
		StackObject whenever possible.
	*/

	class Object;
	namespace Detail
	{
		template<typename T, typename O>
		class ObjectProxy;

		class Iterator;
		class StackIterator;
	}

	//Basically an object, but is on the stack.
	class StackObject
	{
		friend class Detail::ObjectProxy<int, StackObject>;
		friend class Detail::ObjectProxy<boost::string_ref, StackObject>;
	public:
		static StackObject fromStack(lua_State * state, int ind);

		StackObject(lua_State * interpreter, int index);
		typedef Detail::StackIterator iterator;

		//Upon construction, this object must be at the top of the stack.
		Detail::ObjectProxy<int, StackObject> operator[](int i);
		Detail::ObjectProxy<boost::string_ref, StackObject> operator[](boost::string_ref k);

		//Pushes this object onto the stack
		StackObject push();
		void pop(StackObject&);

		int type() const;
		int index() const;
		lua_State * state() const;

		iterator begin();
		iterator end();
	private:
		int stackPush(int i) const;
		int stackPush(boost::string_ref i) const;

		template<typename T>
		void stackSet(int key, const T& t)
		{
			Convert<T>::to(interpreter, t);

			lua_seti(interpreter, canonical, key);
		}

		template<typename T>
		void stackSet(boost::string_ref key, const T& t)
		{
			Convert<T>::to(interpreter, t);

			//Kinda bad form.
			assert(key[key.size()] == 0);
			lua_setfield(interpreter, canonical, key.data());
		}

		lua_State * interpreter;
		int canonical;
	};

	//TODO: THIS LEAKS REFERENCES.
	//TODO: CALL
	class Object
	{
		friend class Detail::ObjectProxy<int, Object>;
		friend class Detail::ObjectProxy<boost::string_ref, Object>;
	public:
		static Object fromStack(lua_State * state, int ind);

		typedef Detail::Iterator iterator;

		Object(lua_State * state, int index);
		Object();

		//Pushes this object onto the stack.
		StackObject push();
		void pop(StackObject&);

		Detail::ObjectProxy<int, Object> operator[](int i);
		Detail::ObjectProxy<boost::string_ref, Object> operator[](boost::string_ref k);

		int type() const;
		int index() const;
		lua_State * state() const;

		iterator begin();
		iterator end();
	private:
		int stackPush(int i) const;
		int stackPush(boost::string_ref i) const;

		template<typename T>
		void stackSet(int key, const T& t)
		{
			lua_rawgeti(interpreter, LUA_REGISTRYINDEX, ind);
			Convert<T>::to(interpreter, t);

			lua_seti(interpreter, -2, key);
			lua_pop(interpreter, 1);
		}

		template<typename T>
		void stackSet(boost::string_ref key, const T& t)
		{
			lua_rawgeti(interpreter, LUA_REGISTRYINDEX, ind);
			Convert<T>::to(interpreter, t);

			//Kinda bad form.
			assert(key[key.size()] == 0);
			lua_setfield(interpreter, -2, key.data());
			lua_pop(interpreter, 1);
		}

		lua_State * interpreter;
		int ind;
	};

	template<>
	struct Convert<Object, void>
	{
		typedef Object type;
		typedef boost::false_type is_primitive;

		static Object&& forward(type&& t)
		{
			return std::move(t);
		}

		template<typename T>
		static T&& universal(Object&& t)
		{
			return static_cast<T&&>(t);
		}

		static int from(lua_State * state, int index, type& out)
		{
			out = Object::fromStack(state, index);
			return 1;
		}

		static int to(lua_State * state, const type& in)
		{
			assert(state = in.state());
			lua_rawgeti(state, LUA_REGISTRYINDEX, in.index());
			return 1;
		}
	};

	template<>
	struct Convert<StackObject, void>
	{
		typedef Object type;
		typedef boost::false_type is_primitive;

		//Cannot use this as an argument to a function, so we're missing the type and forward calls.

		static int from(lua_State * state, int index, StackObject& out)
		{
			out = StackObject::fromStack(state, index);
			return 1;
		}

		static int to(lua_State * state, const StackObject& in)
		{
			assert(state = in.state());
			lua_pushvalue(state, in.index());
			return 1;
		}
	};

	namespace Detail 
	{
		template<typename T>
		struct IsPointerToNonprimitive
			: boost::mpl::and_<
				boost::is_pointer<T>, 
				boost::mpl::not_<typename Convert<T>::is_primitive>
			>
		{};

		template<typename T>
		struct IsNonprimitiveReference
			: boost::mpl::and_<
				boost::is_reference<T>,
				boost::mpl::not_<typename Convert<T>::is_primitive>
			>
		{};

		template<typename T>
		struct IsNonprimitiveValue  
			: boost::mpl::not_<
				boost::mpl::or_<
					typename Convert<T>::is_primitive, 
					boost::is_pointer<T>,
					boost::is_reference<T>
				>
			>
		{};
	}

	// Index cast is called more or less directly from cast, and has three major variants.
	// The first one casts non-pointer primitives, such as ints, floats, doubles and strings, which returns a value.
	// The second one converts pointer-to-nonprimitive type, mostly uservalues bound to lua. 
	// The third is basically the same as the second one, but with const.
	template<typename T>
	T indexCast(lua_State * s, int i, typename boost::enable_if<typename Convert<T>::is_primitive>::type * = nullptr)
	{
		T result;
		Convert<T>::from(s, i, result);
		return std::move(result);
	}

	template<typename T>
	T indexCast(lua_State *s, int i, typename boost::enable_if<Detail::IsPointerToNonprimitive<T>>::type * = nullptr)
	{
		T result = nullptr;
		Convert<T>::from(s, i, result);
		return result;
	}

	template<typename T>
	T indexCast(lua_State *s, int i, typename boost::enable_if<Detail::IsNonprimitiveReference<T>>::type * = nullptr)
	{
		typedef typename boost::remove_reference<T>::type base;
		base* result = nullptr;
		Convert<base*>::from(s, i, result);
		return *result;
	}

	template<typename T>
	T indexCast(lua_State *s, int i, typename boost::enable_if<Detail::IsNonprimitiveValue<T>>::type * = nullptr)
	{
		T* result = nullptr;
		Convert<T*>::from(s, i, result);
		return *result;
	}

	template<typename T>
	T cast(const Object& o) 
	{
		StackCheck check(o.state(), 1, 0);
		Convert<Object>::to(o.state(), o);
		return indexCast<T>(o.state(), -1);
	}

	template<typename T>
	T cast(const StackObject& o) 
	{
		return indexCast<T>(o.state(), o.index());
	}
/*
	template<typename T>
	T cast(const Object& o, typename boost::enable_if<typename Convert<T>::is_primitive>::type * = nullptr)
	{
		StackCheck check(o.state(), 1, 0);
		Convert<Object>::to(o.state(), o);
		return indexCast<T>(o.state(), -1);
	}

	template<typename T>
	T cast(const StackObject& o, typename boost::enable_if<typename Convert<T>::is_primitive>::type * = nullptr)
	{
		return indexCast<T>(o.state(), o.index());
	}

	template<typename T>
	T& cast(const Object& o, typename boost::disable_if<Detail::IsPointerTo
	{
		StackCheck check(o.state(), 1, 0);
		Convert<Object>::to(o.state(), o);
		return indexCast<T>(o.state(), -1);
	}

	template<typename T>
	T& cast(const StackObject& o, typename boost::disable_if<typename Convert<T>::is_primitive>::type * = nullptr)
	{
		return indexCast<T>(o.state(), o.index());
	}
*/

	namespace Detail
	{
		template<typename T, typename Obj>
		class ObjectProxy
		{
		public:
			ObjectProxy(Obj * obj, T key)
				:key(key)
				,object(obj)
			{}

			template<typename U, typename Enable = typename boost::disable_if<boost::is_same<U, StackObject>, void>::type>
			operator U() const
			{
				int pop = boost::is_same<Obj, StackObject>::value ? 1 : 2;
				StackCheck check(object->state(), pop, 0);

				int type = object->stackPush(key);
				return indexCast<U>(object->state(), -1);
			}

			operator Object() const
			{
				int pop = boost::is_same<Obj, StackObject>::value ? 1 : 2;

				StackCheck check(object->state(), pop, 0);
				object->stackPush(key);
				return Object::fromStack(object->state(), -1);
			}

			template<typename U>
			typename boost::disable_if<
				boost::is_same<U, StackObject>,
				ObjectProxy&>::type
			operator=(const U& u)
			{
				StackCheck check(object->state(), 0, 0);
				object->stackSet(key, u);

				return *this;
			}

			ObjectProxy& operator=(const ObjectProxy& other)
			{
				int pop = boost::is_same<Obj, StackObject>::value ? 1 : 2;
				StackCheck check(object->state(), pop, 0);
				other.object->stackPush(other.key);

				object->stackSet(key, StackObject(other.object->state(), -1));
				return *this;
			}
		private:
			T key;
			Obj * object;
		};

		class Iterator
		{
		public:
			explicit Iterator(Object * o);
			Iterator();

			Iterator& operator++();
			Iterator operator++(int);

			Object operator*();
			Object key();

			bool operator==(const Iterator& other);
			bool operator!=(const Iterator& other);
		private:
			Object * o;

			Object currentIndex;
			Object value;
			int hasNext;
			int increments;
		};

		class StackIterator
		{
		public:
			explicit StackIterator(StackObject * o);
			StackIterator();

			StackIterator& operator++();
			StackIterator operator++(int);

			StackObject operator*();
			StackObject key();

			bool operator==(const StackIterator& other);
			bool operator!=(const StackIterator& other);
		private:
			StackObject * o;

			int currentIndex;
			int value;
			int hasNext;
			int increments;
		};
	}

	Object newtable(lua_State * state);
	Object globals(lua_State * state);
	Object compile(lua_State * state, const char * data);
}
