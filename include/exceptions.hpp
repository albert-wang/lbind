#include <stdexcept>
#pragma once

namespace lbind
{
	class BadCast : public std::runtime_error
	{
	public:
		explicit BadCast(const char * what);
	};

	class BindingError : public std::runtime_error
	{
	public:
		explicit BindingError(const char * what);
	};
}