#include <algorithm>
#include <cassert>
#include "rbjit/ssatranslator.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/domtree.h"
#include "rbjit/variable.h"
#include "rbjit/debugprint.h"

RBJIT_NAMESPACE_BEGIN

SsaTranslator::SsaTranslator(ControlFlowGraph* cfg, bool doCopyFolding)
  : cfg_(cfg), doCopyFolding_(doCopyFolding),
    domTree_(cfg->domTree()),
    df_(cfg_->blocks()->size()),
    phiInserted_(cfg_->blocks()->size(), 0),
    processed_(cfg_->blocks()->size(), 0),
    renameStack_(cfg_->variables()->size())
{}

SsaTranslator::~SsaTranslator()
{}

void
SsaTranslator::translate()
{
  // Compute dominance frontier
  computeDf();

  RBJIT_DPRINT(debugPrintDf());

  // Insert phi functions
  insertPhiFunctions();

  // Rename variables
  renameVariables();

#ifdef RBJIT_DEBUG
//  cfg_->debugCheckVariableDefinitions();
#endif
}

////////////////////////////////////////////////////////////
// Dominance frontier computation

void
SsaTranslator::computeDf()
{
  for (int i = 0; i < cfg_->blocks()->size(); ++i) {
    df_[i].assign(cfg_->blocks()->size(), false);
  }

  auto i = cfg_->blocks()->begin();
  auto end = cfg_->blocks()->end();
  for (; i != end; ++i) {
    BlockHeader* b = *i;

    // Skip entry and exit blocks
    if (b == cfg_->entry() || b == cfg_->exit()) {
      continue;
    }

    if (b->hasMultipleBackedges()) {
      BlockHeader::Backedge* e = b->backedge();
      do {
        BlockHeader* runner = e->block();
        while (runner && runner != b->idom()) {
          df_[runner->index()][b->index()] = true;
          runner = runner->idom();
        }
        e = e->next();
      } while (e);
    }
  }
}

////////////////////////////////////////////////////////////
// Phi insertion

void
SsaTranslator::insertPhiFunctions()
{
  size_t blockCount = cfg_->blocks()->size();

  auto i = cfg_->variables()->begin();
  auto end = cfg_->variables()->end();
  for (; i != end; ++i) {
    Variable* v = *i;
    DefInfo* di = v->defInfo();
    if (di == 0) {
      continue;
    }
    const DefSite* dsList = di->defSite();

    if (v->local() && dsList->next() == 0) {
      // No phi functions needed if every definition is in the same block
      continue;
    }

    for (const DefSite* ds = dsList; ds; ds = ds->next()) {
      processed_[ds->defBlock()->index()] = v;
    }

    for (const DefSite* ds = dsList; ds; ds = ds->next()) {
      insertPhiFunctionsForSingleDefSite(ds->defBlock()->index(), v);
    }
  }
}

void
SsaTranslator::insertPhiFunctionsForSingleDefSite(int blockIndex, Variable* v)
{
  size_t blockCount = cfg_->blocks()->size();
  std::vector<bool>& df = df_[blockIndex];

  for (size_t i = 0; i < blockCount; ++i) {
    if (!df[i]) {
      continue;
    }

    // Insert phi node to the dominance frontier of the definition site
    if (phiInserted_[i] != v) {
      insertSinglePhiFunction((*cfg_->blocks())[i], v);
      phiInserted_[i] = v;
    }

    // Add definition site because phi function is a definition itself
    if (processed_[i] != v) {
      processed_[i] = v;
      insertPhiFunctionsForSingleDefSite(i, v);
    }
  }
}

void
SsaTranslator::insertSinglePhiFunction(BlockHeader* block, Variable* v)
{
  int size = block->backedgeSize();
  assert(0 < size && size < 100);

  OpcodePhi* phi = new OpcodePhi(0, 0, block, v, size, block);
  for (Variable** i = phi->rhsBegin(); i < phi->rhsEnd(); ++i) {
    *i = v;
  }
  v->defInfo()->increaseDefCount();
}

////////////////////////////////////////////////////////////
// Variable renaming

void
SsaTranslator::renameVariables()
{
  // Do rename
  renameVariablesForSingleBlock(cfg_->entry());

  // Delete variables deleted through copy propagation
  cfg_->removeVariables(&folded_);
}

void
SsaTranslator::renameVariablesForSingleBlock(BlockHeader* b)
{
  size_t varSize = cfg_->variables()->size();

  // Remember the depths of renameStack_ when entering this method
  int* depths = new int[varSize];
  for (size_t i = 0; i < varSize; ++i) {
    depths[i] = renameStack_[(*cfg_->variables())[i]->index()].size();
  }

  Opcode* footer = b->footer();
  if (b != footer) {
    Opcode* prev = b;
    for (Opcode* op = b->next(); op; prev = op, op = op->next()) {
      // Rename rhs (excluding phi fucntions)
      if (typeid(*op) != typeid(OpcodePhi)) {
        renameVariablesInRhs(op);
      }

      // Rename lhs (including phi functions)
      OpcodeL* opl = dynamic_cast<OpcodeL*>(op);
      if (opl) {
        Variable* lhs = op->lhs();
        OpcodeCopy* copy;
        if (doCopyFolding_ && (copy = dynamic_cast<OpcodeCopy*>(op)) &&
            lhs != cfg_->output() && lhs != cfg_->env()) {
          // Copy propagation
          renameStack_[lhs->index()].push_back(copy->rhs());
          if (lhs->defCount() == 1) {
            folded_.push_back(lhs);
          }
          else {
            lhs->defInfo()->decreaseDefCount();
          }
          cfg_->removeOpcodeAfter(prev);
          op = prev;
        }
        else {
          renameVariablesInLhs(b, opl, lhs);
          renameEnvInLhs(b, opl);
        }
      }
      if (op == footer) {
        break;
      }
    }
  }

  // Rename rhs of phi functions
  BlockHeader* successor = footer->nextBlock();
  if (successor) {
    renameRhsOfPhiFunctions(b, successor);
  }
  successor = footer->nextAltBlock();
  if (successor) {
    renameRhsOfPhiFunctions(b, successor);
  }

  // Go on renaming recursively in child nodes of the dominance tree
  DomTree::Node* n = domTree_->nodeOf(b)->firstChild();
  for (; n; n = n->nextSibling()) {
    BlockHeader* child = (*cfg_->blocks())[domTree_->blockIndexOf(n)];
    renameVariablesForSingleBlock(child);
  }

  // Clean up the values pushed into renameStack_ during this method
  for (size_t i = 0; i < varSize; ++i) {
    renameStack_[(*cfg_->variables())[i]->index()].resize(depths[i]);
  }

  delete[] depths;
}

void
SsaTranslator::renameVariablesInLhs(BlockHeader* b, OpcodeL* opl, Variable* lhs)
{
  if (lhs->defCount() > 1) {
    Variable* temp = cfg_->copyVariable(b, opl, lhs);
    lhs->defInfo()->decreaseDefCount();
    renameStack_[lhs->index()].push_back(temp);
    if (lhs == cfg_->env()) {
      cfg_->setEnv(temp);
    }
    opl->setLhs(temp);

    renameStack_.push_back(std::vector<Variable*>());
  }
  else {
    renameStack_[lhs->index()].push_back(lhs);
  }
}

void
SsaTranslator::renameEnvInLhs(BlockHeader* b, Opcode* op)
{
  OpcodeCall* call = dynamic_cast<OpcodeCall*>(op);
  if (!call) {
    return;
  }

  Variable* env = call->env();
  if (env->defCount() > 1) {
    Variable* temp = cfg_->copyVariable(b, call, env);
    env->defInfo()->decreaseDefCount();
    renameStack_[env->index()].push_back(temp);
    call->setEnv(temp);

    renameStack_.push_back(std::vector<Variable*>());
  }
  else {
    renameStack_[env->index()].push_back(env);
  }
}

void
SsaTranslator::renameVariablesInRhs(Opcode* op)
{
  Variable** rhsEnd = op->rhsEnd();
  for (Variable** i = op->rhsBegin(); i < rhsEnd; ++i) {
    std::vector<Variable*>& stack = renameStack_[(*i)->index()];
    if (stack.empty()) {
      *i = cfg_->undefined();
    }
    else {
      *i = stack.back();
    }
  }
}

void
SsaTranslator::renameRhsOfPhiFunctions(BlockHeader* parent, BlockHeader* b)
{
  // Find the order in which the parent block happens in the backedge
  int c = 0;
  for (BlockHeader::Backedge* e = b->backedge(); e; ++c, e = e->next()) {
    if (e->block() == parent) {
      break;
    }
  }

  // Rename variables in phi nodes
  OpcodePhi* phi;
  for (Opcode* op = b->next(); op && (phi = dynamic_cast<OpcodePhi*>(op)); op = op->next()) {
    std::vector<Variable*>& stack = renameStack_[phi->rhs(c)->index()];
    if (stack.empty()) {
      phi->setRhs(c, cfg_->undefined());
    }
    else {
      phi->setRhs(c, stack.back());
    }
  }
}

std::string
SsaTranslator::debugPrintDf() const
{
  char buf[256];
  std::string result;

  sprintf(buf, "[Dominance Frontier: %x]\n", df_);
  result += buf;

  for (size_t i = 0; i < cfg_->blocks()->size(); ++i) {
    const std::vector<bool>& ba = df_[i];
    sprintf(buf, "%d: size=%d df=", i, ba.size());
    result += buf;
    for (size_t j = 0; j < cfg_->blocks()->size(); ++j) {
      if (ba[j]) {
        sprintf(buf, "%d ", j);
        result += buf;
      }
    }
    result += "\n";
  }

  return result;
}

RBJIT_NAMESPACE_END
