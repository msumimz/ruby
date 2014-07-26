#pragma once

#include <cassert>
#ifdef RBJIT_DEBUG // used in BlockHeader
#include <string>
#endif
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

RBJIT_NAMESPACE_BEGIN

class Variable;
class BlockHeader;
class OpcodeCopy;
class OpcodeJump;
class OpcodeJumpIf;
class OpcodeImmediate;
class OpcodeEnv;
class OpcodeLookup;
class OpcodeCall;
class OpcodeConstant;
class OpcodePrimitive;
class OpcodePhi;
class OpcodeExit;

////////////////////////////////////////////////////////////
// Visitor interface

class OpcodeVisitor {
public:
  virtual bool visitOpcode(BlockHeader* op) = 0;
  virtual bool visitOpcode(OpcodeCopy* op) = 0;
  virtual bool visitOpcode(OpcodeJump* op) = 0;
  virtual bool visitOpcode(OpcodeJumpIf* op) = 0;
  virtual bool visitOpcode(OpcodeImmediate* op) = 0;
  virtual bool visitOpcode(OpcodeEnv* op) = 0;
  virtual bool visitOpcode(OpcodeLookup* op) = 0;
  virtual bool visitOpcode(OpcodeCall* op) = 0;
  virtual bool visitOpcode(OpcodeConstant* op) = 0;
  virtual bool visitOpcode(OpcodePrimitive* op) = 0;
  virtual bool visitOpcode(OpcodePhi* op) = 0;
  virtual bool visitOpcode(OpcodeExit* op) = 0;
};

////////////////////////////////////////////////////////////
// Base abstract class

class Opcode {
public:

  Opcode(int file, int line, Opcode* prev)
    : file_(file), line_(line), prev_(prev), next_(0)
  {
    if (prev) {
      next_ = prev->next_;
      prev->next_ = this;
    }
  }

  // To be overridden by subclasses.
  // Even when overridden, contained variables are NOT released.
  virtual ~Opcode() {}

  virtual Opcode* clone(Opcode* prev) const
  { assert(!"clone() is not implemented"); return 0; }

  Opcode* next() const { return next_; }
  Opcode* prev() const { return prev_; }

  void
  linkToNext(Opcode* next)
  {
    next_ = next;
    next->prev_ = this;
  }

  void
  insertAfter(Opcode* prev)
  {
    prev_ = prev;
    next_ = prev->next_;
    prev->next_->prev_ = this;
    prev->next_ = this;
  }

  void
  unlink()
  {
    if (prev_) { prev_->next_ = next_; }
    if (next_) { next_->prev_ = prev_; }
  }

  // Obtain variables
  virtual Variable* lhs() const { return 0; }
  virtual Variable*const* rhsBegin() const { return 0; }
  virtual Variable*const* rhsEnd() const { return 0; }
  virtual Variable** rhsBegin() { return 0; }
  virtual Variable** rhsEnd() { return 0; }
  virtual BlockHeader* nextBlock() const { return 0; }
  virtual BlockHeader* nextAltBlock() const { return 0; }

  int rhsCount() const { return rhsEnd() - rhsBegin(); }
  bool hasRhs() const { return rhsCount() > 0; }

  virtual int file() const { return 0; }
  virtual int line() const { return 0; }

  virtual bool accept(OpcodeVisitor* visitor) = 0;

  virtual bool isTerminator() const { return false; }

  const char* typeName() const;
  const char* shortTypeName() const;

  virtual Variable* outEnv() { return 0; }
  virtual void setOutEnv(Variable* e) {}

protected:

  int file_ : 8;
  int line_ : 16;
  Opcode* prev_;
  Opcode* next_;
};

////////////////////////////////////////////////////////////
// Helper classes

// Opcode that has one lhs variable
class OpcodeL : public Opcode {
public:

  OpcodeL(int file, int line, Opcode* prev, Variable* lhs)
    : Opcode(file, line, prev), lhs_(lhs)
  {}

  Variable* lhs() const { return lhs_; }
  void setLhs(Variable* lhs) { lhs_ = lhs; }

protected:

  Variable* lhs_;
};

// Opcode that has N of rhs
template <int N>
class OpcodeR : public Opcode {
public:

  OpcodeR(int file, int line, Opcode* prev)
    : Opcode(file, line, prev)
  {}

  Variable* rhs() const { return rhs_[0]; }
  Variable*const* rhsBegin() const { return rhs_; }
  Variable*const* rhsEnd() const { return rhs_ + N; }
  Variable** rhsBegin() { return rhs_; }
  Variable** rhsEnd() { return rhs_ + N; }

  void setRhs(int i, Variable* v) { rhs_[i] = v; }

protected:

  Variable* rhs_[N];
};

// Opcode that has one lhs and N of rhs
template <int N>
class OpcodeLR : public OpcodeL {
public:

  OpcodeLR(int file, int line, Opcode* prev, Variable* lhs)
    : OpcodeL(file, line, prev, lhs)
  {}

  Variable* rhs() const { return rhs_[0]; }
  Variable* rhs(int i) const { return rhs_[i]; }

  Variable*const* rhsBegin() const { return rhs_; }
  Variable*const* rhsEnd() const { return rhs_ + N; }
  Variable** rhsBegin() { return rhs_; }
  Variable** rhsEnd() { return rhs_ + N; }

  void setRhs(int i, Variable* v) { rhs_[i] = v; }

protected:

  Variable* rhs_[N];
};

// Opcode that has one lhs and rhs of any length
class OpcodeVa : public OpcodeL {
public:

  OpcodeVa(int file, int line, Opcode* prev, Variable* lhs, int rhsSize)
    : OpcodeL(file, line, prev, lhs), rhsSize_(rhsSize), rhs_(new Variable*[rhsSize])
  {}

  ~OpcodeVa() { delete rhs_; }

  Variable* rhs() const { return rhs_[0]; }
  Variable* rhs(int i) const { return rhs_[i]; }

  void setRhs(int i, Variable* v) { rhs_[i] = v; }

  int rhsSize() const { return rhsSize_;  }
  Variable*const* rhsBegin() const { return rhs_; }
  Variable*const* rhsEnd() const { return rhs_ + rhsSize_; }
  Variable** rhsBegin() { return rhs_; }
  Variable** rhsEnd() { return rhs_ + rhsSize_; }

protected:

  int rhsSize_;
  Variable** rhs_;
};

////////////////////////////////////////////////////////////
// Opcodes

class BlockHeader : public Opcode {
public:

  BlockHeader(int file, int line, Opcode* prev, BlockHeader* backedge, int index, int depth, BlockHeader* idom)
    : Opcode(file, line, prev), backedge_(backedge, 0), footer_(0),
      index_(index), depth_(depth), idom_(idom), rescueBlock_(0)
  {}

  ~BlockHeader();

  BlockHeader* clone(int baseDepth)
  { return new BlockHeader(file_, line_, 0, 0, index_, depth_ + baseDepth, 0); }

  int index() const { return index_; }
  int depth() const { return depth_; }
  BlockHeader* idom() const { return idom_; }
  void setIdom(BlockHeader* idom) { idom_ = idom; }

  Opcode* footer() const { return footer_; }
  void setFooter(Opcode* footer) { footer_ = footer; }

  BlockHeader* nextBlock() const { assert(footer()); return footer()->nextBlock(); }
  BlockHeader* nextAltBlock() const { assert(footer()); return footer()->nextAltBlock(); }

  void updateJumpDestination(BlockHeader* block);
  void updateJumpAltDestination(BlockHeader* block);

  bool containsOpcode(const Opcode* op);

  // backedges

  class Backedge {
  public:

    Backedge(BlockHeader* block, Backedge* next)
      : block_(block), next_(next)
    {}

    BlockHeader* block() const { return block_; }
    Backedge* next() const { return next_; }

  private:

    friend class BlockHeader;
    BlockHeader* block_;
    Backedge* next_;
  };

  Backedge* backedge() { return &backedge_; }
  const Backedge* backedge() const { return &backedge_; }

  void addBackedge(BlockHeader* block);
  void removeBackedge(BlockHeader* block);
  void updateBackedge(BlockHeader* oldBlock, BlockHeader* newBlock);
  bool hasBackedge() const { return backedge_.block_ != 0; }
  bool hasMultipleBackedges() const { return backedge_.next_ != 0; }
  bool containsBackedge(BlockHeader* block);
  int backedgeIndexOf(BlockHeader* block);
  int backedgeSize() const;
  const Backedge* backedgeAt(int n) const;

  // visitor
  bool visitEachOpcode(OpcodeVisitor* visitor);
  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

  // intrinsic iterator
  template <class T> void
  forEachOpcode(T func)
  {
    Opcode* op = this;
    do {
      func()(op);
      if (!result || op == footer_) {
        break;
      }
      op = op->next();
    } while (op);
  }

  // debug tools
#ifdef RBJIT_DEBUG
  void setDebugName(const char* name)
  { debugName_ = std::string(name); }
  const char* debugName() const { return debugName_.c_str(); }
#else
  void setDebugName(const char* name) {}
  const char* debugName() const { return ""; }
#endif

private:

  Backedge backedge_;
  Opcode* footer_;

  int index_;
  int depth_;

  BlockHeader* idom_;
  BlockHeader* rescueBlock_;

#ifdef RBJIT_DEBUG
  std::string debugName_;
#endif
};

class OpcodeCopy : public OpcodeLR<1> {
public:

  OpcodeCopy(int file, int line, Opcode* prev, Variable* lhs, Variable* rhs)
    : OpcodeLR<1>(file, line, prev, lhs)
  { setRhs(0, rhs); }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

};

class OpcodeJump : public Opcode {
public:

  OpcodeJump(int file, int line, Opcode* prev, BlockHeader* dest)
    : Opcode(file, line, prev)
  { next_ = dest; }

  BlockHeader* dest() const { return static_cast<BlockHeader*>(next()); }
  BlockHeader* nextBlock() const { return dest(); }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

  bool isTerminator() const { return true; }

protected:

  // Allows BlockHeader to call setNextBlock(), which is used in updateJumpDestination()
  friend class BlockHeader;
  void setNextBlock(BlockHeader* dest) { next_ = dest; }

};

class OpcodeJumpIf : public OpcodeR<1> {
public:

  OpcodeJumpIf(int file, int line, Opcode* prev, Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse)
    : OpcodeR<1>(file, line, prev), ifFalse_(ifFalse)
  {
    next_ = ifTrue;
    setRhs(0, cond);
  }

  Variable* cond() const { return rhs(); }
  BlockHeader* ifTrue() const { return static_cast<BlockHeader*>(next()); }
  BlockHeader* ifFalse() const { return ifFalse_; }

  BlockHeader* nextBlock() const { return ifTrue(); }
  BlockHeader* nextAltBlock() const { return ifFalse(); }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

  bool isTerminator() const { return true; }

private:

  // Allows BlockHeader to call setNextBlock(), which is used in updateJumpDestination()
  friend class BlockHeader;
  void setNextBlock(BlockHeader* dest) { next_ = dest; }
  void setNextAltBlock(BlockHeader* dest) { ifFalse_ = dest; }

  BlockHeader* ifFalse_;
};

class OpcodeImmediate : public OpcodeL {
public:

  OpcodeImmediate(int file, int line, Opcode* prev, Variable* lhs, VALUE value)
    : OpcodeL(file, line, prev, lhs), value_(value) {}

  VALUE value() const { return value_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  VALUE value_;
};

class OpcodeEnv : public OpcodeL {
public:

  OpcodeEnv(int file, int line, Opcode* prev, Variable* lhs)
    : OpcodeL(file, line, prev, lhs) {}

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

  // Helper methods
  static ID envName();
  static bool isEnv(Variable* v);

private:

  static ID envName_;

};

class OpcodeLookup : public OpcodeLR<2> {
public:

  OpcodeLookup(int file, int line, Opcode* prev, Variable* lhs, Variable* receiver, ID methodName, Variable* env)
    : OpcodeLR<2>(file, line, prev, lhs), methodName_(methodName)
  {
    setRhs(0, receiver);
    setRhs(1, env);
  }

  OpcodeLookup(int file, int line, Opcode* prev, Variable* lhs, Variable* receiver, ID methodName, Variable* env, mri::MethodEntry me)
    : OpcodeLR<2>(file, line, prev, lhs), methodName_(methodName), me_(me)
  {
    setRhs(0, receiver);
    setRhs(1, env);
  }

  Variable* receiver() const { return rhs(); }
  ID methodName() const { return methodName_; }
  Variable* env() const { return rhs(1); }
  mri::MethodEntry methodEntry() const { return me_; }
  void setMethodEntry(mri::MethodEntry me) { me_ = me; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  ID methodName_;
  mri::MethodEntry me_;
};

class OpcodeCall : public OpcodeVa {
public:

  OpcodeCall(int file, int line, Opcode* prev, Variable* lhs, Variable* lookup, int rhsSize, Variable* env)
    : OpcodeVa(file, line, prev, lhs, rhsSize),
      lookup_(lookup), env_(env) {}

  OpcodeCall* clone(Opcode* prev, Variable* methodEntry) const;

  Variable* lookup() const { return lookup_; }
  OpcodeLookup* lookupOpcode() const;

  Variable* receiver() const { return rhs(0); }

  Variable* outEnv() { return env_; }
  void setOutEnv(Variable* env) { env_ = env; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  Variable* lookup_;

  // Should be treated as a left-side hand value
  Variable* env_;
};

class OpcodeConstant : public OpcodeLR<2> {
public:

  OpcodeConstant(int file, int line, Opcode* prev, Variable* lhs, Variable* base, ID name, bool toplevel, Variable* inEnv, Variable* outEnv)
    : OpcodeLR(file, line, prev, lhs), name_(name), toplevel_(toplevel), outEnv_(outEnv)
  {
    setRhs(0, base);
    setRhs(1, inEnv);
  }

  ID name() const { return name_; }
  Variable* base() const { return rhs(0); }
  bool toplevel() const { return toplevel_; }

  Variable* inEnv() const { return rhs(1); }
  void setInEnv(Variable* e) { setRhs(1, e); }

  Variable* outEnv() { return outEnv_; }
  void setOutEnv(Variable* e) { outEnv_ = e; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  ID name_;
  bool toplevel_;

  // Should be treated as a left-side hand value
  Variable* outEnv_;
};

class OpcodePrimitive : public OpcodeVa {
public:

  OpcodePrimitive(int file, int line, Opcode* prev, Variable* lhs, ID name, int rhsSize)
    : OpcodeVa(file, line, prev, lhs, rhsSize),
      name_(name) {}

  ID name() const { return name_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  ID name_;
};

class OpcodePhi : public OpcodeVa {
public:

  OpcodePhi(int file, int line, Opcode* prev, Variable* lhs, int rhsSize, BlockHeader* block)
    : OpcodeVa(file, line, prev, lhs, rhsSize), block_(block) {}

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

  BlockHeader* block() const { return block_; }

private:

  // Block where the phi opcode is located
  // (The LLVM phi nodes need the backedge information)
  BlockHeader* block_;
};

class OpcodeExit : public Opcode {
public:

  OpcodeExit(int file, int line, Opcode* prev)
    : Opcode(file, line, prev) {}

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

  bool isTerminator() const { return true; }

};

RBJIT_NAMESPACE_END
