#pragma once

#include <unordered_set>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

namespace llvm {
class Module;
}

RBJIT_NAMESPACE_BEGIN

class NativeCompiler;

class PrimitiveStore {
public:

  void load(void** pAddress, size_t* size);
  void buildLookupMap(llvm::Module* module);

  bool isPrimitive(ID name) const;

  static void setup();
  static PrimitiveStore* instance() { return instance_; }

private:

  std::unordered_set<ID> names_;

  static PrimitiveStore* instance_;
};

RBJIT_NAMESPACE_END
