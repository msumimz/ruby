#pragma once

#include <vector>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class BlockHeader;

class DominatorFinder {
public:

  DominatorFinder(ControlFlowGraph* cfg);

  std::vector<BlockHeader*>& dominators();

protected:

  virtual void findDominators() = 0;

  ControlFlowGraph* cfg_;
  std::vector<BlockHeader*>* blocks_;

  std::vector<BlockHeader*> idoms_;

};

RBJIT_NAMESPACE_END

