#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Variable;
class BlockHeader;

class SsaChecker {
public:

  SsaChecker(ControlFlowGraph* cfg);

  void check();

  const std::vector<std::string>& errors() const { return errors_; }

private:

  void checkBlock(BlockHeader* block);
  void checkPhiNodeOfSucceedingBlock(BlockHeader* block, BlockHeader* succ);

  ControlFlowGraph* cfg_;
  std::vector<std::string> errors_;
  std::unordered_set<Variable*> variables_;

};

RBJIT_NAMESPACE_END
