#pragma once
#include <vector>
#include <lua.hpp>

namespace LBind
{
	namespace Detail
	{
		struct FunctionBase;

		class InternalState
		{
		public:
			~InternalState();

			void * allocate(size_t bytes);
			void registerFunction(FunctionBase *);
		private:
			std::vector<void *> allocations;
			std::vector<FunctionBase *> registeredFunctions;
		};

		InternalState * getInternalState(lua_State *);
	}
}