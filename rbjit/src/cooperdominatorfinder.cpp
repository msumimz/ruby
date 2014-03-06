#include <cassert>
#include "rbjit/cooperdominatorfinder.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

CooperDominatorFinder::CooperDominatorFinder(ControlFlowGraph* cfg)
  : DominatorFinder(cfg),
    dfnums_(blocks_->size(), 0),
    idoms_(blocks_->size(), 0)
{}

std::vector<BlockHeader*>
CooperDominatorFinder::dominators()
{
  findDominators();
  return idoms_;
}

void
CooperDominatorFinder::findDominators()
{
  computeDfsOrder();

  bool changed;
  BlockHeader* entry = cfg_->entry();
  do {
    changed = false;
    for (int i = 1; i < blocks_->size(); ++i) {
      BlockHeader* b = (*blocks_)[i];
      BlockHeader::Backedge* e = b->backedge();
      BlockHeader* newIdom = e->block();
      for (e = e->next(); e; e = e->next()) {
        if (e->block() == entry || idoms_[e->block()->index()]) {
          newIdom = findIntersect(e->block(), newIdom);
        }
      }
      if (idoms_[i] != newIdom) {
        idoms_[i] = newIdom;
        changed = true;
      }
    }
  } while (changed);
}

void
CooperDominatorFinder::computeDfsOrder()
{
  std::vector<BlockHeader*> work;

  work.push_back(cfg_->entry());
  int count = 0;
  while (!work.empty()) {
    BlockHeader* b = work.back();
    work.pop_back();

    if (dfnums_[b->index()] > 0) {
      continue;
    }

    dfnums_[b->index()] = count++;

    BlockHeader* n = b->nextAltBlock();
    if (n) {
      work.push_back(n);
    }
    n = b->nextBlock();
    if (n) {
      work.push_back(n);
    }
  }
}

BlockHeader*
CooperDominatorFinder::findIntersect(BlockHeader* b1, BlockHeader* b2)
{
  while (b1 != b2) {
    assert(dfnums_[b1->index()] != dfnums_[b2->index()]);
    while (dfnums_[b1->index()] > dfnums_[b2->index()]) {
      if (idoms_[b1->index()]) {
        b1 = idoms_[b1->index()];
      }
      else {
        b1 = b2;
      }
    }
    while (dfnums_[b2->index()] > dfnums_[b1->index()]) {
      if (idoms_[b2->index()]) {
        b2 = idoms_[b2->index()];
      }
      else {
        b2 = b1;
      }
    }
  }
  return b1;
}

RBJIT_NAMESPACE_END
