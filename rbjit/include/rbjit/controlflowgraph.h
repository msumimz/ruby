#pragma once
#include <vector>
#include <string>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class Opcode;
class BlockHeader;
class Variable;
class BlockVisitor;
class DomTree;

// Single-entry and single-exit control flow graph
class ControlFlowGraph {
public:

  enum { UNKNOWN = 0, YES = 1, NO = 2 };

  ControlFlowGraph()
    : hasEvals_(UNKNOWN), hasDefs_(UNKNOWN), hasBindings_(UNKNOWN),
      opcodeCount_(0), entry_(0), exit_(0),
      output_(0), undefined_(0),
      domTree_(0)
  {}

  ControlFlowGraph* copy() const;

  // Accessors

  int opcodeCount() const { return opcodeCount_; }
  void setOpcodeCount(int count) { opcodeCount_ = count; }

  BlockHeader* entry() const { return entry_; }
  BlockHeader* exit() const { return exit_; }

  Variable* output() const {  return output_; }
  Variable* undefined() const { return undefined_; }

  DomTree* domTree();

  // Blocks

  const std::vector<BlockHeader*>* blocks() const { return &blocks_; }
  std::vector<BlockHeader*>* blocks() { return &blocks_; }

  // Variables

  const std::vector<Variable*>* variables() const { return &variables_; }
  std::vector<Variable*>* variables() { return &variables_; }
  Variable* copyVariable(BlockHeader* defBlock, Opcode* defOpcode, Variable* source);
  void removeVariables(const std::vector<Variable*>* toBeRemoved);

  // Modifiers

  void removeOpcodeAfter(Opcode* prev);
  void inlineAnotherCFG(Opcode* where, ControlFlowGraph* cfg);

  std::string debugPrint() const;
  std::string debugPrintVariables() const;

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

  // Loop preheaders
  std::vector<BlockHeader*> loops_;

  // Variables used in the CFG, including input_ and output_
  std::vector<Variable*> variables_;

  // Variables that should be initialized with call arguments.
  std::vector<Variable*> input_;

  // Return value
  Variable* output_;

  // Undefined value that is used in the process of SSA translating
  Variable* undefined_;

  DomTree* domTree_;
};

// Block visitor interface
class BlockVisitor {
  virtual void visitBlock(BlockHeader* block) = 0;
};

RBJIT_NAMESPACE_END
