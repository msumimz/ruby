#include "rbjit/inliner.h"
#include "rbjit/rubymethod.h"
#include "rbjit/methodinfo.h"
#include "rbjit/opcode.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/typecontext.h"
#include "rbjit/codeduplicator.h"
#include "rbjit/opcodefactory.h"

RBJIT_NAMESPACE_BEGIN

void
Inliner::doInlining()
{
  for (auto i = cfg_->blocks()->cbegin(), end = cfg_->blocks()->cend(); i != end; ++i) {
    block_ = *i;
    Opcode* op = block_;
    Opcode* footer = block_->footer();
    do {
      op = op->next();
      if (typeid(*op) == typeid(OpcodeCall)) {
        inlineCallSite(static_cast<OpcodeCall*>(op));
      }
    } while (op && op != footer);
  }

  RBJIT_ASSERT(cfg_->checkSanityAndPrintErrors());
}

void
Inliner::inlineCallSite(OpcodeCall* op)
{
  TypeLookup* lookup = static_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lookup()));
  assert(typeid(*lookup) == typeid(TypeLookup));

  if (!lookup->isDetermined() || lookup->candidates().size() != 1) {
    return;
  }

  mri::MethodEntry me = lookup->candidates()[0];
  if (!me.methodDefinition().hasAstNode()) {
    return;
  }

  PrecompiledMethodInfo* mi = (PrecompiledMethodInfo*)me.methodDefinition().methodInfo();

  CodeDuplicator dup(mi->cfg(), mi->typeContext(), cfg_, typeContext_);
  dup.duplicateCfg();

  BlockHeader* latter = cfg_->splitBlock(block_, op);
  BlockHeader* initBlock = cfg_->insertEmptyBlockAfter(block_);

  // Duplicate the arguments
  OpcodeFactory entryFact(cfg_, initBlock, initBlock);
  for (auto i = op->rhsBegin(), end = op->rhsEnd(); i < end; ++i) {
    Variable* v = dup.duplicatedVariableOf(*i);
    entryFact.addCopy(v, *i, true);
    v->updateDefSite(block_, op);
  }
  entryFact.addJump(dup.entry());

  // Duplicate the return value
  if (op->lhs()) {
    OpcodeFactory exitFact(cfg_, dup.exit(), dup.exit());
    exitFact.addCopy(op->lhs(), dup.duplicatedVariableOf(cfg_->output()), true);
    op->lhs()->updateDefSite(block_, op);
  }

  static_cast<OpcodeJump*>(block_->footer())->setDest(initBlock);

  delete op;
}

RBJIT_NAMESPACE_END
