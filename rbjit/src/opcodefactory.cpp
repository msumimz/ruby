#include <cassert>
#include "rbjit/opcodefactory.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"
#include "rbjit/controlflowgraph.h"

RBJIT_NAMESPACE_BEGIN

BlockHeader*
OpcodeFactory::createBlockHeader()
{
  BlockHeader* block = new BlockHeader(file_, line_, lastOpcode_, lastBlock_, cfg_->blocks()->size(), depth_, lastBlock_);
  cfg_->blocks_.push_back(block);
  ++cfg_->opcodeCount_;

  return block;
}

BlockHeader*
OpcodeFactory::createFreeBlockHeader(BlockHeader* idom)
{
  BlockHeader* block = new BlockHeader(file_, line_, 0, 0, cfg_->blocks()->size(), depth_, idom);
  cfg_->blocks_.push_back(block);
  ++cfg_->opcodeCount_;

  return block;
}

Variable*
OpcodeFactory::createNamedVariable(ID name)
{
  Variable* v = Variable::createNamed(lastBlock_, lastOpcode_, cfg_->variables_.size(), name);
  cfg_->variables_.push_back(v);
  return v;
}

Variable*
OpcodeFactory::createTemporary(bool useResult)
{
  if (!useResult) {
    return 0;
  }

  Variable* v = Variable::createUnnamedSsa(lastBlock_, lastOpcode_, cfg_->variables_.size());
  cfg_->variables_.push_back(v);
  return v;
}

BlockHeader*
OpcodeFactory::addBlockHeader()
{
  BlockHeader* block = createBlockHeader();
  lastOpcode_ = block;
  lastBlock_ = block;

  return block;
}

BlockHeader*
OpcodeFactory::addFreeBlockHeader(BlockHeader* idom)
{
  BlockHeader* block = createFreeBlockHeader(idom);
  lastOpcode_ = block;
  lastBlock_ = block;

  return block;
}

Variable*
OpcodeFactory::addCopy(Variable* rhs, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, 0, rhs);
  ++cfg_->opcodeCount_;
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addCopy(Variable* lhs, Variable* rhs)
{
  OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, lhs, rhs);
  ++cfg_->opcodeCount_;
  lhs->defInfo()->addDefSite(lastBlock_);

  lastOpcode_ = op;

  return lhs;
}

void
OpcodeFactory::addJump(BlockHeader* dest)
{
  assert(dest);

  OpcodeJump* op = new OpcodeJump(file_, line_, lastOpcode_, dest);
  ++cfg_->opcodeCount_;

  dest->addBackedge(lastBlock_);

  lastBlock_->setFooter(op);

  lastOpcode_ = dest;
  lastBlock_ = dest;
}

void
OpcodeFactory::addJumpIf(Variable* cond, BlockHeader* ifTrue, BlockHeader* ifFalse)
{
  BlockHeader* nextBlock = ifTrue;

  if (!ifFalse) {
    ifFalse = createBlockHeader();
    nextBlock = ifFalse;
  }

  if (!ifTrue) {
    ifTrue = createBlockHeader();
    nextBlock = ifTrue;
  }

  OpcodeJumpIf* op = new OpcodeJumpIf(file_, line_, lastOpcode_, cond, ifTrue, ifFalse);
  ++cfg_->opcodeCount_;

  ifTrue->addBackedge(lastBlock_);
  ifFalse->addBackedge(lastBlock_);

  lastBlock_->setFooter(op);

  lastBlock_ = nextBlock;
  lastOpcode_ = nextBlock;
}

Variable*
OpcodeFactory::addImmediate(VALUE value, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  OpcodeImmediate* op = new OpcodeImmediate(file_, line_, lastOpcode_, 0, value);
  ++cfg_->opcodeCount_;
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);

  return lhs;
}

Variable*
OpcodeFactory::addCall(ID methodName, Variable* self, Variable*const* argsBegin, Variable*const* argsEnd, bool useResult)
{
  int n = 1 + (argsEnd - argsBegin);

  /*
  OpcodeL* op;
  switch (n) {
  case 1:  op = new OpcodeCall<1>(file_, line_, lastOpcode_, 0, selector); break;
  case 2:  op = new OpcodeCall<2>(file_, line_, lastOpcode_, 0, selector); break;
  case 3:  op = new OpcodeCall<3>(file_, line_, lastOpcode_, 0, selector); break;
  case 4:  op = new OpcodeCall<4>(file_, line_, lastOpcode_, 0, selector); break;
  case 5:  op = new OpcodeCall<5>(file_, line_, lastOpcode_, 0, selector); break;
  default: op = new OpcodeCallVa(file_, line_, lastOpcode_, 0, selector, n);
  }
  */
  OpcodeCall* op = new OpcodeCall(file_, line_, lastOpcode_, 0, methodName, n);
  ++cfg_->opcodeCount_;
  lastOpcode_ = op;

  Variable* lhs = createTemporary(useResult);
  op->setLhs(lhs);

  Variable** v = op->rhsBegin();
  *v++ = self;
  for (Variable*const* arg = argsBegin; arg < argsEnd; ++arg) {
    *v++ = *arg;
  }

  return lhs;
}

Variable*
OpcodeFactory::addPhi(Variable*const* rhsBegin, Variable*const* rhsEnd, bool useResult)
{
  if (!useResult) {
    return 0;
  }

  int n = 1 + (rhsEnd - rhsBegin);

  /*
  OpcodeL* op;
  switch (n) {
  case 1:  op = new OpcodePhi<1>(file_, line_, lastOpcode_, 0); break;
  case 2:  op = new OpcodePhi<2>(file_, line_, lastOpcode_, 0); break;
  case 3:  op = new OpcodePhi<3>(file_, line_, lastOpcode_, 0); break;
  case 4:  op = new OpcodePhi<4>(file_, line_, lastOpcode_, 0); break;
  case 5:  op = new OpcodePhi<5>(file_, line_, lastOpcode_, 0); break;
  default: op = new OpcodePhiVa(file_, line_, lastOpcode_, 0, n);
  }
  */
  OpcodePhi* op = new OpcodePhi(file_, line_, lastOpcode_, 0, n);
  ++cfg_->opcodeCount_;
  lastOpcode_ = op;

  Variable* lhs = createTemporary(true);
  op->setLhs(lhs);

  Variable** v = op->rhsBegin();
  for (Variable*const* rhs = rhsBegin; rhs < rhsEnd; ++rhs) {
    *v++ = *rhs;
  }

  return lhs;
}

void
OpcodeFactory::addJumpToReturnBlock(Variable* returnValue)
{
  if (returnValue) {
    OpcodeCopy* op = new OpcodeCopy(file_, line_, lastOpcode_, 0, returnValue);
    ++cfg_->opcodeCount_;
    lastOpcode_ = op;
    if (!cfg_->output_) {
      cfg_->output_ = createTemporary(true);
    }
    op->setLhs(cfg_->output_); // add definfo?
  }

  addJump(cfg_->exit());

  halted_ = true;
}

void
OpcodeFactory::createEntryExitBlocks()
{
  // entry block
  cfg_->entry_ = addBlockHeader();
  cfg_->undefined_ = addImmediate(0, true);

  // exit block
  BlockHeader* exit = createFreeBlockHeader(0);
  exit->setFooter(exit);
  cfg_->exit_ = exit;
}

RBJIT_NAMESPACE_END
