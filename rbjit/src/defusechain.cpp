#include <algorithm>
#include "rbjit/defusechain.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

DefUseChain::DefUseChain(ControlFlowGraph* cfg)
  : cfg_(cfg),
    uses_(cfg_->variableCount()),
    conditions_(cfg_->variableCount(), false),
    block_(nullptr)
{
  build();
}

std::vector<std::pair<Block*, Variable*>>&
DefUseChain::uses(Variable* v)
{
  return uses_[v->index()];
}

const std::vector<std::pair<Block*, Variable*>>&
DefUseChain::uses(Variable* v) const
{
  return uses_[v->index()];
}

bool
DefUseChain::isCondition(Variable* v) const
{
  return conditions_[v->index()];
}

void
DefUseChain::build()
{
  for (auto i = cfg_->begin(), end = cfg_->end(); i != end; ++i) {
    block_ = *i;
    (*i)->visitEachOpcode(this);
  }
}

bool
DefUseChain::visitOpcodeVa(Variable* lhs, OpcodeVa* op)
{
  if (!lhs) {
    return true;
  }

  for (auto i = op->begin(), end = op->end(); i < end; ++i) {
    uses_[(*i)->index()].push_back(std::make_pair(block_, lhs));
  }
  return true;
}

void
DefUseChain::addDefUseChain(Variable* def, Variable* use)
{
  uses_[use->index()].push_back(std::make_pair(block_, def));
}

bool
DefUseChain::visitOpcode(OpcodeCopy* op)
{
  addDefUseChain(op->lhs(), op->rhs());
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeJump* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeJumpIf* op)
{
  conditions_[op->cond()->index()] = true;
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeImmediate* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeEnv* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeLookup* op)
{
  addDefUseChain(op->lhs(), op->receiver());
  addDefUseChain(op->lhs(), op->env());
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeCall* op)
{
  visitOpcodeVa(op->lhs(), op);
  visitOpcodeVa(op->outEnv(), op);
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeConstant* op)
{
  if (!op->lhs()) {
    return true;
  }
  addDefUseChain(op->lhs(), op->base());
  addDefUseChain(op->outEnv(), op->base());
  addDefUseChain(op->lhs(), op->inEnv());
  addDefUseChain(op->outEnv(), op->inEnv());
  return true;
}

bool
DefUseChain::visitOpcode(OpcodePrimitive* op)
{
  return visitOpcodeVa(op->lhs(), op);
}

bool
DefUseChain::visitOpcode(OpcodePhi* op)
{
  return visitOpcodeVa(op->lhs(), op);
}

bool
DefUseChain::visitOpcode(OpcodeExit* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeArray* op)
{
  return visitOpcodeVa(op->lhs(), op);
}

bool
DefUseChain::visitOpcode(OpcodeRange* op)
{
  if (!op->lhs()) {
    return true;
  }
  addDefUseChain(op->lhs(), op->low());
  addDefUseChain(op->lhs(), op->high());
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeString* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeHash* op)
{
  return visitOpcodeVa(op->lhs(), op);
}

bool
DefUseChain::visitOpcode(OpcodeEnter* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeLeave* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeCheckArg* op)
{
  return true;
}

////////////////////////////////////////////////////////////
// Debug print

std::string
DefUseChain::debugPrint() const
{
  std::string out;

  size_t size = cfg_->variableCount();
  for (size_t i = 0; i < size; ++i) {
    out += stringFormat("%Ix:", cfg_->variable(i));
    const std::vector<std::pair<Block*, Variable*>>& uses = uses_[i];
    for (auto i = uses.cbegin(), end = uses.cend(); i != end; ++i) {
      out += stringFormat(" %Ix:%Ix", i->first, i->second);
    };
    out += '\n';
  }

  return out;
}

RBJIT_NAMESPACE_END
