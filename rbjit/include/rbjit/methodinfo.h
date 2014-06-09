#pragma once
#include "rbjit/common.h"
#include "rbjit/rubyobject.h"

RBJIT_NAMESPACE_BEGIN

class TypeConstraint;
class ControlFlowGraph;
class TypeContext;

class MethodInfo {
public:

  enum { UNKNOWN = 0, YES = 1, NO = 2 };

  MethodInfo()
    : cls_(), hasDef_(UNKNOWN), hasEval_(UNKNOWN), returnType_(0)
  {}

  MethodInfo(mri::Class cls)
    : cls_(cls), hasDef_(UNKNOWN), hasEval_(UNKNOWN), returnType_(0)
  {}

  MethodInfo(mri::Class cls, unsigned hasDef, unsigned hasEval, TypeConstraint* returnType)
    : cls_(cls), hasDef_(hasDef), hasEval_(hasEval), returnType_(returnType)
  {}

  virtual ~MethodInfo() {}

  mri::Class class_() const { return cls_; }

  unsigned hasDef() const { return hasDef_; }
  unsigned hasEval() const { return hasEval_; }

  virtual TypeConstraint* returnType() { return returnType_; }

  void setHasDef(int state) { hasDef_ = state; }
  void setHasEval(int state) { hasEval_ = state; }

  bool isMutator() const { return hasDef_ != NO || hasEval_ != NO; }

  static void addToNativeMethod(mri::Class cls, const char* methodName,
    unsigned hasDef, unsigned hasEval, TypeConstraint* returnType);

protected:

  mri::Class cls_;

  unsigned hasDef_ : 2;
  unsigned hasEval_ : 2;

  TypeConstraint* returnType_;

};

class PrecompiledMethodInfo : public MethodInfo {
public:

  PrecompiledMethodInfo(mri::Class cls, RNode* node, const char* methodName)
    : MethodInfo(cls), node_(node), methodName_(methodName), cfg_(0), methodBody_(0),
      lock_(false)
  {}

  ControlFlowGraph* cfg() const { return cfg_; }
  void* methodBody() { return methodBody_; }

  void buildCfg();
  void analyzeTypes();
  void compile();

  TypeConstraint* returnType();

  static PrecompiledMethodInfo* addToExistingMethod(mri::Class cls, ID methodName);

private:

  RNode* node_; // method definition
  const char* methodName_;
  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;

  void* methodBody_; // precompiled code

  bool lock_;
};

RBJIT_NAMESPACE_END
