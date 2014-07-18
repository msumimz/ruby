#pragma once

#include <unordered_set>
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class MutatorTester {
public:

  bool isMutator(mri::MethodEntry me);
  void addMutatorAlias(ID name);

  static MutatorTester* instance() { static MutatorTester m; return &m; }

private:

  MutatorTester();

  std::unordered_set<ID> mutators_;
};

RBJIT_NAMESPACE_END
