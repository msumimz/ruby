#pragma once
#include "rbjit/common.h"

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
  virtual bool visitOpcode(BlockHeader* op) { return true; }
  virtual bool visitOpcode(OpcodeCopy* op) { return true; }
  virtual bool visitOpcode(OpcodeJump* op) { return true; }
  virtual bool visitOpcode(OpcodeJumpIf* op) { return true; }
  virtual bool visitOpcode(OpcodeImmediate* op) { return true; }
  virtual bool visitOpcode(OpcodeCall* op) { return true; }
  virtual bool visitOpcode(OpcodePhi* op) { return true; }
};

////////////////////////////////////////////////////////////
// Base abstract class

class Opcode {
public:

  Opcode() : file_(0), line_(0), next_(0) {}
  Opcode(int file, int line, Opcode* prev)
    : file_(file), line_(line), next_(0)
  {
    if (prev) {
      prev->next_ = this;
    }
  }

  virtual ~Opcode() {}

  Opcode* next() const { return next_; }

  // Obtain variables
  virtual Variable* lhs() const { return 0; }
  virtual Variable*const* rhsBegin() const { return 0; }
  virtual Variable*const* rhsEnd() const { return 0; }
  virtual Variable** rhsBegin() { return 0; }
  virtual Variable** rhsEnd() { return 0; }

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
  Opcode* idom() const { return idom_; }

  Opcode* footer() const { return footer_; }
  void setFooter(Opcode* footer) { footer_ = footer; }

  // backedges

  struct Backedge {
    BlockHeader* block_;
    Backedge* next_;
    Backedge(BlockHeader* block, Backedge* next)
      : block_(block), next_(next)
    {}
  };

  void addBackedge(BlockHeader* block);
  bool hasBackedge() const { return backedge_.block_ != 0; }
  bool hasMultipleBackedges() const { return backedge_.next_ != 0; }
  unsigned backedgeSize() const;
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

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

};

class OpcodeJumpIf : public OpcodeR<1> {
public:

  OpcodeJumpIf(int file, int line, Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse)
    : OpcodeR<1>(file, line, ifTrue), ifFalse_(ifFalse)
  { setRhs(0, cond); }

  Variable* cond() const { return rhs(); }
  BlockHeader* ifTrue() const { return static_cast<BlockHeader*>(next()); }
  BlockHeader* ifFalse() const { return ifFalse_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  BlockHeader* ifFalse_;
};

class OpcodeImmediate : public OpcodeL {
public:

  OpcodeImmediate(int file, int line, Opcode* prev, Variable* lhs, mri::VALUE value)
    : OpcodeL(file, line, prev, lhs), value_(value) {}

  mri::VALUE value() const { return value_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  mri::VALUE value_;
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

  OpcodeCall(int file, int line, Opcode* prev, Variable* lhs, mri::ID methodName, int rhsSize)
    : OpcodeVa(file, line, prev, lhs, rhsSize), methodName_(methodName) {}

  mri::ID methodName() const { return methodName_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  mri::ID methodName_;
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
