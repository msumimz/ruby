#include "rbjit/inliner.h"
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
  RBJIT_DPRINTF(("----------Call Site: (%Ix %Ix) '%s'\n",
    block, op, mri::Id(op->lookupOpcode()->methodName()).name()));

  TypeLookup* lookup = static_cast<TypeLookup*>(typeContext_->typeConstraintOf(op->lookup()));
  assert(typeid(*lookup) == typeid(TypeLookup));

  std::vector<mri::MethodEntry> mes;
  std::vector<mri::Class> cases;
  for (auto i = lookup->candidates().cbegin(), end = lookup->candidates().cend(); i != end; ++i) {
    mri::MethodEntry me = *i;
    if (me.methodDefinition().hasAstNode()) {
      mes.push_back(me);
      cases.push_back(me.class_());
    }
  }

  if (cases.empty()) {
    return false;
  }

  bool otherwise = !lookup->isDetermined();
  if (!otherwise && cases.size() == 1) {
    BlockHeader* latter = cfg_->splitBlock(block, op, true);
    BlockHeader* initBlock = cfg_->insertEmptyBlockAfter(block);
    replaceCallWithMethodBody(mes[0], initBlock, latter, op, op->lhs());
  }
  else {
    return false;
    // OpcodeMultiplexer mul(cfg_);
    // BlockHeader* exitBlock = mul.multiplex(block, op, op->receiver(), cases, otherwise);
  }

  delete op;

  RBJIT_DPRINT(cfg_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintVariables());
  RBJIT_DPRINT(typeContext_->debugPrint());
  RBJIT_DPRINT(cfg_->debugPrintAsDot());
  assert(cfg_->checkSanityAndPrintErrors());

  return true;
}

void
Inliner::replaceCallWithMethodBody(mri::MethodEntry me, BlockHeader* entry, BlockHeader* exit, OpcodeCall* op, Variable* result)
{
  PrecompiledMethodInfo* mi = static_cast<PrecompiledMethodInfo*>(me.methodDefinition().methodInfo());

  CodeDuplicator dup(mi->cfg(), mi->typeContext(), cfg_, typeContext_);
  dup.duplicateCfg();

  // Duplicate the arguments
  OpcodeFactory entryFactory(cfg_, entry, entry);
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
  entryFactory.addEnv(op->env(), true);
  entryFactory.addJump(dup.entry());

  OpcodeFactory exitFactory(cfg_, dup.exit(), dup.exit()->footer());
  if (result) {
    exitFactory.addCopy(result, dup.duplicatedVariableOf(mi->cfg()->output()), true);
  }
  exitFactory.addJump(exit);
}

RBJIT_NAMESPACE_END
