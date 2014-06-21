#include <cstring> // memset
#include "rbjit/opcodemultiplexer.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcodefactory.h"

RBJIT_NAMESPACE_BEGIN

BlockHeader*
OpcodeMultiplexer::multiplex(BlockHeader* block, Opcode* opcode, Variable* selector, const std::vector<mri::Class>& cases, bool otherwise)
{
  // Split the base block at the specified opcode
  BlockHeader* exitBlock = cfg_->splitBlock(block, opcode, true);
  BlockHeader* entryBlock = cfg_->insertEmptyBlockAfter(block);

  OpcodeFactory factory(cfg_, entryBlock, entryBlock);

  Variable* sel = factory.addPrimitive("rbjit__class_of", 1, selector);

  int count = cases.size() - 1 + otherwise;
  for (int i = 0; i < count; ++i) {
    Variable* cls = factory.addImmediate(cases[i].value(), true);
    Variable* cond = factory.addPrimitive("rbjit__bitwise_compare_eq", 2, sel, cls);
    factory.addJumpIf(cond, 0, 0);

    // Create an empty block for each class
    OpcodeFactory trueFactory(factory, 0);
    BlockHeader* trueBlock = trueFactory.lastBlock();
    segments_.push_back(trueBlock);
    trueFactory.addJump(exitBlock);
    factory.lastBlock()->updateJumpDestination(trueBlock);

    factory.addBlockHeaderAsFalseBlock();
  }

  segments_.push_back(factory.lastBlock());
  factory.addJump(exitBlock);

  // Insert an empty phi node if the opcode has a left-hand side value
  if (opcode->lhs()) {
    phi_ = new OpcodePhi(exitBlock->file(), exitBlock->line(), 0, opcode->lhs(), count, exitBlock);
    memset(phi_->rhsBegin(), 0, sizeof(Variable*) * count);
    exitBlock->insertAfter(phi_);
  }

  return exitBlock;
}

RBJIT_NAMESPACE_END
