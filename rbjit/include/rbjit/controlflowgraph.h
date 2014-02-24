#pragma once
#include <vector>
#include <string>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class Opcode;
class BlockHeader;
class Variable;
class BlockVisitor;

// Single-entry and single-exit control flow graph
class ControlFlowGraph {
public:

  enum {
    UNKNOWN = 0,
    YES = 1,
    NO = 2
  };

  ControlFlowGraph()
    : hasEvals_(UNKNOWN), hasDefs_(UNKNOWN), hasBindings_(UNKNOWN),
      opcodeCount_(0), entry_(0), exit_(0), output_(0)
  {}

  BlockHeader* entry() const { return entry_; }
  BlockHeader* exit() const { return exit_; }
  int opcodeCount() const { return opcodeCount_; }

  Variable* output() const {  return output_; }

  const std::vector<BlockHeader*>* blocks() const { return &blocks_; }
  std::vector<BlockHeader*>* blocks() { return &blocks_; }

  const std::vector<Variable*>* variables() const { return &variables_; }
  std::vector<Variable*>* variables() { return &variables_; }

  void setOpcodeCount(int count) { opcodeCount_ = count; }

  void inlineAnotherCFG(Opcode* where, ControlFlowGraph* cfg);

  ControlFlowGraph* copy() const;

  std::string debugDump() const;

private:

  friend class OpcodeFactory;

  // properties
  int hasEvals_ : 2;
  int hasDefs_ : 2;
  int hasBindings_ : 2;

  BlockHeader* entry_;
  BlockHeader* exit_;

  int opcodeCount_;

  // Basic blocks
  std::vector<BlockHeader*> blocks_;

  // Loop headers
  std::vector<BlockHeader*> loops_;

  // Variables used in the CFG, including input_ and output_
  std::vector<Variable*> variables_;

  // Variables that should be initialized with call arguments.
  std::vector<Variable*> input_;

  // Return value
  Variable* output_;
};

// Block visitor interface
class BlockVisitor {
  virtual void visitBlock(BlockHeader* block) = 0;
};

RBJIT_NAMESPACE_END
