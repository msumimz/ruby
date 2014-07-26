#include "rbjit/inliner.h"
#include "rbjit/methodinfo.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typecontext.h"
#include "rbjit/codeduplicator.h"
#include "rbjit/opcodefactory.h"
#include "rbjit/debugprint.h"
#include "rbjit/opcodemultiplexer.h"
#include "rbjit/recompilationmanager.h"

RBJIT_NAMESPACE_BEGIN

Inliner::Inliner(PrecompiledMethodInfo* mi)
  : mi_(mi), cfg_(mi->cfg()), typeContext_(mi->typeContext()),
  visited_(cfg_->blocks()->size(), false)
{}

void
Inliner::doInlining()
{
  work_.push_back(cfg_->entry());

loop:
  while (!work_.empty()) {
    BlockHeader* block = work_.back();
    work_.pop_back();
    visited_[block->index()] = true;

    Opcode* footer = block->footer();
    for (Opcode* op = block->next(); op != footer; op = op->next()) {
      OpcodeCall* call = dynamic_cast<OpcodeCall*>(op);
      if (call) {
        ID methodName = call->lookupOpcode()->methodName();
        if (inlineCallSite(block, call)) {
          visited_.resize(cfg_->blocks()->size());
          RecompilationManager::instance()->addCalleeCallerRelation(methodName, mi_);
          goto loop;
        }
      }
    }

    BlockHeader* b = footer->nextBlock();
    if (b && !visited_[b->index()]) {
      work_.push_back(b);
    }
    b = footer->nextAltBlock();
    if (b && !visited_[b->index()]) {
      work_.push_back(b);
    }
  }
}

bool
Inliner::inlineCallSite(BlockHeader* block, OpcodeCall* op)
{
  RBJIT_DPRINTF(("----------Call Site: (%Ix %Ix) '%s'\n",
    block, op, mri::Id(op->lookupOpcode()->methodName()).name()));

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
    BlockHeader* join = cfg_->splitBlock(block, op, true, true);
    join->setDebugName("inliner_join");
    BlockHeader* initBlock = cfg_->insertEmptyBlockAfter(block);
    initBlock->setDebugName("inliner_arguments");
    replaceCallWithMethodBody(methodInfos[0], initBlock, join, op, op->lhs(), op->outEnv());
  }
  else {
    OpcodeMultiplexer mul(cfg_, typeContext_);
    BlockHeader* exitBlock = mul.multiplex(block, op, op->receiver(), cases, otherwise);

    OpcodePhi* phi = mul.phi();
    OpcodePhi* envPhi = mul.envPhi();
    int size = methodInfos.size() + otherwise;
    int otherwiseIndex = size - 1;
    for (int i = 0; i < size; ++i) {
      BlockHeader* block = mul.segments()[i];
      BlockHeader* join = cfg_->splitBlock(block, block, false, false);
      join->setDebugName("inliner_join");

      std::pair<Variable*, Variable*> result;
      if (i == otherwiseIndex) {
        // Otherwise branch
        result = insertCall(mri::MethodEntry(), block, join, op);
      }
      else if (inlinable[i]) {
        result = replaceCallWithMethodBody(methodInfos[i], block, join, op, 0, 0);
      }
      else {
        result = insertCall(methodInfos[i]->methodEntry(), block, join, op);
      }

      int index = -1;
      if (result.first && phi) {
        index = exitBlock->backedgeIndexOf(join);
        assert(0 <= index && index < size);
        phi->setRhs(index, result.first);
      }
      if (envPhi) {
        assert(result.second);
        if (index == -1) {
          index = exitBlock->backedgeIndexOf(join);
          assert(0 <= index && index < size);
        }
        envPhi->setRhs(index, result.second);
      }
    }
  }

  removeOpcodeCall(op);

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintDotHeader());
  RBJIT_DPRINT(cfg_->debugPrintAsDot());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_DPRINT(typeContext_->debugPrint());
  assert(cfg_->checkSanityAndPrintErrors());
  assert(cfg_->checkSsaAndPrintErrors());

  return true;
}

std::pair<Variable*, Variable*>
Inliner::replaceCallWithMethodBody(MethodInfo* methodInfo, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result, Variable* exitEnv)
{
  // entry should not be terminated
  assert(!entry->footer()->isTerminator());

  assert(typeid(*methodInfo) == typeid(PrecompiledMethodInfo));
  PrecompiledMethodInfo* mi = static_cast<PrecompiledMethodInfo*>(methodInfo);

  CodeDuplicator dup;
  dup.incorporate(mi->cfg(), mi->typeContext(), cfg_, typeContext_);

  // Duplicate the arguments
  OpcodeFactory entryFactory(cfg_, entry, entry->footer());
  auto i = op->rhsBegin();
  auto end = op->rhsEnd();
  auto arg = mi->cfg()->inputs()->cbegin();
  auto argEnd = mi->cfg()->inputs()->cend();
  for (; arg != argEnd; ++arg, ++i) {
    Variable* newArg = dup.duplicatedVariableOf(*arg);
    entryFactory.addCopy(newArg, *i, true);
  }

  // Set the inlined method's entry env
  Variable* curEnv = op->lookupOpcode()->env();
  Variable* entryEnv = dup.duplicatedVariableOf(mi->cfg()->entryEnv());
  typeContext_->updateTypeConstraint(entryEnv, TypeSameAs(typeContext_, curEnv));

  // Jump to the duplicated method's entry block
  entryFactory.addJump(dup.entry());

  // Begin at the duplicated method's exit block
  OpcodeFactory exitFactory(cfg_, dup.exit(), dup.exit()->footer());

  // Set the output variable
  if (op->lhs()) {
    exitFactory.addCopy(result, dup.duplicatedVariableOf(mi->cfg()->output()), true);
    if (!result) {
      result = cfg_->copyVariable(exitFactory.lastBlock(), exitFactory.lastOpcode(), op->lhs());
      typeContext_->addNewTypeConstraint(result, typeContext_->typeConstraintOf(op->lhs())->clone());
      static_cast<OpcodeL*>(exitFactory.lastOpcode())->setLhs(result);
    }
  }

  // Set the env variable
  Variable* env = dup.duplicatedVariableOf(mi->cfg()->exitEnv());
  if (exitEnv) {
    exitFactory.addCopy(exitEnv, env, true);
    env = exitEnv;
  }

  exitFactory.addJump(exit);

  work_.push_back(dup.entry());

  return std::make_pair(result, env);
}

std::pair<Variable*, Variable*>
Inliner::insertCall(mri::MethodEntry me, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op)
{
  // OpcodeLookup
  OpcodeLookup* lookup = op->lookupOpcode();
  OpcodeFactory factory(cfg_, entry, entry->footer());
  Variable* newLookup = factory.addLookup(lookup->receiver(), lookup->methodName(), me);
  if (!me.isNull()) {
    typeContext_->addNewTypeConstraint(newLookup, TypeConstant::create(reinterpret_cast<VALUE>(me.ptr())));
  }
  else {
    typeContext_->addNewTypeConstraint(newLookup, TypeLookup::create());
  }

  // OpcodeCall
  Variable* result = factory.addDuplicateCall(op, newLookup, !!op->lhs());
  Variable* env = static_cast<OpcodeCall*>(factory.lastOpcode())->outEnv();
  if (result) {
    typeContext_->addNewTypeConstraint(result, TypeAny::create());
  }
  typeContext_->addNewTypeConstraint(env, TypeEnv::create());

  factory.addJump(exit);

  return std::make_pair(result, env);
}

void
Inliner::removeOpcodeCall(OpcodeCall* op)
{
  // Remove OpcodeLookup
  // The variable does not be removed; instead, its defOpode is cleared to zero.
  op->lookup()->defOpcode()->unlink();
  delete op->lookup()->defOpcode();
  op->lookup()->setDefOpcode(0);

  // Remove OpcodeCall
  delete op;
}

RBJIT_NAMESPACE_END
