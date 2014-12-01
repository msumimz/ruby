#include <typeinfo>
#include <cstdarg>
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/rubyobject.h"
#include "rbjit/idstore.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// Opcode

const char*
Opcode::typeName() const
{
  if (typeid(*this) == typeid(Block)) {
    return "Block";
  }
  else {
    const int skip = strlen("const rbjit::Opcode"); // hopefully expaneded to constant at compile time
    return typeid(*this).name() + skip;
  }
}

const char*
Opcode::shortTypeName() const
{
  if (typeid(*this) == typeid(OpcodeImmediate)) {
    return "Imm";
  }
  else if (typeid(*this) == typeid(OpcodeCodeBlock)) {
    return "Block";
  }
  else if (typeid(*this) == typeid(OpcodeConstant)) {
    return "Const";
  }
  else if (typeid(*this) == typeid(OpcodePrimitive)) {
    return "Prim";
  }
  else {
    return typeName();
  }
}

////////////////////////////////////////////////////////////
// Call

OpcodeLookup*
OpcodeCall::lookupOpcode() const
{
  assert(typeid(*lookup()->defOpcode()) == typeid(OpcodeLookup));
  return static_cast<OpcodeLookup*>(lookup()->defOpcode());
}

OpcodeCall*
OpcodeCall::clone(Variable* methodEntry) const
{
  OpcodeCall* op = new OpcodeCall(sourceLocation(), lhs(), methodEntry, rhsCount() - 2, codeBlock(), env_);
  for (int i = 0; i <= argCount(); ++i) {
    op->setRhs(i, rhs(i));
  }
  return op;
}

////////////////////////////////////////////////////////////
// Primitive

OpcodePrimitive*
OpcodePrimitive::create(SourceLocation* loc, Variable* lhs, ID name, Variable*const* argsBegin, Variable*const* argsEnd)
{
  int n = argsEnd - argsBegin;
  OpcodePrimitive* op = new OpcodePrimitive(loc, lhs, name, n);

  Variable** v = op->begin();
  for (Variable*const* arg = argsBegin; arg < argsEnd; ++arg) {
    *v++ = *arg;
  }

  return op;
}

OpcodePrimitive*
OpcodePrimitive::create(SourceLocation* loc, Variable* lhs, ID name, int argCount, ...)
{
  OpcodePrimitive* op = new OpcodePrimitive(loc, lhs, name, argCount);

  va_list args;
  va_start(args, argCount);
  for (auto v = op->begin(), end = op->end(); v != end; ++v) {
    *v = va_arg(args, Variable*);
  }
  va_end(args);

  return op;
}

////////////////////////////////////////////////////////////
// Env

ID OpcodeEnv::envName_;

ID
OpcodeEnv::envName()
{
  if (!envName_) {
    envName_ = IdStore::get(ID_env);
  }
  return envName_;
}

bool
OpcodeEnv::isEnv(Variable* v)
{
  return v->name() == envName();
}

RBJIT_NAMESPACE_END
