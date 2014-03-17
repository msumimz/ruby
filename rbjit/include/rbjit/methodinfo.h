#pragma once
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

class TypeConstraint;
class ControlFlowGraph;

class MethodInfo {
public:

  enum { UNKNOWN = 0, YES = 1, NO = 2 };

  MethodInfo()
    : hasDef_(UNKNOWN), hasEval_(UNKNOWN), returnType_(0)
  {}

  MethodInfo(unsigned hasDef, unsigned hasEval, TypeConstraint* returnType)
    : hasDef_(hasDef), hasEval_(hasEval), returnType_(returnType)
  {}

  unsigned hasDef() const { return hasDef_; }
  unsigned hasEval() const { return hasEval_; }
  TypeConstraint* returnType() const { return returnType_; }

  void setHasDef(int state) { hasDef_ = state; }
  void setHasEval(int state) { hasEval_ = state; }

  bool isMutator() const { return hasDef_ != NO || hasEval_ != NO; }

  static void addToNativeMethod(mri::Class cls, const char* methodName,
    unsigned hasDef, unsigned hasEval, TypeConstraint* returnType);

private:

  unsigned hasDef_ : 2;
  unsigned hasEval_ : 2;

  TypeConstraint* returnType_;

};

class PrecompiledMethodInfo : public MethodInfo {
public:

  PrecompiledMethodInfo(RNode* node, const char* methodName)
    : MethodInfo(), node_(node), methodName_(methodName), cfg_(0), methodBody_(0)
  {}

  void* methodBody() { return methodBody_; }

  void compile();

private:

  RNode* node_;
  const char* methodName_;
  ControlFlowGraph* cfg_;

  void* methodBody_;

};

RBJIT_NAMESPACE_END
