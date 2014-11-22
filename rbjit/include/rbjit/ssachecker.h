#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class ControlFlowGraph;
class Variable;
class Block;

class SsaChecker {
public:

  SsaChecker(const ControlFlowGraph* cfg);

  void check();

  const std::vector<std::string>& errors() const { return errors_; }

private:

  void checkBlock(Block* block);
  void checkPhiNodeOfSucceedingBlock(Block* block, Block* succ);

  const ControlFlowGraph* cfg_;
  std::vector<std::string> errors_;
  std::unordered_set<Variable*> variables_;

};

RBJIT_NAMESPACE_END
