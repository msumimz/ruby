#pragma once

// namespace
#define RBJIT_NAMESPACE_BEGIN namespace rbjit {
#define RBJIT_NAMESPACE_END   }

RBJIT_NAMESPACE_BEGIN

namespace mri {
typedef size_t VALUE;
typedef size_t ID;
}

RBJIT_NAMESPACE_END

// MRI's node implementation
extern "C" {
struct RNode;
}
