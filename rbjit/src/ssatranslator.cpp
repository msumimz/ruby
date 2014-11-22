#include <algorithm>
#include <cassert>
#include "rbjit/ssatranslator.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/domtree.h"
#include "rbjit/variable.h"
#include "rbjit/debugprint.h"
#include "rbjit/idstore.h"
#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

SsaTranslator::SsaTranslator(ControlFlowGraph* cfg, DefInfoMap* defInfoMap, DomTree* domTree, bool doCopyFolding)
: cfg_(cfg), defInfoMap_(defInfoMap),
    domTree_(domTree), doCopyFolding_(doCopyFolding),
    df_(cfg_->blockCount()),
    phiInserted_(cfg_->blockCount(), 0),
    processed_(cfg_->blockCount(), 0),
    renameStack_(cfg_->variableCount())
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
  for (int i = 0; i < cfg_->blockCount(); ++i) {
    df_[i].assign(cfg_->blockCount(), false);
  }

  for (auto i = cfg_->begin(), end = cfg_->end(); i != end; ++i) {
    Block* b = *i;

    // Skip entry blocks
    if (b == cfg_->entryBlock()) {
      continue;
    }

    if (b->backedgeCount() >= 2) {
      Block* baseDom = domTree_->idomOf(b);
      for (auto e = b->backedgeBegin(), eend = b->backedgeEnd(); e != eend; ++e) {
        Block* runner = *e;
        while (runner != baseDom) {
          df_[runner->index()][b->index()] = true;
          runner = domTree_->idomOf(runner);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////
// Phi insertion

void
SsaTranslator::insertPhiFunctions()
{
  size_t blockCount = cfg_->blockCount();

  for (auto i = cfg_->variableBegin(), end = cfg_->variableEnd(); i != end; ++i) {
    Variable* v = *i;
    DefInfo* di = defInfoMap_->find(v);
    if (di == nullptr) {
      continue;
    }
    const DefSite* dsList = di->defSite();

    if (di->isLocal() && dsList->next() == 0) {
      // Insertion of phi functions is not needed if every definition is in the same block
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
  size_t blockCount = cfg_->blockCount();
  std::vector<bool>& df = df_[blockIndex];

  for (size_t i = 0; i < blockCount; ++i) {
    if (!df[i]) {
      continue;
    }

    // Insert phi node to the dominance frontier of the definition site
    if (phiInserted_[i] != v) {
      insertSinglePhiFunction(cfg_->block(i), v);
      phiInserted_[i] = v;
    }

    // Add a definition site because phi function is a definition itself
    if (processed_[i] != v) {
      processed_[i] = v;
      insertPhiFunctionsForSingleDefSite(i, v);
    }
  }
}

void
SsaTranslator::insertSinglePhiFunction(Block* block, Variable* v)
{
  int size = block->backedgeCount();
  assert(0 < size && size < 100);

  OpcodePhi* phi = new OpcodePhi(nullptr, v, size, block);
  block->insertOpcode(block->begin(), phi);
  for (Variable** i = phi->begin(); i < phi->end(); ++i) {
    *i = v;
  }
  defInfoMap_->find(v)->increaseDefCount();
}

////////////////////////////////////////////////////////////
// Variable renaming

void
SsaTranslator::renameVariables()
{
  // Set renameStack_ of arguments
  for (auto i = cfg_->constInputBegin(), end = cfg_->constInputEnd(); i != end; ++i) {
    renameStack_[(*i)->index()].push_back(*i);
  }

  // Do rename
  renameVariablesForSingleBlock(cfg_->entryBlock());

  // Delete variables deleted through copy propagation
  cfg_->removeVariables(folded_);

  // Reset the definition sites of the input variables. The input variables
  // don't have any actual definitions, so that their definition sites have no
  // chance to be updated during SSA transformation.
  for (auto i = cfg_->constInputBegin(), end = cfg_->constInputEnd(); i != end; ++i) {
    (*i)->setDefBlock(cfg_->entryBlock());
    (*i)->setDefOpcode(0);
  }
}

void
SsaTranslator::renameVariablesForSingleBlock(Block* b)
{
  size_t varSize = cfg_->variableCount();

  // Remember the depths of renameStack_ when entering this method
  int* depths = new int[varSize];
  for (size_t i = 0; i < varSize; ++i) {
    depths[i] = renameStack_[cfg_->variable(i)->index()].size();
  }

  Opcode* op = nullptr;
  for (auto i = b->begin(), end = b->end(); i != end; ++i) {
loop_head:
    op = *i;

    RBJIT_DPRINTF(("block: %Ix opcode: %Ix\n", b, op));
    // Rename rhs (excluding phi fucntions)
    if (typeid(*op) != typeid(OpcodePhi)) {
      renameVariablesInRhs(op);
    }

    // Rename lhs (including phi functions)
    Variable* lhs = op->lhs();
    if (!lhs) {
      continue;
    }
    OpcodeCopy* copy;
    if (doCopyFolding_ && (copy = dynamic_cast<OpcodeCopy*>(op)) &&
        lhs != cfg_->output() && !OpcodeEnv::isEnv(lhs) &&
        lhs->nameRef() == copy->rhs()->nameRef()) {
      // Copy propagation
      renameStack_[lhs->index()].push_back(copy->rhs());
      DefInfo* di = defInfoMap_->find(lhs);
      if (di->defCount() == 1) {
        folded_.push_back(lhs);
      }
      else {
        di->decreaseDefCount();
      }
      i = b->removeOpcode(i);
      goto loop_head;
    }
    else {
      assert(dynamic_cast<OpcodeL*>(op)); // op should be an OpcodeL if lhs isn't null
      renameVariablesInLhs(b, static_cast<OpcodeL*>(op), lhs);
      renameEnvInLhs(b, static_cast<OpcodeL*>(op));
    }
  }

  // Rename rhs of phi functions
  Block* successor = b->nextBlock();
  if (successor) {
    renameRhsOfPhiFunctions(b, successor);
  }
  successor = b->nextAltBlock();
  if (successor) {
    renameRhsOfPhiFunctions(b, successor);
  }

  // Go on renaming recursively in child nodes of the dominance tree
  DomTree::Node* n = domTree_->nodeOf(b)->firstChild();
  for (; n; n = n->nextSibling()) {
    Block* child = cfg_->block(domTree_->blockIndexOf(n));
    renameVariablesForSingleBlock(child);
  }

  // Clean up the values pushed into renameStack_ during this method
  for (size_t i = 0; i < varSize; ++i) {
    renameStack_[cfg_->variable(i)->index()].resize(depths[i]);
  }

  delete[] depths;
}

void
SsaTranslator::renameVariablesInLhs(Block* b, OpcodeL* opl, Variable* lhs)
{
  if (!lhs) {
    return;
  }

  DefInfo* di = defInfoMap_->find(lhs);
  if (di->defCount() > 1) {
    Variable* temp = lhs->copy(b, opl);
    cfg_->addVariable(temp);
    di->decreaseDefCount();
    renameStack_[lhs->index()].push_back(temp);
    if (OpcodeEnv::isEnv(lhs)) {
      if (b == cfg_->entryBlock()) {
        cfg_->setEntryEnv(temp);
      }
      else if (b == cfg_->exitBlock()) {
        cfg_->setExitEnv(temp);
      }
    }
    opl->setLhs(temp);

    renameStack_.push_back(std::vector<Variable*>());
  }
  else {
    renameStack_[lhs->index()].push_back(lhs);
    // The phi function's lhs variable's definition site should be updated
    lhs->setDefBlock(b);
    lhs->setDefOpcode(opl);
  }
}

void
SsaTranslator::renameEnvInLhs(Block* b, Opcode* op)
{
  Variable* env = op->outEnv();
  if (!env) {
    return;
  }

  DefInfo* di = defInfoMap_->find(env);
  if (di->defCount() > 1) {
    Variable* temp = env->copy(b, op);
    cfg_->addVariable(temp);
    di->decreaseDefCount();
    renameStack_[env->index()].push_back(temp);
    op->setOutEnv(temp);

    renameStack_.push_back(std::vector<Variable*>());
  }
  else {
    renameStack_[env->index()].push_back(env);
    env->setDefBlock(b);
    env->setDefOpcode(op);
  }
}

void
SsaTranslator::renameVariablesInRhs(Opcode* op)
{
  Variable** rhsEnd = op->end();
  for (Variable** i = op->begin(); i < rhsEnd; ++i) {
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
SsaTranslator::renameRhsOfPhiFunctions(Block* parent, Block* b)
{
  // Find the order in which the parent block happens in the backedge
  int c = 0;
  for (auto i = b->backedgeBegin(), end = b->backedgeEnd(); i != end; ++i, ++c) {
    if (*i == parent) {
      break;
    }
  }

  // Rename variables in phi nodes
  for (auto i = b->begin(), end = b->end(); i != end; ++i) {
    OpcodePhi* phi = dynamic_cast<OpcodePhi*>(*i);
    if (!phi) {
      break;
    }
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
  std::string result = stringFormat("[Dominance Frontier: %x]\n", df_);

  for (size_t i = 0; i < cfg_->blockCount(); ++i) {
    const std::vector<bool>& ba = df_[i];
    result += stringFormat("%d: size=%d df=", i, ba.size());
    for (size_t j = 0; j < cfg_->blockCount(); ++j) {
      if (ba[j]) {
        result += stringFormat("%d ", j);
      }
    }
    result += "\n";
  }

  return result;
}

RBJIT_NAMESPACE_END
