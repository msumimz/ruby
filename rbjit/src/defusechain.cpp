#include <algorithm>
#include "rbjit/defusechain.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

DefUseChain::DefUseChain(ControlFlowGraph* cfg)
  : cfg_(cfg),
    uses_(cfg_->variables()->size()),
    conditions_(cfg_->variables()->size(), false),
    block_(0)
{
  build();
}

std::vector<std::pair<BlockHeader*, Variable*>>&
DefUseChain::uses(Variable* v)
{
  return uses_[v->index()];
}

const std::vector<std::pair<BlockHeader*, Variable*>>&
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
  size_t blockCount = cfg_->blocks()->size();
  for (size_t i = 0; i < blockCount; ++i) {
    BlockHeader* block = (*cfg_->blocks())[i];
    block->visitEachOpcode(this);
  }
}

bool
DefUseChain::visitOpcodeVa(OpcodeVa* op)
{
  Variable* lhs = op->lhs();
  if (!lhs) {
    return true;
  }

  auto i = op->rhsBegin();
  auto end = op->rhsEnd();
  for (; i < end; ++i) {
    uses_[(*i)->index()].push_back(std::make_pair(block_, lhs));
  }
  return true;
}

bool
DefUseChain::visitOpcode(BlockHeader* op)
{
  block_ = op;
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeCopy* op)
{
  uses_[op->rhs()->index()].push_back(std::make_pair(block_, op->lhs()));
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
  uses_[op->receiver()->index()].push_back(std::make_pair(block_, op->lhs()));
  uses_[op->env()->index()].push_back(std::make_pair(block_, op->lhs()));
  return true;
}

bool
DefUseChain::visitOpcode(OpcodeCall* op)
{
  return visitOpcodeVa(op);
}

bool
DefUseChain::visitOpcode(OpcodePrimitive* op)
{
  return true;
}

bool
DefUseChain::visitOpcode(OpcodePhi* op)
{
  return visitOpcodeVa(op);
}

bool
DefUseChain::visitOpcode(OpcodeExit* op)
{
  return true;
}

std::string
DefUseChain::debugPrint() const
{
  char buf[256];
  std::string out;

  size_t size = cfg_->variables()->size();
  for (size_t i = 0; i < size; ++i) {
    sprintf(buf, "%Ix:", (*cfg_->variables())[i]);
    out += buf;
    const std::vector<std::pair<BlockHeader*, Variable*>>& uses = uses_[i];
    for (auto i = uses.cbegin(), end = uses.cend(); i != end; ++i) {
      sprintf(buf, " %Ix:%Ix", i->first, i->second);
      out += buf;
    };
    out += '\n';
  }

  return out;
}

RBJIT_NAMESPACE_END
