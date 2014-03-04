#pragma once

#include <vector>
#include "rbjit/dominatorfinder.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class BlockHeader;

// Lengauer-Tarjan dominator finding algorithm
class LTDominatorFinder : public DominatorFinder {
public:

  LTDominatorFinder(ControlFlowGraph* cfg);

  std::vector<BlockHeader*>& dominators();

private:

  void findDominators();

  void dfs();
  void compress(int v);
  int eval(int v);
  void link(int v, int w);

  std::vector<int> parent_;
  std::vector<int> ancestor_;
  std::vector<int> child_;
  std::vector<int> vertex_;
  std::vector<int> label_;
  std::vector<int> semi_;
  std::vector<int> size_;
  std::vector<int> dom_;
  std::vector<std::vector<int> > bucket_;

};

RBJIT_NAMESPACE_END
