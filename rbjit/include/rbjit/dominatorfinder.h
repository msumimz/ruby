#pragma once

#include <vector>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class BlockHeader;

class DominatorFinder {
public:

  DominatorFinder(ControlFlowGraph* cfg);

  virtual std::vector<BlockHeader*> dominators() = 0;
  virtual void setDominatorsToCfg();

protected:

  void findDominators();

  ControlFlowGraph* cfg_;
  std::vector<BlockHeader*>* blocks_;

};

RBJIT_NAMESPACE_END

