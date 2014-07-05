#pragma once

#include <cassert>
#include <string>

// namespace
#define RBJIT_NAMESPACE_BEGIN namespace rbjit {
#define RBJIT_NAMESPACE_END   }

// Assertion
#ifdef RBJIT_DEBUG
# define RBJIT_ASSERT(c) assert(c)
# define RBJIT_ASSUME(c) assert(c)
#else
# define RBJIT_ASSERT(c)
# define RBJIT_ASSUME(c) __assume(c)
#endif

// Unreachable code path
#define RBJIT_UNREACHABLE RBJIT_ASSUME(!"Unreachable code path")

RBJIT_NAMESPACE_BEGIN

// Safe sprintf-like functions
std::string stringFormat(const char* format, ...);
std::string stringFormatVarargs(const char* format, va_list args);

RBJIT_NAMESPACE_END
