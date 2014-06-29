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

RBJIT_NAMESPACE_BEGIN

Inliner::Inliner(PrecompiledMethodInfo* mi)
  : mi_(mi), cfg_(mi->cfg()), typeContext_(mi->typeContext())
{}

void
Inliner::doInlining()
{
  for (int i = 0; i < cfg_->blocks()->size(); ++i) {
    BlockHeader* block = (*cfg_->blocks())[i];
    Opcode* op = block;
    Opcode* footer = block->footer();
    do {
      if (typeid(*op) == typeid(OpcodeCall) && inlined_.find(op) == inlined_.end()) {
        if (inlineCallSite(block, static_cast<OpcodeCall*>(op))) {
          break;
        }
      }
      op = op->next();
    } while (op && op != footer);
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

  if (!otherwise && cases.size() == 1) {
    BlockHeader* join = cfg_->splitBlock(block, op, true, true);
    join->setDebugName("inliner_join");
    BlockHeader* initBlock = cfg_->insertEmptyBlockAfter(block);
    initBlock->setDebugName("inliner_arguments");
    replaceCallWithMethodBody(methodInfos[0], initBlock, join, op, op->lhs());
    removeOpcodeCall(op);
  }
  else {
    OpcodeMultiplexer mul(cfg_);
    BlockHeader* exitBlock = mul.multiplex(block, op, op->receiver(), cases, otherwise);

    OpcodePhi* phi = mul.phi();
    int size = methodInfos.size();
    for (int i = 0; i < size; ++i) {
      BlockHeader* block = mul.segments()[i];
      BlockHeader* join = cfg_->splitBlock(block, block, false, false);
      join->setDebugName("inliner_join");

      Variable* result;
      if (inlinable[i]) {
        result = replaceCallWithMethodBody(methodInfos[i], block, join, op, 0);
      }
      else {
        result = insertCall(methodInfos[i], block, join, op);
      }
      // TODO: phi node for envs
      phi->setRhs(i, result);
    }

    if (otherwise) {
      assert(mul.segments().size() == methodInfos.size() + 1);
      BlockHeader* block = mul.segments().back();
      op->insertAfter(block);
      op->env()->updateDefSite(block, op);
      inlined_.insert(op);

      if (op->lhs()) {
        Variable* lhs = cfg_->copyVariable(block, op, op->lhs());
        typeContext_->addNewTypeConstraint(lhs, typeContext_->typeConstraintOf(op->lhs())->independantClone());
        op->setLhs(lhs);
        phi->setRhs(methodInfos.size(), lhs);
      }
    }
    else {
      removeOpcodeCall(op);
    }
  }

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintDotHeader());
  RBJIT_DPRINT(cfg_->debugPrintAsDot());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_DPRINT(typeContext_->debugPrint());
  assert(cfg_->checkSanityAndPrintErrors());

  return true;
}

Variable*
Inliner::replaceCallWithMethodBody(MethodInfo* methodInfo, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result)
{
  // entry should not be terminated
  assert(!entry->footer()->isTerminator());

  assert(typeid(*methodInfo) == typeid(PrecompiledMethodInfo));
  PrecompiledMethodInfo* mi = static_cast<PrecompiledMethodInfo*>(methodInfo);

  CodeDuplicator dup(mi->cfg(), mi->typeContext(), cfg_, typeContext_);
  dup.duplicateCfg();

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

  // The opcode call implicitly defines the env, so that we need to define it
  // as explict opcode.
  Variable* env = entryFactory.addEnv(true);
  typeContext_->addNewTypeConstraint(env, typeContext_->typeConstraintOf(op->env())->clone());

  // Jump to the duplicated method's entry block
  entryFactory.addJump(dup.entry());

  // Begin at the duplicated method's exit block
  OpcodeFactory exitFactory(cfg_, dup.exit(), dup.exit()->footer());

  exitFactory.addCopy(result, dup.duplicatedVariableOf(mi->cfg()->output()), true);

  if (!result && op->lhs()) {
    result = cfg_->copyVariable(exitFactory.lastBlock(), exitFactory.lastOpcode(), op->lhs());
    typeContext_->addNewTypeConstraint(result, typeContext_->typeConstraintOf(op->lhs())->clone());
    static_cast<OpcodeL*>(exitFactory.lastOpcode())->setLhs(result);
  }

  exitFactory.addJump(exit);

  return result;
}

Variable*
Inliner::insertCall(MethodInfo* mi, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op)
{
  assert(typeid(*mi) == typeid(CMethodInfo));

  OpcodeLookup* lookup = op->lookupOpcode();
  OpcodeFactory factory(cfg_, entry, entry->footer());
  Variable* newLookup = factory.addLookup(lookup->receiver(), lookup->methodName(), mi->methodEntry());
  Variable* result = factory.addDuplicateCall(op, newLookup, !!op->lhs());

  // Set type constraints
  typeContext_->addNewTypeConstraint(newLookup, TypeConstant::create(reinterpret_cast<VALUE>(mi->methodEntry().ptr())));
  typeContext_->addNewTypeConstraint(result, TypeAny::create());
  typeContext_->addNewTypeConstraint(static_cast<OpcodeCall*>(result->defOpcode())->env(), TypeEnv::create());
  
  return result;
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
