#include <cassert>
#include <algorithm>
#include "rbjit/ltdominatorfinder.h"
#include "rbjit/opcode.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/block.h"

#ifdef RBJIT_DEBUG
#include "rbjit/cooperdominatorfinder.h"
#endif

RBJIT_NAMESPACE_BEGIN

LTDominatorFinder::LTDominatorFinder(const ControlFlowGraph* cfg)
  : cfg_(cfg),
    parent_(cfg_->blockCount() + 1, 0),
    ancestor_(cfg_->blockCount() + 1, 0),
    child_(cfg_->blockCount() + 1, 0),
    vertex_(cfg_->blockCount() + 1, 0),
    label_(cfg_->blockCount() + 1, 0),
    semi_(cfg_->blockCount() + 1, 0),
    size_(cfg_->blockCount() + 1, 0),
    dom_(cfg_->blockCount() + 1, 0),
    pred_(cfg_->blockCount() + 1),
    bucket_(cfg_->blockCount() + 1),
    computed_(false)
{}

std::vector<Block*>
LTDominatorFinder::dominators()
{
  findDominators();

  std::vector<Block*> idoms(cfg_->blockCount());
  for (int i = 0; i < cfg_->blockCount(); ++i) {
    if (dom_[i + 1] == 0) {
      continue;
    }
    idoms[i] = cfg_->block(dom_[i + 1] - 1);
  }

#ifdef RBJIT_DEBUG
  debugVerify(idoms);
#endif

  return idoms;
}

#ifdef RBJIT_DEBUG

void
LTDominatorFinder::debugVerify(std::vector<Block*>& doms)
{
  CooperDominatorFinder cooper(cfg_);
  std::vector<Block*> cooperDoms = cooper.dominators();

  bool error = false;
  for (int i = 0; i < cfg_->blockCount(); ++i) {
    // LTDominatorFinder does not find an exit block's dominator
    if (cfg_->block(i) == cfg_->exitBlock()) {
      continue;
    }
    if (doms[i] != cooperDoms[i]) {
      fprintf(stderr, "error block %Ix's idom is wrong: cooper %Ix, lt %Ix\n", cfg_->block(i), cooperDoms[i], doms[i]);
      error = true;
    }
  }

  if (error) {
    for (int i = 0; i < cfg_->blockCount(); ++i) {
      fprintf(stderr, "%2d: %Ix %Ix %Ix\n", i, cfg_->block(i), cooperDoms[i], doms[i]);
    }
  }

  assert(!error && "dominator check failed");
}

#endif

void
LTDominatorFinder::findDominators()
{
  if (computed_) {
    return;
  }
  computed_ = true;

  // Step 1
  // Carry out a depth-first search of the problem graph. Number the vertices
  // from 1 to n as they are reached during the search. Initialize the
  // variables used in succeeding steps.
  dfs();

  for (int i = cfg_->blockCount(); i >= 2; --i) {
    int w = vertex_[i];
    // Step 2
    // Compute the semidominators of all vertices. Carry out the computation
    // vertex by vertex in decreasing order by number.
    for (auto p = pred_[w].cbegin(), pend = pred_[w].cend(); p != pend; ++p) {
      int u = eval(*p);
      if (semi_[u] < semi_[w]) {
        semi_[w] = semi_[u];
      }
    };
    bucket_[vertex_[semi_[w]]].push_back(w);
    link(parent_[w],  w);

    // Step 3
    // Implicitly define the immediate dominator of each vertex.
    std::vector<int>& b = bucket_[parent_[w]];
    for (auto p = b.cbegin(), pend = b.cend(); p != pend; ++p) {
      int v = *p;
      int u = eval(v);
      if (semi_[u] < semi_[v]) {
        dom_[v] = u;
      }
      else {
        dom_[v] = parent_[w];
      }
    };
    b.clear();
  }

  // Step 4
  // Explicitly define the immediate dominator of each vertex, carrying out the
  // computation vertex by vertex in increasing order by number.
  for (int i = 2; i <= cfg_->blockCount(); ++i) {
    int w = vertex_[i];
    if (dom_[w] != vertex_[semi_[w]]) {
      dom_[w] = dom_[dom_[w]];
    }
  }

}

void
LTDominatorFinder::dfs()
{
  std::vector<int> work;
  int n = 0;

  int v = cfg_->entryBlock()->index() + 1;
  Block* b;
  for (;;) {
    for (;;) {
      semi_[v] = ++n;
      vertex_[n] = label_[v] = v;
      ancestor_[v] = child_[v] = 0;
      size_[v] = 1;

      work.push_back(v);

      b = cfg_->block(v - 1)->nextBlock();
      if (!b) {
        break;
      }

      int w = b->index() + 1;
      pred_[w].push_back(v);
      if (semi_[w] == 0) {
        parent_[w] = v;
      }
      else {
        break;
      }

      v = b->index() + 1;
    }

  altBlockLoop:
    b = 0;
    while (!work.empty()) {
      v = work.back();
      work.pop_back();
      b = cfg_->block(v - 1)->nextAltBlock();
      if (b) {
        break;
      }
    }
    if (!b) {
      return;
    }

    int w = b->index() + 1;
    pred_[w].push_back(v);
    if (semi_[w] == 0) {
      parent_[w] = v;
    }
    else {
      goto altBlockLoop;
    }
    v = w;
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

#if 0 // simple O(n log n) version

int
LTDominatorFinder::eval(int v)
{
  if (ancestor_[v] == 0) {
    return v;
  }
  compress(v);
  return label_[v];
}

void
LTDominatorFinder::link(int v, int w)
{
  ancestor_[w] = v;
}

#endif

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
