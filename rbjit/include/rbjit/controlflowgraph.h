#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class Opcode;
class OpcodeCall;
class BlockHeader;
class Variable;
class BlockVisitor;
class DomTree;

// Single-entry and single-exit control flow graph
class ControlFlowGraph {
public:

  ControlFlowGraph();

  ControlFlowGraph* copy() const;

  // Accessors

  BlockHeader* entry() const { return entry_; }
  BlockHeader* exit() const { return exit_; }

  Variable* output() const {  return output_; }
  Variable* undefined() const { return undefined_; }
  Variable* env() const { return env_; }
  void setEnv(Variable* env) { env_ = env; }

  DomTree* domTree();

  // Blocks

  const std::vector<BlockHeader*>* blocks() const { return &blocks_; }
  std::vector<BlockHeader*>* blocks() { return &blocks_; }

  bool containsBlock(const BlockHeader* block) const
  { return std::find(blocks_.cbegin(), blocks_.cend(), block) != blocks_.cend(); }

  // Variables

  const std::vector<Variable*>* variables() const { return &variables_; }
  std::vector<Variable*>* variables() { return &variables_; }
  Variable* copyVariable(BlockHeader* defBlock, Opcode* defOpcode, Variable* source);
  void removeVariables(const std::vector<Variable*>* toBeRemoved);
  void clearDefInfo();

  bool containsVariable(const Variable* v) const
  { return std::find(variables_.cbegin(), variables_.cend(), v) != variables_.cend(); }

  bool containsInInputs(const Variable* v) const
  { return std::find(inputs_.cbegin(), inputs_.cend(), v) != inputs_.cend(); }

  // Method arguments

  const std::vector<Variable*>* inputs() const { return &inputs_; }
  std::vector<Variable*>* inputs() { return &inputs_; }

  int requiredArgCount() const { return requiredArgCount_; }
  void setRequiredArgCount(int requiredArgCount) { requiredArgCount_ = requiredArgCount; }

  bool hasOptionalArg() const { return hasOptionalArg_; }
  void setHasOptionalArg(bool hasOptionalArg) { hasOptionalArg_ = hasOptionalArg; }

  bool hasRestArg() const { return hasRestArg_; }
  void setHasRestArg(bool hasRestArg) { hasRestArg_ = hasRestArg; }

  // Modifiers

  void removeOpcodeAfter(Opcode* prev);
  void removeOpcode(Opcode* op);
  BlockHeader* splitBlock(BlockHeader* block, Opcode* op);
  BlockHeader* insertEmptyBlockAfter(BlockHeader* block);

  // Sanity check for debugging

  bool checkSanity() const;
  bool checkSanityAndPrintErrors() const;

  // debug print

  std::string debugPrint() const;
  std::string debugPrintBlock(BlockHeader* block) const;
  std::string debugPrintVariables() const;
  std::string debugPrintTypeConstraints() const;

private:

  friend class OpcodeFactory;

  BlockHeader* entry_;
  BlockHeader* exit_;

  // Basic blocks
  std::vector<BlockHeader*> blocks_;

  // Loop preheaders
  std::vector<BlockHeader*> loops_;

  // Variables used in the CFG, including inputs_ and output_
  std::vector<Variable*> variables_;

  // Method arguments
  std::vector<Variable*> inputs_;
  int requiredArgCount_;
  bool hasOptionalArg_;
  bool hasRestArg_;

  // Return value
  Variable* output_;

  // Undefined value that is used in the process of SSA translating
  Variable* undefined_;

  // Environment
  // When two environments are equal, two method lookups with the same receiver
  // class and method name result in the same method entry.
  Variable* env_;

  // Dominance tree
  DomTree* domTree_;
};

// Block visitor interface
class BlockVisitor {
  virtual void visitBlock(BlockHeader* block) = 0;
};

RBJIT_NAMESPACE_END
