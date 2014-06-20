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
  for (int i = 0; i < cfg_->blocks()->size(); ++i) {
    BlockHeader* block = (*cfg_->blocks())[i];
    Opcode* op = block;
    Opcode* footer = block->footer();
    do {
      if (typeid(*op) == typeid(OpcodeCall)) {
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
  TypeLookup* lookup = static_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lookup()));
  assert(typeid(*lookup) == typeid(TypeLookup));

  if (!lookup->isDetermined() || lookup->candidates().size() != 1) {
    return false;
  }

  mri::MethodEntry me = lookup->candidates()[0];
  if (!me.methodDefinition().hasAstNode()) {
    return false;
  }

  PrecompiledMethodInfo* mi = (PrecompiledMethodInfo*)me.methodDefinition().methodInfo();

  replaceCallWithMethodBody(mi, block, op);

  delete op;

  return true;
}

void
Inliner::replaceCallWithMethodBody(PrecompiledMethodInfo* mi, BlockHeader* block, OpcodeCall* op)
{
  CodeDuplicator dup(mi->cfg(), mi->typeContext(), cfg_, typeContext_);
  dup.duplicateCfg();

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());

  BlockHeader* latter = cfg_->splitBlock(block, op);
  BlockHeader* initBlock = cfg_->insertEmptyBlockAfter(block);

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

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  assert(cfg_->checkSanityAndPrintErrors());
}

RBJIT_NAMESPACE_END
