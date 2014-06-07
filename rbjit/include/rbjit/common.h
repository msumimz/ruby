#pragma once
#include <cassert>

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
