#pragma once

#include "rbjit/dominatorfinder.h"

RBJIT_NAMESPACE_BEGIN

class CooperDominatorFinder : public DominatorFinder {
public:

  CooperDominatorFinder(const ControlFlowGraph* cfg);

  std::vector<Block*> dominators();

private:

  void findDominators();
  void computeDfsOrder();
  int computeDfsOrderInternal(Block* b, int count);
  Block* findIntersect(Block* b1, Block* b2);

  const ControlFlowGraph* cfg_;
  std::vector<Block*> idoms_;
  std::vector<int> dfnums_;
  std::vector<Block*> postorders_;

};

RBJIT_NAMESPACE_END

