#pragma once
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

class Variable;
class BlockHeader;
class OpcodeCopy;
class OpcodeJump;
class OpcodeJumpIf;
class OpcodeImmediate;
class OpcodeCall;
class OpcodePhi;

////////////////////////////////////////////////////////////
// Visitor interface

class OpcodeVisitor {
public:
  virtual bool visitOpcode(BlockHeader* op) = 0;
  virtual bool visitOpcode(OpcodeCopy* op) = 0;
  virtual bool visitOpcode(OpcodeJump* op) = 0;
  virtual bool visitOpcode(OpcodeJumpIf* op) = 0;
  virtual bool visitOpcode(OpcodeImmediate* op) = 0;
  virtual bool visitOpcode(OpcodeCall* op) = 0;
  virtual bool visitOpcode(OpcodePhi* op) = 0;
};

////////////////////////////////////////////////////////////
// Base abstract class

class Opcode {
public:

  Opcode(int file, int line, Opcode* prev)
    : file_(file), line_(line), next_(0)
  {
    if (prev) {
      next_ = prev->next_;
      prev->next_ = this;
    }
  }

  virtual ~Opcode() {}

  Opcode* next() const { return next_; }
  void removeNextOpcode() { next_ = next_->next_; }

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

protected:

  int file_ : 8;
  int line_ : 16;
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

  int index() const { return index_; }
  int depth() const { return depth_; }
  BlockHeader* idom() const { return idom_; }

  Opcode* footer() const { return footer_; }
  void setFooter(Opcode* footer) { footer_ = footer; }

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

  void addBackedge(BlockHeader* block);
  bool hasBackedge() const { return backedge_.block_ != 0; }
  bool hasMultipleBackedges() const { return backedge_.next_ != 0; }
  int backedgeSize() const;
  const Backedge* backedgeAt(int n) const;

  // visitor
  bool visitEachOpcode(OpcodeVisitor* visitor);
  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  Backedge backedge_;
  Opcode* footer_;

  int index_;
  int depth_;

  BlockHeader* idom_;
  BlockHeader* rescueBlock_;
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

private:

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

/*
template <int N>
class OpcodeCall : public OpcodeLR<N> {
public:

  OpcodeCall(int file, int line, Opcode* prev, Variable* lhs, ID methodName)
    : OpcodeLR<N>(file, line, prev, lhs), methodName_(methodName) {}

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  ID methodName_;
};
*/

class OpcodeCall : public OpcodeVa {
public:

  OpcodeCall(int file, int line, Opcode* prev, Variable* lhs, ID methodName, int rhsSize)
    : OpcodeVa(file, line, prev, lhs, rhsSize), methodName_(methodName) {}

  ID methodName() const { return methodName_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  ID methodName_;
};

/*
template <int N>
class OpcodePhi : public OpcodeLR<N> {
public:

  OpcodePhi(int file, int line, Opcode* prev, Variable* lhs)
    : OpcodeLR<N>(file, line, prev, lhs) {}

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

};
*/

class OpcodePhi : public OpcodeVa {
public:

  OpcodePhi(int file, int line, Opcode* prev, Variable* lhs, int rhsSize)
    : OpcodeVa(file, line, prev, lhs, rhsSize) {}

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

};

RBJIT_NAMESPACE_END
