#include <cstring> // memset
#include "rbjit/opcodemultiplexer.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcodefactory.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

Variable*
OpcodeMultiplexer::generateTypeTestOpcode(OpcodeFactory* factory, Variable* selector, mri::Class cls)
{
  Variable* cond = cfg_->createVariable();

  if (cls == mri::Class::trueClass()) {
    factory->addPrimitive(cond, "rbjit__is_true", 1, selector);
  }
  else if (cls == mri::Class::falseClass()) {
    factory->addPrimitive(cond, "rbjit__is_false", 1, selector);
  }
  else if (cls == mri::Class::nilClass()) {
    factory->addPrimitive(cond, "rbjit__is_nil", 1, selector);
  }
  else if (cls == mri::Class::fixnumClass()) {
    factory->addPrimitive(cond, "rbjit__is_fixnum", 1, selector);
  }
  else {
    Variable* c = factory->addImmediate(cfg_->createVariable(), cls, true);
    Variable* selc = factory->addPrimitive(cfg_->createVariable(), "rbjit__class_of", 1, selector);
    factory->addPrimitive(cond, "rbjit__bitwise_compare_eq", 2, c, selc);
  }

  return cond;
}

BlockHeader*
OpcodeMultiplexer::multiplex(BlockHeader* block, Opcode* opcode, Variable* selector, const std::vector<mri::Class>& cases, bool otherwise)
{
  // Split the base block at the specified opcode
  BlockHeader* exitBlock = cfg_->splitBlock(block, opcode, true, true);
  exitBlock->setDebugName("mul_exit");
  BlockHeader* entryBlock = cfg_->insertEmptyBlockAfter(block);
  entryBlock->setDebugName("mul_entry");

  OpcodeFactory factory(cfg_, entryBlock, entryBlock);

  int count = cases.size() - 1 + otherwise;
  for (int i = 0; i < count; ++i) {
    Variable* cls = factory.addImmediate(cfg_->createVariable(), cases[i].value(), true);
    Variable* cond = generateTypeTestOpcode(&factory, selector, cases[i].value());
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

  // Insert an empty phi node for the lhs variable
  Variable* lhs = opcode->lhs();
  Opcode* last = exitBlock;
  if (lhs) {
    phi_ = new OpcodePhi(exitBlock->file(), exitBlock->line(), 0, lhs, count + 1, exitBlock);
    memset(phi_->rhsBegin(), 0, sizeof(Variable*) * (count + 1));
    phi_->insertAfter(last);
    lhs->updateDefSite(exitBlock, phi_);
    last = phi_;
  }

  // Insert an empty phi node for the env
  if (typeid(*opcode) == typeid(OpcodeCall)) {
    OpcodeCall* call = static_cast<OpcodeCall*>(opcode);
    envPhi_ = new OpcodePhi(exitBlock->file(), exitBlock->line(), 0, call->env(), count + 1, exitBlock);
    memset(envPhi_->rhsBegin(), 0, sizeof(Variable*) * (count + 1));
    envPhi_->insertAfter(last);
    lhs->updateDefSite(exitBlock, envPhi_);
  }

  return exitBlock;
}

RBJIT_NAMESPACE_END
