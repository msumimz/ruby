#pragma once

#include <vector>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Block;

class DominatorFinder {
public:

  virtual ~DominatorFinder() {}
  virtual std::vector<Block*> dominators() = 0;

};

RBJIT_NAMESPACE_END

