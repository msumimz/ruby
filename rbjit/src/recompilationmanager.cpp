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

void
RecompilationManager::addConstantReferrer(ID constant, PrecompiledMethodInfo* referrer)
{
  auto i = constantReferrerMap_.find(constant);
  if (i == constantReferrerMap_.end()) {
    std::unordered_set<PrecompiledMethodInfo*> s;
    s.insert(referrer);
    constantReferrerMap_.insert(std::make_pair(constant, std::move(s)));
  }
  else {
    i->second.insert(referrer);
  }
  referrer->addRef();
}

std::unordered_set<PrecompiledMethodInfo*>*
RecompilationManager::constantReferrerList(ID constant)
{
  auto i = constantReferrerMap_.find(constant);
  if (i != constantReferrerMap_.end()) {
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
RecompilationManager::invalidateMethod(PrecompiledMethodInfo* mi)
{
  if (mi->isActive()) {
    mi->restoreISeqDefinition();
    if (mi->isJitOnly()) {
      mi->compile();
    }
  }
  mi->release();
}

void
RecompilationManager::invalidateCompiledCodeByName(ID name)
{
  std::unordered_set<PrecompiledMethodInfo*>* callers = callerList(name);
  if (!callers) {
    return;
  }

  for (auto i = callers->cbegin(), end = callers->cend(); i != end; ++i) {
    invalidateMethod(*i);
  }
  callers->clear();
}

void
RecompilationManager::invalidateCompiledCodeByConstantRedefinition(ID name)
{
  std::unordered_set<PrecompiledMethodInfo*>* referrers = constantReferrerList(name);
  if (!referrers) {
    return;
  }

  for (auto i = referrers->cbegin(), end = referrers->cend(); i != end; ++i) {
    invalidateMethod(*i);
  }
  referrers->clear();
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

void
rbjit_invalidateCompiledCodeByConstantRedefinition(ID name)
{
  RecompilationManager::instance()->invalidateCompiledCodeByConstantRedefinition(name);
}

}

RBJIT_NAMESPACE_END
