#include <cstring> // memset
#include "rbjit/opcodemultiplexer.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcodefactory.h"
#include "rbjit/variable.h"
#include "rbjit/typecontext.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/idstore.h"
#include "rbjit/scope.h"

RBJIT_NAMESPACE_BEGIN

Variable*
OpcodeMultiplexer::generateTypeTestOpcode(OpcodeFactory* factory, Variable* selector, mri::Class cls)
{
  Variable* cond;

  if (cls == mri::Class::trueClass()) {
    cond = factory->addPrimitive(IdStore::get(ID_rbjit__is_true), 1, selector);
  }
  else if (cls == mri::Class::falseClass()) {
    cond = factory->addPrimitive(IdStore::get(ID_rbjit__is_false), 1, selector);
  }
  else if (cls == mri::Class::nilClass()) {
    cond = factory->addPrimitive(IdStore::get(ID_rbjit__is_nil), 1, selector);
  }
  else if (cls == mri::Class::fixnumClass()) {
    cond = factory->addPrimitive(IdStore::get(ID_rbjit__is_fixnum), 1, selector);
  }
  else {
    Variable* c = factory->addImmediate(cls, true);
    Variable* selc = factory->addPrimitive(IdStore::get(ID_rbjit__class_of), 1, selector);
    cond = factory->addPrimitive(IdStore::get(ID_rbjit__bitwise_compare_eq), 2, c, selc);
  }

  typeContext_->addNewTypeConstraint(cond,
      TypeSelection::create(TypeExactClass::create(mri::Class::trueClass()),
                            TypeExactClass::create(mri::Class::falseClass())));

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

  OpcodeFactory factory(cfg_, scope_, entryBlock, entryBlock);

  int count = cases.size() - 1 + otherwise;
  for (int i = 0; i < count; ++i) {
    Variable* cond = generateTypeTestOpcode(&factory, selector, cases[i].value());
    factory.addJumpIf(cond, 0, 0);

    // Create an empty block for each class
    OpcodeFactory trueFactory(factory);
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
    envPhi_ = new OpcodePhi(exitBlock->file(), exitBlock->line(), 0, call->outEnv(), count + 1, exitBlock);
    memset(envPhi_->rhsBegin(), 0, sizeof(Variable*) * (count + 1));
    envPhi_->insertAfter(last);
    call->outEnv()->updateDefSite(exitBlock, envPhi_);
  }

  return exitBlock;
}

RBJIT_NAMESPACE_END
