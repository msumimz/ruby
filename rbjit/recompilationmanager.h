#pragma once

#include <unordered_map>
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

extern "C" void rbjit_notifyMethodRedefined(ID name);

RBJIT_NAMESPACE_BEGIN

class PrecompiledMethodInfo;

class RecompilationManager {
public:

  void addCalleeCallerRelation(ID callee, PrecompiledMethodInfo* caller);
  std::vector<PrecompiledMethodInfo*>* callerList(ID callee);

  void notifyMethodRedefined(ID name);

  static RecompilationManager* instance();
  static VALUE recompile(mri::MethodDefinition::CFunc func, VALUE recv, int argc, const VALUE *argv);

private:

  void addMethodInfo(void* func, PrecompiledMethodInfo* mi);
  PrecompiledMethodInfo* findMethodInfo(void* func) const;
  void removeMethodInfo(void* func);

  std::unordered_map<ID, std::vector<PrecompiledMethodInfo*>> calleeCallerMap_;
  std::unordered_map<void*, PrecompiledMethodInfo*> methodInfoMap_;

};

RBJIT_NAMESPACE_END
