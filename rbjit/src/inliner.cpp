#include <memory>
#include "rbjit/inliner.h"
#include "rbjit/methodinfo.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typecontext.h"
#include "rbjit/codeduplicator.h"
#include "rbjit/block.h"
#include "rbjit/blockbuilder.h"
#include "rbjit/debugprint.h"
#include "rbjit/opcodedemux.h"
#include "rbjit/recompilationmanager.h"
#include "rbjit/compilationinstance.h"

RBJIT_NAMESPACE_BEGIN

Inliner::Inliner(PrecompiledMethodInfo* mi)
  : mi_(mi), cfg_(mi->compilationInstance()->cfg()),
    scope_(mi->compilationInstance()->scope()),
    typeContext_(mi->compilationInstance()->typeContext()),
    visited_(cfg_->blockCount(), false)
{}

void
Inliner::doInlining()
{
  work_.push_back(cfg_->entryBlock());

loop:
  while (!work_.empty()) {
    Block* block = work_.back();
    work_.pop_back();
    visited_[block->index()] = true;

    for (auto i = block->begin(), end = block->end(); i != end; ++i) {
      Opcode* op = *i;
      OpcodeCall* call = dynamic_cast<OpcodeCall*>(op);
      if (call) {
        ID methodName = call->lookupOpcode()->methodName();
        if (inlineCallSite(block, i)) {
          visited_.resize(cfg_->blockCount());
          RecompilationManager::instance()->addCalleeCallerRelation(methodName, mi_);
          goto loop;
        }
      }
    }

    Block* b = block->nextBlock();
    if (b && !visited_[b->index()]) {
      work_.push_back(b);
    }
    b = block->nextAltBlock();
    if (b && !visited_[b->index()]) {
      work_.push_back(b);
    }
  }
}

bool
Inliner::inlineCallSite(Block* block, Block::Iterator callIter)
{
  OpcodeCall* op = static_cast<OpcodeCall*>(*callIter);
  RBJIT_ASSERT(typeid(*op) == typeid(OpcodeCall));

  RBJIT_DPRINTF(("----------Call Site: (%Ix %Ix) '%s'\n",
    block, op, mri::Id(op->lookupOpcode()->methodName()).name()));

  TypeConstraint* type = typeContext_->typeConstraintOf(op->receiver());
  std::unique_ptr<TypeList> list(type->resolve());

  TypeLookup* lookup = static_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lookup()));
  assert(typeid(*lookup) == typeid(TypeLookup));

  std::vector<MethodInfo*> methodInfos;
  std::vector<mri::Class> cases;
  std::vector<char> inlinable;
  bool otherwise = !lookup->isDetermined();
  for (auto i = lookup->candidates().cbegin(), end = lookup->candidates().cend(); i != end; ++i) {
    mri::MethodEntry me = *i;
    MethodInfo* mi = me.methodInfo();
    if (!mi) {
      otherwise = true;
      continue;
    }

    methodInfos.push_back(mi);
    cases.push_back(me.class_());
    if (me.methodInfo() == mi_ || !me.methodInfo()->astNode()) {
      inlinable.push_back(false);
    }
    else {
      inlinable.push_back(true);
    }
  }

  if (cases.empty()) {
    return false;
  }

  if (!otherwise && cases.size() == 1 && !inlinable[0]) {
    return false;
  }

  if (!otherwise && cases.size() == 1 && inlinable[0]) {
    Block* join = cfg_->splitBlock(block, callIter, true);
    join->setDebugName("inliner_join");

    std::tuple<Variable*, Variable*, Block*> result = replaceCallWithMethodBody(methodInfos[0], block, op, op->lhs(), op->outEnv());
    std::get<2>(result)->addJumpTo(nullptr, join);
  }
  else {
    OpcodeDemux mul(cfg_, scope_, typeContext_);
    Block* exitBlock = mul.demultiplex(block, callIter, op->receiver(), cases, otherwise);

    OpcodePhi* phi = mul.phi();
    OpcodePhi* envPhi = mul.envPhi();
    int size = methodInfos.size() + otherwise;
    int otherwiseIndex = size - 1;
    for (int i = 0; i < size; ++i) {
      Block* block = mul.segments()[i];

      std::tuple<Variable*, Variable*, Block*> result;
      if (i == otherwiseIndex) {
        // Otherwise branch
        result = insertCall(mri::MethodEntry(), block, op);
      }
      else if (inlinable[i]) {
        result = replaceCallWithMethodBody(methodInfos[i], block, op, 0, 0);
      }
      else {
        result = insertCall(methodInfos[i]->methodEntry(), block, op);
      }
      std::get<2>(result)->addJumpTo(nullptr, exitBlock);

      int index = -1;
      if (std::get<0>(result) && phi) {
        index = exitBlock->backedgeIndexOf(std::get<2>(result));
        assert(0 <= index && index < size);
        phi->setRhs(index, std::get<0>(result));
      }
      if (envPhi) {
        assert(std::get<1>(result));
        if (index == -1) {
          index = exitBlock->backedgeIndexOf(std::get<2>(result));
          assert(0 <= index && index < size);
        }
        envPhi->setRhs(index, std::get<1>(result));
      }
    }
  }

  // Remove OpcodeLookup
  // The variable does not be removed; instead, its defOpode is cleared to zero.
  Opcode* l = op->lookup()->defOpcode();
  block->removeOpcode(l);
  delete op->lookup()->defOpcode();
  op->lookup()->setDefOpcode(0);

  // Remove OpcodeCall
  delete op;

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintDotHeader());
  RBJIT_DPRINT(cfg_->debugPrintAsDot());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_DPRINT(typeContext_->debugPrint());
  assert(cfg_->checkSanityAndPrintErrors());
  assert(cfg_->checkSsaAndPrintErrors());

  return true;
}

std::tuple<Variable*, Variable*, Block*>
Inliner::replaceCallWithMethodBody(MethodInfo* methodInfo, Block* entry, OpcodeCall* op, Variable* result, Variable* exitEnv)
{
  // entry should not be terminated
  assert(entry->opcodeCount() == 0 || !entry->footer());

  assert(typeid(*methodInfo) == typeid(PrecompiledMethodInfo));
  PrecompiledMethodInfo* mi = static_cast<PrecompiledMethodInfo*>(methodInfo);
  ControlFlowGraph* inlinedCfg = mi->compilationInstance()->cfg();
  TypeContext* inlinedTypeContext = mi->compilationInstance()->typeContext();

  CodeDuplicator dup;
  dup.incorporate(inlinedCfg, inlinedTypeContext, cfg_, typeContext_);

  // Duplicate the arguments
  BlockBuilder entryBuilder(cfg_, scope_, nullptr, entry);
  auto i = op->begin();
  auto end = op->end();
  auto arg = inlinedCfg->constInputBegin();
  auto argEnd = inlinedCfg->constInputEnd();
  for (; arg != argEnd; ++arg, ++i) {
    Variable* newArg = dup.duplicatedVariableOf(*arg);
    entryBuilder.add(new OpcodeCopy(nullptr, newArg, *i));
  }

  // Set the inlined method's entry env
  Variable* curEnv = op->lookupOpcode()->env();
  Variable* entryEnv = dup.duplicatedVariableOf(inlinedCfg->entryEnv());
  typeContext_->updateTypeConstraint(entryEnv, TypeSameAs(typeContext_, curEnv));

  // Jump to the duplicated method's entry block
  entryBuilder.addJump(nullptr, dup.entry());

  // Begin at the duplicated method's exit block
  BlockBuilder exitBuilder(cfg_, scope_, nullptr, dup.exit());

  // Set the output variable
  if (op->lhs()) {
    OpcodeCopy* copy = new OpcodeCopy(nullptr, result, dup.duplicatedVariableOf(inlinedCfg->output()));
    if (!result) {
      result = op->lhs()->copy(exitBuilder.block(), copy);
      cfg_->addVariable(result);
      typeContext_->addNewTypeConstraint(result, typeContext_->typeConstraintOf(op->lhs())->clone());
      copy->setLhs(result);
    }
    exitBuilder.add(copy);
  }

  // Set the env variable
  Variable* env = dup.duplicatedVariableOf(inlinedCfg->exitEnv());
  if (exitEnv) {
    exitBuilder.add(new OpcodeCopy(nullptr, exitEnv, env));
    env = exitEnv;
  }

  work_.push_back(dup.entry());

  return std::make_tuple(result, env, exitBuilder.block());
}

std::tuple<Variable*, Variable*, Block*>
Inliner::insertCall(mri::MethodEntry me, Block* entry, OpcodeCall* op)
{
  // OpcodeLookup
  OpcodeLookup* lookup = op->lookupOpcode();
  BlockBuilder builder(cfg_, scope_, nullptr, entry);
  Variable* newLookup = builder.add(new OpcodeLookup(nullptr, nullptr, lookup->receiver(), lookup->methodName(), lookup->env(), me));
  if (!me.isNull()) {
    typeContext_->addNewTypeConstraint(newLookup, TypeConstant::create(reinterpret_cast<VALUE>(me.ptr())));
  }
  else {
    typeContext_->addNewTypeConstraint(newLookup, TypeLookup::create());
  }

  // OpcodeCall
  Opcode* call = duplicateCall(op, newLookup, builder.block());
  builder.addWithoutLhsAssigned(call);

  return std::make_tuple(call->lhs(), call->outEnv(), builder.block());
}

OpcodeCall*
Inliner::duplicateCall(OpcodeCall* source, Variable* lookup, Block* defBlock)
{
  OpcodeCall* op = new OpcodeCall(source->sourceLocation(), nullptr, lookup, source->rhsCount(), nullptr);

  if (source->lhs()) {
    Variable* lhs = source->lhs()->copy(defBlock, op);
    cfg_->addVariable(lhs);
    typeContext_->addNewTypeConstraint(lhs, TypeAny::create());
    op->setLhs(lhs);
  }

  Variable* outEnv = source->outEnv()->copy(defBlock, op);
  cfg_->addVariable(outEnv);
  typeContext_->addNewTypeConstraint(outEnv, TypeEnv::create());
  op->setOutEnv(outEnv);

  for (int i = 0; i < source->rhsCount(); ++i) {
    op->setRhs(i, source->rhs(i));
  }

  return op;
}

RBJIT_NAMESPACE_END
