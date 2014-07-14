#include "rbjit/recompilationmanager.h"
#include "rbjit/methodinfo.h"
#include "rbjit/rubymethod.h"

extern "C" {
void rb_raise(VALUE, const char*, ...);
VALUE rb_eRuntimeError;
}

RBJIT_NAMESPACE_BEGIN

void
RecompilationManager::addCalleeCallerRelation(ID callee, PrecompiledMethodInfo* caller)
{
  auto i = calleeCallerMap_.find(callee);
  if (i == calleeCallerMap_.end()) {
    std::unordered_set<PrecompiledMethodInfo*> s;
    s.insert(caller);
    calleeCallerMap_.insert(std::make_pair(callee, std::move(s)));
  }
  else {
    i->second.insert(caller);
  }
}

std::unordered_set<PrecompiledMethodInfo*>*
RecompilationManager::callerList(ID callee)
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

void
RecompilationManager::notifyMethodRedefined(ID name)
{
  std::unordered_set<PrecompiledMethodInfo*>* callers = callerList(name);
  if (!callers) {
    return;
  }

  for (auto i = callers->cbegin(), end = callers->cend(); i != end; ++i) {
    (*i)->restoreISeqDefinition();
  }
  callers->clear();
}

extern "C" void
rbjit_notifyMethodRedefined(ID name)
{
  RecompilationManager::instance()->notifyMethodRedefined(name);
}

RBJIT_NAMESPACE_END
