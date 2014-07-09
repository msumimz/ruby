#pragma once

#include <unordered_map>
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

class PrecompiledMethodInfo;

class RecompilationManager {
public:

  void addCalleeCallerRelation(ID callee, PrecompiledMethodInfo* caller);
  const std::vector<PrecompiledMethodInfo*>* callerList(ID callee) const;

  static RecompilationManager* instance();

private:

  std::unordered_map<ID, std::vector<PrecompiledMethodInfo*>> calleeCallerMap_;

};

RBJIT_NAMESPACE_END
