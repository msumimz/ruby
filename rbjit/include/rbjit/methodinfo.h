#pragma once
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class TypeConstraint;
class ControlFlowGraph;
class TypeContext;

class MethodInfo {
public:

  MethodInfo(mri::MethodEntry me) : me_(me) {}

  virtual ~MethodInfo() {}

  mri::MethodEntry methodEntry() const { return me_; }
  mri::Class class_() const { return me_.class_(); }
  ID methodName() const { return me_.methodName(); }

  virtual TypeConstraint* returnType() = 0;
  virtual RNode* astNode() const = 0;
  virtual bool isMutator() = 0;

private:

  mri::MethodEntry me_;

};

////////////////////////////////////////////////////////////
// CMethodInfo

class CMethodInfo : public MethodInfo {
public:

  CMethodInfo(mri::MethodEntry me, bool mutator, TypeConstraint* returnType)
    : MethodInfo(me), mutator_(mutator), returnType_(returnType)
  {}

  ~CMethodInfo();

  static CMethodInfo* construct(mri::Class cls, const char* methodName,
    bool mutator, TypeConstraint* returnType);

  virtual TypeConstraint* returnType() { return returnType_; }
  virtual RNode* astNode() const { return 0; }
  virtual bool isMutator() { return mutator_; }

protected:

  bool mutator_;
  TypeConstraint* returnType_;

};

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

class PrecompiledMethodInfo : public MethodInfo {
public:

  PrecompiledMethodInfo(mri::MethodEntry me)
    : MethodInfo(me), cfg_(0), typeContext_(0), returnType_(0),
      origDef_(), lock_(false)
  {}

  ~PrecompiledMethodInfo();

  static PrecompiledMethodInfo* construct(mri::MethodEntry me);
  static PrecompiledMethodInfo* construct(mri::Class cls, ID methodName);

  TypeConstraint* returnType();
  RNode* astNode() const;
  bool isMutator();

  ControlFlowGraph* cfg() { return cfg_; }
  TypeContext* typeContext() { return typeContext_; }

  void compile();

  std::string debugPrintBanner(const char* stage) const;
  std::string debugPrintAst() const;

private:

  void buildCfg();
  void analyzeTypes();
  void* generateCode();

  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;
  TypeConstraint* returnType_;
  mri::MethodDefinition origDef_;

  bool lock_;
};

RBJIT_NAMESPACE_END
