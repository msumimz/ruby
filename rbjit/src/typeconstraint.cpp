#include "rbjit/typeconstraint.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// TypeAnalyzer

#if 0
class TypeAnalyzer : public OpcodeVisitor {
public:

  TypeAnalyzer() : constraint_(0) {}

  bool visitOpcode(BlockHeader* op);
  bool visitOpcode(OpcodeCopy* op);
  bool visitOpcode(OpcodeJump* op);
  bool visitOpcode(OpcodeJumpIf* op);
  bool visitOpcode(OpcodeImmediate* op);
  bool visitOpcode(OpcodeLookup* op);
  bool visitOpcode(OpcodeCall* op);
  bool visitOpcode(OpcodePhi* op);
  bool visitOpcode(OpcodeExit* op);

  TypeConstraint* typeConstraint() const { return constraint_; }

private:

  TypeConstraint* constraint_;

};

bool TypeAnalyzer::visitOpcode(BlockHeader* op) { assert(false); return true; }
bool TypeAnalyzer::visitOpcode(OpcodeJump* op) { assert(false); return true; }
bool TypeAnalyzer::visitOpcode(OpcodeJumpIf* op) { assert(false); return true; }
bool TypeAnalyzer::visitOpcode(OpcodeExit* op) { assert(false); return true; }

bool
TypeAnalyzer::visitOpcode(OpcodeCopy* op)
{
  constraint_ = new TypeSameAs(op->rhs());
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeImmediate* op)
{
  constraint_ = new TypeConstant(op->value());
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeLookup* op)
{
  constraint_ = new TypeMethodEntry();
  return true;
}

bool
TypeAnalyzer::visitOpcode(OpcodeCall* op)
{
  constraint_ = op->methodEntry()->typeConstraint()->methodType();
}

bool
TypeAnalyzer::visitOpcode(OpcodePhi* op)
{
  Variable*const* i = op->rhsBegin();
  Variable*const* end = op->rhsEnd();
  TypeConstraint* t = *i++;
  for (; i < end; ++i) {
    t = new TypeSelection(t, (*i)->typeConstraint()->clone());
  }
  constraint_ = t;
  return true;
}
#endif

////////////////////////////////////////////////////////////
// TypeConstraint

TypeConstraint*
TypeConstraint::create(Variable* v)
{
  return 0;
#if 0
  TypeAnalyzer a;
  v->defOpcode()->accept(&a);

  return a.typeConstraint();
#endif
}

RBJIT_NAMESPACE_END
