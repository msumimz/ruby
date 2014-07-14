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

  void invalidateCompiledCodeByName(ID name);
  void removeMethodInfoFromMethodEntry(mri::MethodEntry me);

  static RecompilationManager* instance();
  static VALUE recompile(mri::MethodDefinition::CFunc func, VALUE recv, int argc, const VALUE *argv);

private:

  std::unordered_map<ID, std::unordered_set<PrecompiledMethodInfo*>> calleeCallerMap_;

};

RBJIT_NAMESPACE_END
