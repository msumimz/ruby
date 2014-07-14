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
  caller->addRef();
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
RecompilationManager::invalidateCompiledCodeByName(ID name)
{
  std::unordered_set<PrecompiledMethodInfo*>* callers = callerList(name);
  if (!callers) {
    return;
  }

  for (auto i = callers->cbegin(), end = callers->cend(); i != end; ++i) {
    PrecompiledMethodInfo* mi = *i;
    if (mi->isActive()) {
      mi->restoreISeqDefinition();
      if (mi->isJitOnly()) {
        mi->compile();
      }
    }
    mi->release();
  }
  callers->clear();
}

extern "C" {

void
rbjit_invalidateCompiledCodeByName(ID name)
{
  RecompilationManager::instance()->invalidateCompiledCodeByName(name);
}

void
rbjit_removeMethodInfoFromMethodEntry(rb_method_entry_t* me)
{
  MethodInfo* mi = mri::MethodEntry(me).methodInfo();
  if (mi) {
    mi->detachMethodEntry();
    mri::MethodEntry(me).setMethodInfo(nullptr);
  }
}

}

RBJIT_NAMESPACE_END
