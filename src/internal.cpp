#include "internal.hpp"
#include "function.hpp"

#include <memory>

namespace lbind
{
	namespace Detail
	{
		InternalState::~InternalState()
		{
			for (size_t i = 0; i < allocations.size(); ++i)
			{
				free(allocations[i]);
			}

			for (size_t i = 0; i < registeredFunctions.size(); ++i)
			{
				delete registeredFunctions[i];
			}
		}

		void InternalState::registerFunction(FunctionBase * b)
		{
			registeredFunctions.push_back(b);
		}

		void * InternalState::allocate(size_t bytes)
		{
			void * result = malloc(bytes);
			allocations.push_back(result);

			return result;
		}

		InternalState * getInternalState(lua_State * s)
		{
			return *reinterpret_cast<InternalState **>(lua_getextraspace(s));
		}
	}
}