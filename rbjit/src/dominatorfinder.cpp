#include "rbjit/dominatorfinder.h"
#include "rbjit/controlflowgraph.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

DominatorFinder::DominatorFinder(ControlFlowGraph* cfg)
  : cfg_(cfg), blocks_(cfg->blocks())
{}

void
DominatorFinder::setDominatorsToCfg()
{
  std::vector<BlockHeader*> idoms = dominators();

  for (int i = 0; i < blocks_->size(); ++i) {
    if (i == cfg_->entry()->index()) {
      continue;
    }
    (*blocks_)[i]->setIdom(idoms[i]);
  }
}

RBJIT_NAMESPACE_END
