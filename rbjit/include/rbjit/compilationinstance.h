#pragma once
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class TypeConstraint;
class TypeContext;
class ControlFlowGraph;
class PrecompiledMethodInfo;
class Scope;

class CompilationInstance {
public:

  CompilationInstance(mri::Class holderClass, ID name, RNode* source, mri::CRef cref);
  ~CompilationInstance();

  void reset();

  mri::Class holderClass() const { return holderClass_; }
  ID name() const { return name_; }
  const RNode* source() const { return source_; }

  void* generateCode(PrecompiledMethodInfo* mi);

  ControlFlowGraph* cfg();
  Scope* scope();
  TypeContext* typeContext();
  TypeConstraint* returnType();
  bool isMutator();
  bool isJitOnly();

  // Debugging methods
  std::string debugPrintBanner(const char* stage) const;
  std::string debugPrintAst() const;

private:

  enum State { UNKNOWN, YES, NO };

  void buildCfg();
  void analyzeTypes();

  mri::Class holderClass_;
  ID name_;
  RNode* source_;
  mri::CRef cref_;

  ControlFlowGraph* cfg_;
  ControlFlowGraph* origCfg_;
  Scope* scope_;
  Scope* origScope_;
  TypeContext* typeContext_;
  TypeConstraint* returnType_;

  State mutator_;
  State jitOnly_;

  bool lock_;
};

RBJIT_NAMESPACE_END
