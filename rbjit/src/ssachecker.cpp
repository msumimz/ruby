#include "rbjit/ssachecker.h"
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/domtree.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

SsaChecker::SsaChecker(const ControlFlowGraph* cfg)
  : cfg_(cfg)
{}

void
SsaChecker::check()
{
  // Define input variables
  for (auto i = cfg_->inputBegin(), end = cfg_->inputEnd(); i != end; ++i) {
    variables_.insert(*i);
  }

  LTDominatorFinder domFinder(cfg_);
  std::vector<Block*> doms = domFinder.dominators();
  DomTree domTree(cfg_, doms);

  std::vector<DomTree::Node*> work;
  DomTree::Node* node = domTree.nodeOf(cfg_->entryBlock());
  work.push_back(node);

  while (!work.empty()) {
    DomTree::Node* node = work.back();
    work.pop_back();

    checkBlock(domTree.blockOf(node));

    for (node = node->firstChild(); node; node = node->nextSibling()) {
      work.push_back(node);
    }
  }
}

void
SsaChecker::checkBlock(Block* block)
{
  for (auto i = block->begin(), end = block->end(); i != end; ++i) {
    Opcode* op = *i;
    if (typeid(*op) != typeid(OpcodePhi)) {
      for (auto i = op->begin(), end = op->end(); i < end; ++i) {
        Variable* rhs = *i;
        if (rhs && variables_.find(rhs) == variables_.end()) {
          errors_.push_back(stringFormat("The use of variable %Ix at block %Ix, opcode %Ix is not dominated by its definition", rhs, block, op));
        }
      }
    }

    Variable* lhs = op->lhs();
    if (lhs) {
      auto result = variables_.insert(lhs);
      if (!result.second) {
        errors_.push_back(stringFormat("Variable %Ix at block %Ix, opcode %Ix is defined twice", lhs, block, op));
      }
    }
    Variable* outEnv = op->outEnv();
    if (outEnv) {
      auto result = variables_.insert(outEnv);
      if (!result.second) {
        errors_.push_back(stringFormat("Env variable %Ix at block %Ix, opcode %Ix is defined twice", outEnv, block, op));
      }
    }
  }

  // Check phi nodes of the succeeding blocks
  checkPhiNodeOfSucceedingBlock(block, block->nextBlock());
  checkPhiNodeOfSucceedingBlock(block, block->nextAltBlock());
}

void
SsaChecker::checkPhiNodeOfSucceedingBlock(Block* block, Block* succ)
{
  if (!succ || typeid(*succ->begin()) != typeid(OpcodePhi)) {
    return;
  }

  int index = succ->backedgeIndexOf(block);
  assert(index >= 0);

  for (auto i = succ->begin(), end = succ->end(); i != end; ++i) {
    OpcodePhi* phi = dynamic_cast<OpcodePhi*>(*i);
    if (!phi) {
      break;
    }
    Variable* v = phi->rhs(index);
    if (v && variables_.find(v) == variables_.end()) {
      errors_.push_back(stringFormat("The use of the %d-th variable %Ix of the phi node %Ix at block %Ix is not dominated by its definition", index, v, phi, block));
    }
  }
}

RBJIT_NAMESPACE_END
