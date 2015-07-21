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

	struct Statistics
	{
		size_t converts;
		size_t luaToC;
		size_t cToLua;
	};

	//Passing in null gets global statistics
	const Statistics& getStatistics(lua_State *);
}