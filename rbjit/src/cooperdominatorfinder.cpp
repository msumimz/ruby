#include <cassert>
#include "rbjit/cooperdominatorfinder.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"
#include "rbjit/block.h"

RBJIT_NAMESPACE_BEGIN

CooperDominatorFinder::CooperDominatorFinder(const ControlFlowGraph* cfg)
  : cfg_(cfg),
    idoms_(cfg->blockCount(), 0),
    dfnums_(cfg->blockCount(), 0)
{}

std::vector<Block*>
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
  Block* entry = cfg_->entryBlock();
  do {
    changed = false;
    // loop by reverse postorder
    for (auto i = postorders_.rbegin(), end = postorders_.rend(); i != end; ++i) {
      Block* b = *i;
      if (b == cfg_->entryBlock()) {
        continue;
      }
      assert(b->backedgeCount() > 0);
      Block* newIdom = *b->backedgeBegin();
      for (auto j = b->backedgeBegin(), jend = b->backedgeEnd(); j != jend; ++j) {
        if (idoms_[(*j)->index()]) {
          newIdom = findIntersect(*j, newIdom);
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
  postorders_.reserve(cfg_->blockCount());
  computeDfsOrderInternal(cfg_->entryBlock(), 0);
}

int
CooperDominatorFinder::computeDfsOrderInternal(Block* b, int count)
{
  if (dfnums_[b->index()] > 0) {
    return count;
  }

  dfnums_[b->index()] = count++;

  Block* n = b->nextBlock();
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

Block*
CooperDominatorFinder::findIntersect(Block* b1, Block* b2)
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
