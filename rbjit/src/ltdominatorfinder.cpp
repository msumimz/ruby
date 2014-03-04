#include <cassert>
#include <algorithm>
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"

#ifdef RBJIT_DEBUG
#include "rbjit/cooperdominatorfinder.h"
#endif

RBJIT_NAMESPACE_BEGIN

LTDominatorFinder::LTDominatorFinder(ControlFlowGraph* cfg)
  : DominatorFinder(cfg),
    parent_(blocks_->size(), 0),
    ancestor_(blocks_->size(), 0),
    child_(blocks_->size(), 0),
    vertex_(blocks_->size(), 0),
    label_(blocks_->size(), 0),
    semi_(blocks_->size(), 0),
    size_(blocks_->size(), 0),
    dom_(blocks_->size(), 0),
    bucket_(blocks_->size())
{}

std::vector<BlockHeader*>&
LTDominatorFinder::dominators()
{
#ifdef RBJIT_DEBUG
  std::vector<BlockHeader*>& doms = DominatorFinder::dominators();

  CooperDominatorFinder cooper(cfg_);
  std::vector<BlockHeader*>& cooperDoms = cooper.dominators();

  bool error = false;
  for (int i = 0; i < blocks_->size(); ++i) {
    if (cfg_->exit()->index() == i) {
      continue;
    }
    if (doms[i] != cooperDoms[i]) {
      fprintf(stderr, "error block %Ix's idom is wrong\n", (*blocks_)[i]);
      error = true;
    }
  }

  assert(!error && "dominator check failed");

  return doms;
#else
  return DominatorFinder::dominators();
#endif
}

void
LTDominatorFinder::findDominators()
{
  for (int i = blocks_->size() - 1; i >= 1; --i) {
    int w = vertex_[i];
    // Step2
    BlockHeader* block = (*blocks_)[w];
    for (BlockHeader::Backedge* e = block->backedge(); e->block(); e = e->next()) {
      int v = e->block()->index();
      int u = eval(v);
      if (semi_[u] < semi_[i]) {
        semi_[i] = semi_[u];
      }
    }
    bucket_[semi_[w]].push_back(w);
    link(parent_[w],  w);

    // Step 3
    std::vector<int>& b = bucket_[parent_[w]];
    std::for_each(b.begin(), b.end(), [this, w](int v) {
      int u = eval(v);
      if (semi_[u] < semi_[v]) {
        dom_[v] = u;
      }
      else {
        dom_[v] = parent_[w];
      }
    });
    b.clear();
  }

  // Step 4
  for (int i = 1; i < blocks_->size(); ++i) {
    int w = vertex_[i];
    if (dom_[w] != vertex_[semi_[w]]) {
      dom_[w] = dom_[dom_[w]];
    }
  }

  for (int i = 0; i < blocks_->size(); ++i) {
    if (i == cfg_->entry()->index()) {
      continue;
    }
    idoms_[i] = (*blocks_)[dom_[i]];
  }
}

void
LTDominatorFinder::dfs()
{
  // Step 1
  std::vector<int> work;
  int n = 0;

  work.push_back(0);

  while (!work.empty()) {
    int v = work.back();
    work.pop_back();

    semi_[v] = n;
    vertex_[n] = v;
    label_[v] = v;
    size_[v] = 1;
    ++n;

    BlockHeader* b = (*blocks_)[v]->nextAltBlock();
    if (b && semi_[b->index()] == 0) {
      parent_[b->index()] = v;
      work.push_back(b->index());
    }

    b = (*blocks_)[v]->nextBlock();
    if (b && semi_[b->index()] == 0) {
      parent_[b->index()] = v;
      work.push_back(b->index());
    }
  }
}

void
LTDominatorFinder::compress(int v)
{
  if (ancestor_[ancestor_[v]] != 0) {
    compress(ancestor_[v]);
    if (semi_[label_[ancestor_[v]]] < semi_[label_[v]]) {
      label_[v] = label_[ancestor_[v]];
    }
    ancestor_[v] = ancestor_[ancestor_[v]];
  }
}

int
LTDominatorFinder::eval(int v)
{
  if (ancestor_[v] == 0) {
    return label_[v];
  }

  compress(v);
  if (semi_[label_[ancestor_[v]]] >= semi_[label_[v]]) {
    return label_[v];
  }
  else {
    return label_[ancestor_[v]];
  }
}

void
LTDominatorFinder::link(int v, int w)
{
  int s = w;
  while (semi_[label_[w]] < semi_[label_[child_[s]]]) {
    if (size_[s] + size_[child_[child_[s]]] >= 2 * size_[child_[s]]) {
      ancestor_[child_[s]] = s;
      child_[s] = child_[child_[s]];
    }
    else {
      size_[child_[s]] = size_[s];
      s = ancestor_[s] = child_[s];
    }
  }
  label_[s] = label_[w];
  size_[v] = size_[v] + size_[w];
  if (size_[v] < 2 * size_[w]) {
    std::swap(s, child_[v]);
  }
  while (s != 0) {
    ancestor_[s] = v;
    s = child_[s];
  }
}

RBJIT_NAMESPACE_END
