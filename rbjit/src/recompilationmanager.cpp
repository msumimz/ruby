#include "rbjit/recompilationmanager.h"

RBJIT_NAMESPACE_BEGIN

void
RecompilationManager::addCalleeCallerRelation(ID callee, PrecompiledMethodInfo* caller)
{
  auto i = calleeCallerMap_.find(callee);
  if (i == calleeCallerMap_.end()) {
    calleeCallerMap_.insert(make_pair(callee, std::vector<PrecompiledMethodInfo*>(1, caller)));
  }
  else {
    i->second.push_back(caller);
  }
}

const std::vector<PrecompiledMethodInfo*>*
RecompilationManager::callerList(ID callee) const
{
  auto i = calleeCallerMap_.find(callee);
  if (i != calleeCallerMap_.end()) {
    return &(i->second);
  }
  return nullptr;
}

RecompilationManager*
RecompilationManager::instance()
{
  static RecompilationManager m;
  return &m;
}

RBJIT_NAMESPACE_END
