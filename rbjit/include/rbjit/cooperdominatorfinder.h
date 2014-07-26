#pragma once

#include "rbjit/dominatorfinder.h"

RBJIT_NAMESPACE_BEGIN

class CooperDominatorFinder : public DominatorFinder {
public:

  CooperDominatorFinder(ControlFlowGraph* cfg);

  std::vector<BlockHeader*> dominators();

private:

  void findDominators();
  void computeDfsOrder();
  int computeDfsOrderInternal(BlockHeader* b, int count);
  BlockHeader* findIntersect(BlockHeader* b1, BlockHeader* b2);

  std::vector<BlockHeader*> idoms_;
  std::vector<int> dfnums_;
  std::vector<BlockHeader*> postorders_;

};

RBJIT_NAMESPACE_END

