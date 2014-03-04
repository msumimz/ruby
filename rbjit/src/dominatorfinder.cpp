#include "rbjit/dominatorfinder.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"

#ifdef RBJIT_DEBUG
#include "rbjit/cooperdominatorfinder.h"
#endif

RBJIT_NAMESPACE_BEGIN

DominatorFinder::DominatorFinder(ControlFlowGraph* cfg)
  : cfg_(cfg), blocks_(cfg->blocks()),
    idoms_(blocks_->size())
{}

std::vector<BlockHeader*>&
DominatorFinder::dominators()
{
  findDominators();

  return idoms_;
}

RBJIT_NAMESPACE_END
