#pragma once
#include <cassert>

// namespace
#define RBJIT_NAMESPACE_BEGIN namespace rbjit {
#define RBJIT_NAMESPACE_END   }

// Unreachable code path
#ifdef RBJIT_DEBUG
# define RBJIT_UNREACHABLE assert(!"Unreachable code path")
#else
# define RBJIT_UNREACHABLE __assume(false)
#endif
