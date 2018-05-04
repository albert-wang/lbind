#include "classes.hpp"

namespace lbind
{
	namespace Detail
	{
		void * ownership(void * ptr, boost::uint8_t vals)
		{
			assert((vals | 0x03) == 0x03);

			return (void *)(((boost::intptr_t)ptr) | vals);
		}

		boost::uint8_t ownership(void * ptr)
		{
			return ((boost::intptr_t)ptr) & 0x03;
		}

		void * ownershipless(void * ptr)
		{
			return (void *)(((boost::intptr_t)ptr) & ~boost::intptr_t(0x03));
		}
	}
}