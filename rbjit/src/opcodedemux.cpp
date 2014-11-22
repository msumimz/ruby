#include <cstring> // memset
#include "rbjit/opcodedemux.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/blockbuilder.h"
#include "rbjit/variable.h"
#include "rbjit/typecontext.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/idstore.h"
#include "rbjit/scope.h"
#include "rbjit/block.h"

RBJIT_NAMESPACE_BEGIN

Variable*
OpcodeDemux::generateTypeTestOpcode(BlockBuilder* builder, Variable* selector, mri::Class cls, SourceLocation* loc)
{
  Variable* cond;

  if (cls == mri::Class::trueClass()) {
    cond = builder->add(OpcodePrimitive::create(loc, nullptr, IdStore::get(ID_rbjit__is_true), 1, selector));
  }
  else if (cls == mri::Class::falseClass()) {
    cond = builder->add(OpcodePrimitive::create(loc, nullptr, IdStore::get(ID_rbjit__is_false), 1, selector));
  }
  else if (cls == mri::Class::nilClass()) {
    cond = builder->add(OpcodePrimitive::create(loc, nullptr, IdStore::get(ID_rbjit__is_nil), 1, selector));
  }
  else if (cls == mri::Class::fixnumClass()) {
    cond = builder->add(OpcodePrimitive::create(loc, nullptr, IdStore::get(ID_rbjit__is_fixnum), 1, selector));
  }
  else {
    Variable* c = builder->add(new OpcodeImmediate(loc, nullptr, cls));
    Variable* selc = builder->add(OpcodePrimitive::create(loc, nullptr, IdStore::get(ID_rbjit__class_of), 1, selector));
    cond = builder->add(OpcodePrimitive::create(loc, nullptr, IdStore::get(ID_rbjit__bitwise_compare_eq), 2, c, selc));
  }

  typeContext_->addNewTypeConstraint(cond,
      TypeSelection::create(TypeExactClass::create(mri::Class::trueClass()),
                            TypeExactClass::create(mri::Class::falseClass())));

  return cond;
}

Block*
OpcodeDemux::demultiplex(Block* block, Block::Iterator iter, Variable* selector,
                         const std::vector<mri::Class>& cases, bool otherwise)
{
  Opcode* opcode = *iter;
  segments_.clear();

  // Split the base block at the specified opcode
  exitBlock_ = cfg_->splitBlock(block, iter, true);
  exitBlock_->setDebugName("demux_exit");

  BlockBuilder builder(cfg_, scope_, nullptr, block);

  int count = cases.size() - 1 + otherwise;
  for (int i = 0; i < count; ++i) {
    Variable* cond = generateTypeTestOpcode(&builder, selector, cases[i].value(), opcode->sourceLocation());
    Block* trueBlock = new Block("demux_segment");
    cfg_->addBlock(trueBlock);
    Block* nextBlock = new Block("demux_cond");
    cfg_->addBlock(nextBlock);
    builder.add(new OpcodeJumpIf(opcode->sourceLocation(), cond, trueBlock, nextBlock));

    segments_.push_back(trueBlock);
    builder.setBlock(nextBlock);
  }

  segments_.push_back(builder.block());

  // Insert an empty phi node for the out env
  if (typeid(*opcode) == typeid(OpcodeCall)) {
    OpcodeCall* call = static_cast<OpcodeCall*>(opcode);
    envPhi_ = new OpcodePhi(opcode->sourceLocation(), call->outEnv(), count + 1, exitBlock_);
    memset(envPhi_->begin(), 0, sizeof(Variable*) * (count + 1));
    exitBlock_->insertOpcode(exitBlock_->begin(), envPhi_);
    call->outEnv()->setDefBlock(exitBlock_);
    call->outEnv()->setDefOpcode(envPhi_);
  }

  // Insert an empty phi node for the lhs variable
  Variable* lhs = opcode->lhs();
  if (lhs) {
    phi_ = new OpcodePhi(opcode->sourceLocation(), lhs, count + 1, exitBlock_);
    memset(phi_->begin(), 0, sizeof(Variable*) * (count + 1));
    exitBlock_->insertOpcode(exitBlock_->begin(), phi_);
    lhs->setDefBlock(exitBlock_);
    lhs->setDefOpcode(phi_);
  }

  return exitBlock_;
}

RBJIT_NAMESPACE_END
