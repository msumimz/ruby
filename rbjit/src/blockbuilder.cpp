#include "rbjit/blockbuilder.h"
#include "rbjit/block.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/scope.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

BlockBuilder::BlockBuilder(ControlFlowGraph* cfg, Scope* scope, DefInfoMap* defInfoMap, Block* block)
  : cfg_(cfg), scope_(scope), defInfoMap_(defInfoMap), block_(block), halted_(false)
{
  cfg_->addBlock(block);
}

BlockBuilder::BlockBuilder(const BlockBuilder* builder, Block* block)
  : cfg_(builder->cfg_), scope_(builder->scope_), defInfoMap_(builder->defInfoMap_), block_(block), halted_(false)
{
  cfg_->addBlock(block);
}

void
BlockBuilder::updateDefSite(Variable* v, Block* b, Opcode* op)
{
  if (defInfoMap_) {
    defInfoMap_->updateDefSite(v, b, op);
  }
  else {
    v->setDefBlock(b);
    v->setDefOpcode(op);
  }
}

Variable*
BlockBuilder::add(ID name, bool belongsToScope, OpcodeL* op)
{
  NamedVariable* nameRef = nullptr;
  if (belongsToScope) {
    nameRef = scope_->find(name);
    assert(nameRef);
  }

  Variable* v = new Variable(name, nameRef);
  op->setLhs(v);
  cfg_->addVariable(v);
  block_->addOpcode(op);
  updateDefSite(v, block_, op);

  return v;
}

Variable*
BlockBuilder::add(OpcodeL* op)
{
  Variable* v = op->lhs();
  if (!v) {
    v = new Variable();
    op->setLhs(v);
    cfg_->addVariable(v);
  }
  block_->addOpcode(op);
  updateDefSite(v, block_, op);

  return v;
}

void
BlockBuilder::addWithoutLhsAssigned(Opcode* op)
{
  block_->addOpcode(op);
}

void
BlockBuilder::addJump(SourceLocation* loc, Block* dest)
{
  addWithoutLhsAssigned(new OpcodeJump(loc, dest));
}

void
BlockBuilder::addJumpIf(SourceLocation* loc, Variable* cond, Block* ifTrue, Block* ifFalse)
{
  addWithoutLhsAssigned(new OpcodeJumpIf(loc, cond, ifTrue, ifFalse));
}

RBJIT_NAMESPACE_END
