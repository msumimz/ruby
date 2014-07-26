#include <cassert>
#include "rbjit/cooperdominatorfinder.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

CooperDominatorFinder::CooperDominatorFinder(ControlFlowGraph* cfg)
  : DominatorFinder(cfg),
    idoms_(blocks_->size(), 0),
    dfnums_(blocks_->size(), 0)
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
    // loop by reverse postorder
    for (auto i = postorders_.rbegin(), end = postorders_.rend(); i != end; ++i) {
      BlockHeader* b = *i;
      if (b == cfg_->entry()) {
        continue;
      }
      BlockHeader::Backedge* e = b->backedge();
      BlockHeader* newIdom = e->block();
      for (; e; e = e->next()) {
        if (idoms_[e->block()->index()]) {
          newIdom = findIntersect(e->block(), newIdom);
        }
      }
      if (idoms_[b->index()] != newIdom) {
        idoms_[b->index()] = newIdom;
        changed = true;
      }
    }
  } while (changed);
}

void
CooperDominatorFinder::computeDfsOrder()
{
  postorders_.reserve(cfg_->blocks()->size());
  computeDfsOrderInternal(cfg_->entry(), 0);
}

int
CooperDominatorFinder::computeDfsOrderInternal(BlockHeader* b, int count)
{
  if (dfnums_[b->index()] > 0) {
    return count;
  }

  dfnums_[b->index()] = count++;

  BlockHeader* n = b->nextBlock();
  if (n) {
    count = computeDfsOrderInternal(n, count);
  }
  n = b->nextAltBlock();
  if (n) {
    count = computeDfsOrderInternal(n, count);
  }

  postorders_.push_back(b);

  return count;
}

BlockHeader*
CooperDominatorFinder::findIntersect(BlockHeader* b1, BlockHeader* b2)
{
  while (b1 != b2) {
    assert(dfnums_[b1->index()] != dfnums_[b2->index()]);
    while (dfnums_[b1->index()] > dfnums_[b2->index()]) {
      assert(idoms_[b1->index()]);
      b1 = idoms_[b1->index()];
    }
    while (dfnums_[b2->index()] > dfnums_[b1->index()]) {
      assert(idoms_[b2->index()]);
      b2 = idoms_[b2->index()];
    }
  }
  return b1;
}

RBJIT_NAMESPACE_END
