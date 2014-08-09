#pragma once

#include <unordered_map>
#include <unordered_set>
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class PrecompiledMethodInfo;

class RecompilationManager {
public:

  void addCalleeCallerRelation(ID callee, PrecompiledMethodInfo* caller);
  std::unordered_set<PrecompiledMethodInfo*>* callerList(ID callee);

  void addConstantReferrer(ID constant, PrecompiledMethodInfo* referrer);
  std::unordered_set<PrecompiledMethodInfo*>* constantReferrerList(ID constant);

  void invalidateCompiledCodeByName(ID name);
  void removeMethodInfoFromMethodEntry(mri::MethodEntry me);
  void invalidateCompiledCodeByConstantRedefinition(ID name);

  static RecompilationManager* instance();

private:

  static void invalidateMethod(PrecompiledMethodInfo* mi);

  std::unordered_map<ID, std::unordered_set<PrecompiledMethodInfo*>> calleeCallerMap_;
  std::unordered_map<ID, std::unordered_set<PrecompiledMethodInfo*>> constantReferrerMap_;

};

RBJIT_NAMESPACE_END
