#pragma once

#include "rbjit/dominatorfinder.h"

RBJIT_NAMESPACE_BEGIN

class CooperDominatorFinder : public DominatorFinder {
public:

  CooperDominatorFinder(ControlFlowGraph* cfg);

private:

  void findDominators();
  void computeDfsOrder();
  BlockHeader* findIntersect(BlockHeader* b1, BlockHeader* b2);

  std::vector<int> dfnums_;

};

RBJIT_NAMESPACE_END

