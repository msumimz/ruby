#include "rbjit/inliner.h"
#include "rbjit/rubymethod.h"
#include "rbjit/methodinfo.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typecontext.h"
#include "rbjit/codeduplicator.h"
#include "rbjit/opcodefactory.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

void
Inliner::doInlining()
{
  for (auto i = cfg_->blocks()->cbegin(), end = cfg_->blocks()->cend(); i != end; ++i) {
    block_ = *i;
    Opcode* op = block_->next();
    Opcode* footer = block_->footer();
    do {
      if (typeid(*op) == typeid(OpcodeCall)) {
        op = inlineCallSite(static_cast<OpcodeCall*>(op));
	footer = block_->footer(); // When inline is performed, block_ can be updated.
      }
      op = op->next();
    } while (op && op != footer);
  }

  RBJIT_ASSERT(cfg_->checkSanityAndPrintErrors());
}

Opcode*
Inliner::inlineCallSite(OpcodeCall* op)
{
  TypeLookup* lookup = static_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lookup()));
  assert(typeid(*lookup) == typeid(TypeLookup));

  if (!lookup->isDetermined() || lookup->candidates().size() != 1) {
    return op;
  }

  mri::MethodEntry me = lookup->candidates()[0];
  if (!me.methodDefinition().hasAstNode()) {
    return op;
  }

  PrecompiledMethodInfo* mi = (PrecompiledMethodInfo*)me.methodDefinition().methodInfo();

  CodeDuplicator dup(mi->cfg(), mi->typeContext(), cfg_, typeContext_);
  dup.duplicateCfg();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());

  BlockHeader* latter = cfg_->splitBlock(block_, op);
  BlockHeader* initBlock = cfg_->insertEmptyBlockAfter(block_);

  // Duplicate the arguments
  OpcodeFactory entryFactory(cfg_, initBlock, initBlock);
  auto i = op->rhsBegin();
  auto end = op->rhsEnd();
  auto arg = mi->cfg()->inputs()->cbegin();
  auto argEnd = mi->cfg()->inputs()->cend();
  for (; arg != argEnd; ++arg, ++i) {
    Variable* newArg = dup.duplicatedVariableOf(*arg);
    entryFactory.addCopy(newArg, *i, true);
  }

  // The opcode call implicitly defines the env, so that we need define an
  // explict opcode.
  entryFactory.addEnv(op->env(), true);

  entryFactory.addJump(dup.entry());

  OpcodeFactory exitFactory(cfg_, dup.exit(), dup.exit()->footer());
  if (op->lhs()) {
    // Duplicate the return value
    exitFactory.addCopy(op->lhs(), dup.duplicatedVariableOf(cfg_->output()), true);
  }
  exitFactory.addJump(latter);

  delete op;

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  assert(cfg_->checkSanityAndPrintErrors());

  block_ = latter;
  return latter;
}

RBJIT_NAMESPACE_END
