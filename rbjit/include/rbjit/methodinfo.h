#pragma once
#include <unordered_set>
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class TypeConstraint;
class ControlFlowGraph;
class TypeContext;

class MethodInfo {
public:

  // Reference count
  void addRef() { ++refCount_; }
  void release() { if (--refCount_ == 0) { assert(!active_); delete this; } }

  // Activeness
  bool isActive() const { return active_; }
  void detachMethodEntry() { active_ = false; release(); }

  // MethodEntry
  mri::MethodEntry methodEntry() const { return me_; }
  mri::Class class_() const { return me_.class_(); }
  ID methodName() const { return me_.methodName(); }


  virtual TypeConstraint* returnType() = 0;
  virtual RNode* astNode() const = 0;

  virtual bool isMutator() = 0;
  virtual bool isJitOnly() = 0;

protected:

  MethodInfo(mri::MethodEntry me)
    : refCount_(1), active_(!me.isNull()), me_(me) {}
  virtual ~MethodInfo() {}

  void* operator new(size_t s) { return ::operator new(s); }
  void operator delete(void* p) { ::delete p; }

private:

  // reference count: a MethodInfo is referred to by RecompliationManager's
  // callee-caller map in multiple times Thus, to avoid dangling pointers keep
  // a reference count.
  int refCount_;

  // True when a MethodInfo has a rb_method_entry_t instance associated with it.
  bool active_;

  mri::MethodEntry me_;

};

////////////////////////////////////////////////////////////
// CMethodInfo

class CMethodInfo : public MethodInfo {
public:

  static CMethodInfo* construct(mri::Class cls, const char* methodName,
    bool mutator, TypeConstraint* returnType);

  TypeConstraint* returnType() { return returnType_; }
  RNode* astNode() const { return 0; }

  bool isMutator() { return mutator_; }
  bool isJitOnly() { return false; }

private:

  CMethodInfo(mri::MethodEntry me, bool mutator, TypeConstraint* returnType)
    : MethodInfo(me), mutator_(mutator), returnType_(returnType)
  {}

  ~CMethodInfo();

  TypeConstraint* returnType_;
  bool mutator_;

};

////////////////////////////////////////////////////////////
// PrecompiledMethodInfo

class PrecompiledMethodInfo : public MethodInfo {
public:

  static PrecompiledMethodInfo* construct(mri::MethodEntry me);
  static PrecompiledMethodInfo* construct(mri::Class cls, ID methodName);

  TypeConstraint* returnType();
  RNode* astNode() const;
  bool isMutator();
  bool isJitOnly();

  ControlFlowGraph* cfg() { return cfg_; }
  TypeContext* typeContext() { return typeContext_; }

  void compile();
  void restoreISeqDefinition();

  // Debugging methods
  std::string debugPrintBanner(const char* stage) const;
  std::string debugPrintAst() const;

private:

  PrecompiledMethodInfo(mri::MethodEntry me)
    : MethodInfo(me), cfg_(0), typeContext_(0), returnType_(0),
      mutator_(UNKNOWN), jitOnly_(UNKNOWN), origDef_(), origCfg_(0),
      lock_(false)
  {}

  ~PrecompiledMethodInfo();

  enum State { UNKNOWN, YES, NO };

  void buildCfg();
  void analyzeTypes();
  void* generateCode();

  ControlFlowGraph* cfg_;
  TypeContext* typeContext_;
  TypeConstraint* returnType_;
  State mutator_;
  State jitOnly_;

  mri::MethodDefinition origDef_;
  ControlFlowGraph* origCfg_;

  bool lock_;
};

RBJIT_NAMESPACE_END
