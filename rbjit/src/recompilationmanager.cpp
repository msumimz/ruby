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
    calleeCallerMap_.insert(make_pair(callee, std::vector<PrecompiledMethodInfo*>(1, caller)));
  }
  else {
    i->second.push_back(caller);
  }
}

std::vector<PrecompiledMethodInfo*>*
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
RecompilationManager::addMethodInfo(void* func, PrecompiledMethodInfo* mi)
{
  methodInfoMap_[func] = mi;
}

PrecompiledMethodInfo*
RecompilationManager::findMethodInfo(void* func) const
{
  auto f = methodInfoMap_.find(func);
  assert(f == methodInfoMap_.end());
  return f->second;
}

void
RecompilationManager::removeMethodInfo(void* func)
{
  methodInfoMap_.erase(func);
}

VALUE
RecompilationManager::recompile(mri::MethodDefinition::CFunc func, VALUE recv, int argc, const VALUE *argv)
{
  RecompilationManager* rm = RecompilationManager::instance();
  PrecompiledMethodInfo* mi = rm->findMethodInfo(func);

  if (!mi) {
    rb_raise(rb_eRuntimeError, "[BUG] failed to recompile JIT-compiled method");
  }
  mi->recompile();
  mri::MethodDefinition::Invoker invoker = mi->methodEntry().methodDefinition().defaultCFuncInvoker();
  mi->methodEntry().methodDefinition().setCFuncInvoker(invoker);

  rm->removeMethodInfo(func);

  return invoker(func, recv, argc, argv);
}

void
RecompilationManager::notifyMethodRedefined(ID name)
{
  std::vector<PrecompiledMethodInfo*>* callers = callerList(name);
  if (!callers) {
    return;
  }

  for (auto i = callers->cbegin(), end = callers->cend(); i != end; ++i) {
    PrecompiledMethodInfo* mi = *i;
    mri::MethodDefinition def = mi->methodEntry().methodDefinition();
    def.setCFuncInvoker(static_cast<mri::MethodDefinition::Invoker>(recompile));
    addMethodInfo(static_cast<void*>(def.cFunc()), mi);
  }
  callers->clear();
}

extern "C" {

void
rbjit_notifyMethodRedefined(ID name)
{
  RecompilationManager::instance()->notifyMethodRedefined(name);
}

}

RBJIT_NAMESPACE_END
