#include <cstring> // memset
#include "rbjit/opcodemultiplexer.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcodefactory.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

BlockHeader*
OpcodeMultiplexer::multiplex(BlockHeader* block, Opcode* opcode, Variable* selector, const std::vector<mri::Class>& cases, bool otherwise)
{
  // Split the base block at the specified opcode
  BlockHeader* exitBlock = cfg_->splitBlock(block, opcode, true, true);
  exitBlock->setDebugName("mul_exit");
  BlockHeader* entryBlock = cfg_->insertEmptyBlockAfter(block);
  entryBlock->setDebugName("mul_entry");

  OpcodeFactory factory(cfg_, entryBlock, entryBlock);

  Variable* sel = factory.addPrimitive(cfg_->createVariable(), "rbjit__class_of", 1, selector);

  int count = cases.size() - 1 + otherwise;
  for (int i = 0; i < count; ++i) {
    Variable* cls = factory.addImmediate(cfg_->createVariable(), cases[i].value(), true);
    Variable* cond = factory.addPrimitive(cfg_->createVariable(), "rbjit__bitwise_compare_eq", 2, sel, cls);
    factory.addJumpIf(cond, 0, 0);

    // Create an empty block for each class
    OpcodeFactory trueFactory(factory, 0);
    BlockHeader* trueBlock = trueFactory.lastBlock();
    trueBlock->setDebugName("mul_segment");
    segments_.push_back(trueBlock);
    trueFactory.addJump(exitBlock);
    factory.lastBlock()->updateJumpDestination(trueBlock);

    factory.addBlockHeaderAsFalseBlock();
    factory.lastBlock()->setDebugName("mul_cond");
  }

  segments_.push_back(factory.lastBlock());
  factory.addJump(exitBlock);

  // Insert an empty phi node if the opcode has a left-hand side value
  Variable* lhs = opcode->lhs();
  if (lhs) {
    phi_ = new OpcodePhi(exitBlock->file(), exitBlock->line(), 0, lhs, count + 1, exitBlock);
    memset(phi_->rhsBegin(), 0, sizeof(Variable*) * (count + 1));
    phi_->insertAfter(exitBlock);
    lhs->updateDefSite(exitBlock, phi_);
  }

  return exitBlock;
}

RBJIT_NAMESPACE_END
