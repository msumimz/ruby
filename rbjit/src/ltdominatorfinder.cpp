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
    parent_(blocks_->size() + 1, 0),
    ancestor_(blocks_->size() + 1, 0),
    child_(blocks_->size() + 1, 0),
    vertex_(blocks_->size() + 1, 0),
    label_(blocks_->size() + 1, 0),
    semi_(blocks_->size() + 1, 0),
    size_(blocks_->size() + 1, 0),
    dom_(blocks_->size() + 1, 0),
    pred_(blocks_->size() + 1),
    bucket_(blocks_->size() + 1),
    computed_(false)
{}

std::vector<BlockHeader*>
LTDominatorFinder::dominators()
{
  findDominators();

  std::vector<BlockHeader*> idoms(blocks_->size());
  for (int i = 0; i < blocks_->size(); ++i) {
    if (i == cfg_->entry()->index()) {
      continue;
    }
    idoms[i] = (*blocks_)[dom_[i + 1] - 1];
  }

#ifdef RBJIT_DEBUG
  debugVerify(idoms);
#endif

  return idoms;
}

void
LTDominatorFinder::setDominatorsToCfg()
{
  findDominators();

#ifdef RBJIT_DEBUG
  debugVerify(dominators());
#endif

  for (int i = 0; i < blocks_->size(); ++i) {
    if (i == cfg_->entry()->index()) {
      continue;
    }
    (*blocks_)[i]->setIdom((*blocks_)[dom_[i + 1] - 1]);
  }
}

#ifdef RBJIT_DEBUG

void
LTDominatorFinder::debugVerify(std::vector<BlockHeader*>& doms)
{
  CooperDominatorFinder cooper(cfg_);
  std::vector<BlockHeader*> cooperDoms = cooper.dominators();

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

  for (int i = blocks_->size(); i >= 2; --i) {
    int w = vertex_[i];
    // Step 2
    // Compute the semidominators of all vertices. Carry out the computation
    // vertex by vertex in decreasing order by number.
    std::for_each(pred_[w].cbegin(), pred_[w].cend(), [this, i, w](int v) {
      int u = eval(v);
      if (semi_[u] < semi_[w]) {
        semi_[w] = semi_[u];
      }
    });
    bucket_[vertex_[semi_[w]]].push_back(w);
    link(parent_[w],  w);

    // Step 3
    // Implicitly define the immediate dominator of each vertex.
    std::vector<int>& b = bucket_[parent_[w]];
    std::for_each(b.cbegin(), b.cend(), [this, w](int v) {
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
  // Explicitly define the immediate dominator of each vertex, carrying out the
  // computation vertex by vertex in increasing order by number.
  for (int i = 2; i <= blocks_->size(); ++i) {
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

  work.push_back(cfg_->entry()->index() + 1);

  while (!work.empty()) {
    int v = work.back();
    work.pop_back();

    semi_[v] = ++n;
    vertex_[n] = label_[v] = v;
    size_[v] = 1;

    BlockHeader* b = (*blocks_)[v - 1]->nextAltBlock();
    if (b) {
      int w = b->index() + 1;
      if (semi_[w] == 0) {
        parent_[w] = v;
        work.push_back(w);
      }
      pred_[w].push_back(v);
    }

    b = (*blocks_)[v - 1]->nextBlock();
    if (b) {
      int w = b->index() + 1;
      if (semi_[w] == 0) {
        parent_[w] = v;
        work.push_back(w);
      }
      pred_[w].push_back(v);
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
