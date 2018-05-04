#include "exceptions.hpp"

namespace lbind
{
	BadCast::BadCast(const char * what)
		:std::runtime_error(what)
	{}

	BindingError::BindingError(const char * what)
		:std::runtime_error(what)
	{}
}