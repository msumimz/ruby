#pragma once
#include <cassert>
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"

#define RBJIT_DEFINE_ACCEPT \
  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

RBJIT_NAMESPACE_BEGIN

class Variable;
class Block;
class Scope;

class OpcodeCopy;
class OpcodeJump;
class OpcodeJumpIf;
class OpcodeImmediate;
class OpcodeEnv;
class OpcodeLookup;
class OpcodeCall;
class OpcodeCodeBlock;
class OpcodeConstant;
class OpcodePrimitive;
class OpcodePhi;
class OpcodeExit;
class OpcodeArray;
class OpcodeRange;
class OpcodeString;
class OpcodeHash;
class OpcodeEnter;
class OpcodeLeave;
class OpcodeCheckArg;

////////////////////////////////////////////////////////////
// Visitor interface

class OpcodeVisitor {
public:
  virtual bool visitOpcode(OpcodeCopy* op) = 0;
  virtual bool visitOpcode(OpcodeJump* op) = 0;
  virtual bool visitOpcode(OpcodeJumpIf* op) = 0;
  virtual bool visitOpcode(OpcodeImmediate* op) = 0;
  virtual bool visitOpcode(OpcodeEnv* op) = 0;
  virtual bool visitOpcode(OpcodeLookup* op) = 0;
  virtual bool visitOpcode(OpcodeCall* op) = 0;
  virtual bool visitOpcode(OpcodeCodeBlock* op) = 0;
  virtual bool visitOpcode(OpcodeConstant* op) = 0;
  virtual bool visitOpcode(OpcodePrimitive* op) = 0;
  virtual bool visitOpcode(OpcodePhi* op) = 0;
  virtual bool visitOpcode(OpcodeExit* op) = 0;
  virtual bool visitOpcode(OpcodeArray* op) = 0;
  virtual bool visitOpcode(OpcodeRange* op) = 0;
  virtual bool visitOpcode(OpcodeString* op) = 0;
  virtual bool visitOpcode(OpcodeHash* op) = 0;
  virtual bool visitOpcode(OpcodeEnter* op) = 0;
  virtual bool visitOpcode(OpcodeLeave* op) = 0;
  virtual bool visitOpcode(OpcodeCheckArg* op) = 0;
};

////////////////////////////////////////////////////////////
// Source location

class SourceLocation {
  // TODO
};

////////////////////////////////////////////////////////////
// Base abstract class

class Opcode {
public:

  Opcode(SourceLocation* loc)
    : sourceLocation_(loc)
  {}

  virtual ~Opcode() {}

  typedef Variable** Iterator;
  typedef Variable*const* ConstIterator;

  SourceLocation* sourceLocation() const { return sourceLocation_;  }

  ////////////////////////////////////////////////////////////
  // Left-hand side value

  virtual Variable* lhs() const { return nullptr; }

  ////////////////////////////////////////////////////////////
  // Right-hand side values

  virtual Variable** begin() { return nullptr; }
  virtual Variable** end() { return nullptr; }
  virtual Variable*const* begin() const { return nullptr; }
  virtual Variable*const* end() const { return nullptr; }
  virtual Variable*const* cbegin() const { return begin(); }
  virtual Variable*const* cend() const { return end(); }

  int rhsCount() const { return end() - begin(); }
  bool hasRhs() const { return rhsCount() > 0; }

  ////////////////////////////////////////////////////////////
  // Env

  virtual Variable* outEnv() { return nullptr; }
  virtual void setOutEnv(Variable* e) { assert(!"Unimplemented"); }

  ////////////////////////////////////////////////////////////
  // Terminator

  virtual bool isTerminator() const { return false; }

  ////////////////////////////////////////////////////////////
  // Type name

  const char* typeName() const;
  const char* shortTypeName() const;

  ////////////////////////////////////////////////////////////
  // Visitor

  virtual bool accept(OpcodeVisitor* visitor) = 0;

protected:

  SourceLocation* sourceLocation_;
};

////////////////////////////////////////////////////////////
// OpcodeTerminator

class OpcodeTerminator {
public:
  virtual Block* nextBlock() const { return nullptr; }
  virtual void setNextBlock(Block* block) { assert(!"Undefined");  }
  virtual Block* nextAltBlock() const { return nullptr; }
  virtual void setNextAltBlock(Block* block) { assert(!"Undefined"); }
};

////////////////////////////////////////////////////////////
// Helper classes

// Opcode that has one lhs variable
class OpcodeL : public Opcode {
public:

  OpcodeL(SourceLocation* loc, Variable* lhs)
    : Opcode(loc), lhs_(lhs)
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

  OpcodeR(SourceLocation* loc)
    : Opcode(loc)
  {}

  Variable* rhs() const { return rhs_[0]; }
  Variable** begin() { return rhs_; }
  Variable** end() { return rhs_ + N; }
  Variable*const* begin() const { return rhs_; }
  Variable*const* end() const { return rhs_ + N; }

  void setRhs(int i, Variable* v) { rhs_[i] = v; }

protected:

  Variable* rhs_[N];
};

// Opcode that has one lhs and N of rhs
template <int N>
class OpcodeLR : public OpcodeL {
public:

  OpcodeLR(SourceLocation* loc, Variable* lhs)
    : OpcodeL(loc, lhs)
  {}

  Variable* rhs() const { return rhs_[0]; }
  Variable* rhs(int i) const { return rhs_[i]; }

  Variable** begin() { return rhs_; }
  Variable** end() { return rhs_ + N; }
  Variable*const* begin() const { return rhs_; }
  Variable*const* end() const { return rhs_ + N; }

  void setRhs(int i, Variable* v) { rhs_[i] = v; }

protected:

  Variable* rhs_[N];
};

// Opcode that has one lhs and rhs of any length
class OpcodeVa : public OpcodeL {
public:

  OpcodeVa(SourceLocation* loc, Variable* lhs, int rhsCount)
    : OpcodeL(loc, lhs), rhsCount_(rhsCount), rhs_(new Variable*[rhsCount])
  {}

  ~OpcodeVa() { delete rhs_; }

  Variable* rhs() const { return rhs_[0]; }
  Variable* rhs(int i) const { return rhs_[i]; }

  void setRhs(int i, Variable* v) { rhs_[i] = v; }

  int rhsCount() const { return rhsCount_;  }
  Variable** begin() { return rhs_; }
  Variable** end() { return rhs_ + rhsCount_; }
  Variable*const* begin() const { return rhs_; }
  Variable*const* end() const { return rhs_ + rhsCount_; }

protected:

  int rhsCount_;
  Variable** rhs_;
};

////////////////////////////////////////////////////////////
// Opcodes

class OpcodeCopy : public OpcodeLR<1> {
public:

  OpcodeCopy(SourceLocation* loc, Variable* lhs, Variable* rhs)
    : OpcodeLR<1>(loc, lhs)
  { setRhs(0, rhs); }

  RBJIT_DEFINE_ACCEPT

};

class OpcodeJump : public Opcode, public OpcodeTerminator {
public:

  OpcodeJump(SourceLocation* loc, Block* dest)
    : Opcode(loc), dest_(dest)
  {}

  RBJIT_DEFINE_ACCEPT

  Block* nextBlock() const { return dest_; }
  void setNextBlock(Block* block) { dest_ = block; }
  Block* nextAltBlock() const { return nullptr;  }
  bool isTerminator() const { return true; }

private:

  Block* dest_;

};

class OpcodeJumpIf : public OpcodeR<1>, public OpcodeTerminator {
public:

  OpcodeJumpIf(SourceLocation* loc, Variable* cond, Block* ifTrue, Block* ifFalse)
    : OpcodeR<1>(loc), ifTrue_(ifTrue), ifFalse_(ifFalse)
  { setRhs(0, cond); }

  RBJIT_DEFINE_ACCEPT

  Variable* cond() const { return rhs(); }

  Block* nextBlock() const { return ifTrue_; }
  void setNextBlock(Block* block) { ifTrue_ = block; }
  Block* nextAltBlock() const { return ifFalse_; }
  void setNextAltBlock(Block* block) { ifFalse_ = block; }
  bool isTerminator() const { return true; }

private:

  Block* ifTrue_;
  Block* ifFalse_;
};

class OpcodeImmediate : public OpcodeL {
public:

  OpcodeImmediate(SourceLocation* loc, Variable* lhs, VALUE value)
    : OpcodeL(loc, lhs), value_(value) {}

  RBJIT_DEFINE_ACCEPT

  VALUE value() const { return value_; }

protected:

  VALUE value_;
};

class OpcodeEnv : public OpcodeL {
public:

  OpcodeEnv(SourceLocation* loc, Variable* lhs)
    : OpcodeL(loc, lhs) {}

  RBJIT_DEFINE_ACCEPT

  // Helper methods
  static ID envName();
  static bool isEnv(Variable* v);

private:

  static ID envName_;

};

class OpcodeLookup : public OpcodeLR<2> {
public:

  OpcodeLookup(SourceLocation* loc, Variable* lhs, Variable* receiver, ID methodName, Variable* env)
    : OpcodeLR<2>(loc, lhs), methodName_(methodName)
  {
    setRhs(0, receiver);
    setRhs(1, env);
  }

  OpcodeLookup(SourceLocation* loc, Variable* lhs, Variable* receiver, ID methodName, Variable* env, mri::MethodEntry me)
    : OpcodeLR<2>(loc, lhs), methodName_(methodName), me_(me)
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

  OpcodeCall(SourceLocation* loc, Variable* lhs, Variable* lookup, int argCount, Variable* codeBlock, Variable* env)
    : OpcodeVa(loc, lhs, argCount + 2), env_(env)
  {
    setLookup(lookup);
    setCodeBlock(codeBlock);
  }

  OpcodeCall* clone(Variable* methodEntry) const;

  Variable* receiver() const { return rhs(0); }

  int argCount() const { return rhsCount() - 2; }
  Iterator argBegin() { return begin(); }
  Iterator argEnd() { return end() - 2; }
  ConstIterator argBegin() const { return cbegin(); }
  ConstIterator argEnd() const { return cend() - 2; }
  ConstIterator constArgBegin() const { return cbegin(); }
  ConstIterator constArgEnd() const { return cend() - 2; }

  Variable* lookup() const { return rhs(rhsCount() - 2); }
  void setLookup(Variable* lookup) { setRhs(rhsCount() - 2, lookup); }
  OpcodeLookup* lookupOpcode() const;

  Variable* codeBlock() const { return rhs(rhsCount() - 1); }
  void setCodeBlock(Variable* codeBlock) { setRhs(rhsCount() - 1, codeBlock); }

  Variable* outEnv() { return env_; }
  void setOutEnv(Variable* env) { env_ = env; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  // Should be treated as a left-side hand value
  Variable* env_;
};

class OpcodeCodeBlock : public OpcodeL {
public:

  OpcodeCodeBlock(SourceLocation* loc, Variable* lhs, const RNode* nodeIter)
    : OpcodeL(loc, lhs), nodeIter_(nodeIter)
  {}

  const RNode* nodeIter() const { return nodeIter_; }

  bool accept(OpcodeVisitor* visitor) { return visitor->visitOpcode(this); }

private:

  const RNode* nodeIter_;
};

class OpcodeConstant : public OpcodeLR<2> {
public:

  OpcodeConstant(SourceLocation* loc, Variable* lhs, Variable* base, ID name, bool toplevel, Variable* inEnv, Variable* outEnv)
    : OpcodeLR(loc, lhs), name_(name), toplevel_(toplevel), outEnv_(outEnv)
  {
    setRhs(0, base);
    setRhs(1, inEnv);
  }

  RBJIT_DEFINE_ACCEPT

  ID name() const { return name_; }
  Variable* base() const { return rhs(0); }
  bool toplevel() const { return toplevel_; }

  Variable* inEnv() const { return rhs(1); }
  void setInEnv(Variable* e) { setRhs(1, e); }

  Variable* outEnv() { return outEnv_; }
  void setOutEnv(Variable* e) { outEnv_ = e; }

private:

  ID name_;
  bool toplevel_;

  // Should be treated as a left-side hand value
  Variable* outEnv_;
};

class OpcodePrimitive : public OpcodeVa {
public:

  OpcodePrimitive(SourceLocation* loc, Variable* lhs, ID name, int rhsCount)
    : OpcodeVa(loc, lhs, rhsCount),
      name_(name) {}

  RBJIT_DEFINE_ACCEPT

  static OpcodePrimitive* create(SourceLocation* loc, Variable* lhs, ID name, Variable*const* argsBegin, Variable*const* argsEnd);
  static OpcodePrimitive* create(SourceLocation* loc, Variable* lhs, ID name, int argCount, ...);

  ID name() const { return name_; }

private:

  ID name_;
};

class OpcodePhi : public OpcodeVa {
public:

  OpcodePhi(SourceLocation* loc, Variable* lhs, int rhsCount, Block* block)
    : OpcodeVa(loc, lhs, rhsCount), block_(block) {}

  RBJIT_DEFINE_ACCEPT

  Block* block() const { return block_; }

private:

  // Block where the phi opcode is located
  // (The LLVM phi nodes need the backedge information)
  Block* block_;
};

class OpcodeExit : public Opcode, public OpcodeTerminator {
public:

  OpcodeExit(SourceLocation* loc)
    : Opcode(loc) {}

  RBJIT_DEFINE_ACCEPT

  bool isTerminator() const { return true; }
};

class OpcodeArray : public OpcodeVa {
public:

  OpcodeArray(SourceLocation* loc, Variable* lhs, int rhsCount)
    : OpcodeVa(loc, lhs, rhsCount)
  {}

  RBJIT_DEFINE_ACCEPT
};

class OpcodeRange : public OpcodeLR<2> {
public:

  OpcodeRange(SourceLocation* loc, Variable* lhs, Variable* low, Variable* high, bool exclusive)
    : OpcodeLR(loc, lhs), exclusive_(exclusive)
  {
    setRhs(0, low);
    setRhs(1, high);
  }

  RBJIT_DEFINE_ACCEPT

  Variable* low() const { return rhs(0); }
  Variable* high() const { return rhs(1); }

  bool exclusive() const { return exclusive_; }

private:

  bool exclusive_;
};

class OpcodeString : public OpcodeL {
public:

  OpcodeString(SourceLocation* loc, Variable* lhs, VALUE string)
    : OpcodeL(loc, lhs), string_(string)
  {}

  RBJIT_DEFINE_ACCEPT

  VALUE string() const { return string_;  }

private:

  VALUE string_;
};

class OpcodeHash : public OpcodeVa {
public:

  OpcodeHash(SourceLocation* loc, Variable* lhs, int rhsCount)
    : OpcodeVa(loc, lhs, rhsCount)
  { assert(rhsCount % 2 == 0); }

  RBJIT_DEFINE_ACCEPT
};

class OpcodeEnter : public Opcode {
public:

  OpcodeEnter(SourceLocation* loc, Scope* scope)
    : Opcode(loc), scope_(scope)
  {}

  RBJIT_DEFINE_ACCEPT

  Scope* scope() const { return scope_; }

private:

  Scope* scope_;
};

class OpcodeLeave : public Opcode {
public:

  OpcodeLeave(SourceLocation* loc)
    : Opcode(loc)
  {}

  RBJIT_DEFINE_ACCEPT
};

class OpcodeCheckArg : public Opcode {
public:

  OpcodeCheckArg(SourceLocation* loc)
    : Opcode(loc)
  {}

  RBJIT_DEFINE_ACCEPT
};

#undef RBJIT_DEFINE_ACCEPT

RBJIT_NAMESPACE_END
