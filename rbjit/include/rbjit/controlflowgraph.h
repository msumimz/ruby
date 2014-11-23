#pragma once

#include <cassert>
#include <algorithm> // std::find
#ifdef RBJIT_DEBUG // used in BlockHeader
#include <string>
#endif
#include "rbjit/common.h"
#include "rbjit/rubymethod.h"
#include "rbjit/block.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

class Variable;
class Block;
class Scope;
class Opcode;

////////////////////////////////////////////////////////////
// ControlFlowGraph

class ControlFlowGraph {
public:

  ControlFlowGraph()
    : output_(nullptr), entryBlock_(nullptr), exitBlock_(nullptr),
      entryEnv_(nullptr), exitEnv_(nullptr), undefined_(nullptr)
  {}

  ~ControlFlowGraph();

  ////////////////////////////////////////////////////////////
  // Basic blocks

  void
  addBlock(Block* b)
  {
    assert(b);
    assert(std::find(blocks_.begin(), blocks_.end(), b) == blocks_.end());
    b->setIndex(blocks_.size());
    blocks_.push_back(b);
  }

  void removeBlock(Block* b)
  {
    auto i = std::find(blocks_.begin(), blocks_.end(), b);
    assert(i != blocks_.end());
    i = blocks_.erase(i);
    for (auto end = blocks_.end(); i != end; ++i) {
      b->setIndex(b->index() - 1);
    }
  }

  bool containsBlock(Block* b) const
  {
    return std::find(blocks_.begin(), blocks_.end(), b) != blocks_.end();
  }

  Block* splitBlock(Block* b, Block::Iterator i, bool deleteOpcode)
  {
    Block* newBlock = b->splitAt(i, deleteOpcode);
    addBlock(newBlock);
    return newBlock;
  }

  size_t blockCount() const { return blocks_.size(); }
  Block* block(size_t i) const { return blocks_[i]; }

  typedef std::vector<Block*>::iterator Iterator;
  typedef std::vector<Block*>::const_iterator ConstIterator;

  Iterator begin() { return blocks_.begin(); }
  Iterator end() { return blocks_.end(); }
  ConstIterator begin() const { return blocks_.begin(); }
  ConstIterator end() const { return blocks_.end(); }
  ConstIterator cbegin() const { return blocks_.cbegin(); }
  ConstIterator cend() const { return blocks_.cend(); }

  ////////////////////////////////////////////////////////////
  // Variables

  void addVariable(Variable* v)
  {
    assert(v);
    assert(std::find(variables_.begin(), variables_.end(), v) == variables_.end());
    v->setIndex(variables_.size());
    variables_.push_back(v);
  }

  void removeVariable(Variable* v)
  {
    auto i = std::find(variables_.begin(), variables_.end(), v);
    assert(i != variables_.end());
    i = variables_.erase(i);
    for (auto end = variables_.end(); i != end; ++i) {
      v->setIndex(v->index() - 1);
    }
  }

  void removeVariables(const std::vector<Variable*>& toBeRemoved) {
    // Delete and zero-clear the elements that should be removed
    for (auto i = toBeRemoved.cbegin(), end = toBeRemoved.cend(); i != end; ++i) {
      variables_[(*i)->index()] = nullptr;
      delete (*i);
    };

    // Remove zero-cleared elements
    variables_.erase(std::remove_if(variables_.begin(), variables_.end(),
          [](Variable* v) { return !v; }), variables_.end());

    // Reset indexes
    for (int i = 0; i < variables_.size(); ++i) {
      variables_[i]->setIndex(i);
    }
  }

  bool containsVariable(const Variable* v) const
  { return std::find(variables_.begin(), variables_.end(), v) != variables_.end(); }

  size_t variableCount() const { return variables_.size(); }
  Variable* variable(size_t i) { return variables_[i]; }

  typedef std::vector<Variable*>::iterator VariableIterator;
  typedef std::vector<Variable*>::const_iterator ConstVariableIterator;

  VariableIterator variableBegin() { return variables_.begin(); }
  VariableIterator variableEnd() { return variables_.end(); }
  ConstVariableIterator variableBegin() const { return variables_.begin(); }
  ConstVariableIterator variableEnd() const { return variables_.end(); }
  ConstVariableIterator constVariableBegin() const { return variables_.cbegin(); }
  ConstVariableIterator constVariableEnd() const { return variables_.cend(); }

  ////////////////////////////////////////////////////////////
  // Arguments

  typedef std::vector<Variable*>::iterator InputIterator;
  typedef std::vector<Variable*>::const_iterator ConstInputIterator;

  InputIterator inputBegin() { return inputs_.begin(); }
  InputIterator inputEnd() { return inputs_.end(); }
  ConstInputIterator inputBegin() const { return inputs_.begin(); }
  ConstInputIterator inputEnd() const { return inputs_.end(); }
  ConstInputIterator constInputBegin() const { return inputs_.cbegin(); }
  ConstInputIterator constInputEnd() const { return inputs_.cend(); }

  void addInput(Variable* v) { inputs_.push_back(v); }
  void removeInput(InputIterator i) { inputs_.erase(i); }
  bool containsInput(Variable* v) const
  { return std::find(inputs_.begin(), inputs_.end(), v) != inputs_.end(); }

  size_t inputCount() { return inputs_.size(); }
  Variable* input(size_t i) { return inputs_[i]; }

  ////////////////////////////////////////////////////////////
  // Return value

  Variable* output() const {  return output_; }
  void setOutput(Variable* output) {  output_ = output; }

  ////////////////////////////////////////////////////////////
  // Entry blocks

  Block* entryBlock() const
  { assert(entryBlock_); return entryBlock_; }

  void setEntryBlock(Block* block)
  {
    assert(std::find(blocks_.begin(), blocks_.end(), block) != blocks_.end());
    entryBlock_ = block;
  }

  ////////////////////////////////////////////////////////////
  // Exit blocks

  Block* exitBlock() const
  { assert(exitBlock_); return exitBlock_; }

  void setExitBlock(Block* block)
  {
    assert(std::find(blocks_.begin(), blocks_.end(), block) != blocks_.end());
    exitBlock_ = block;
  }

  ////////////////////////////////////////////////////////////
  // Entry and exit environments

  Variable* entryEnv() const
  { assert(entryEnv_); return entryEnv_; }

  void setEntryEnv(Variable* env)
  {
    assert(std::find(variables_.cbegin(), variables_.cend(), env) != variables_.cend());
    entryEnv_ = env;
  }

  Variable* exitEnv() const
  { assert(exitEnv_); return exitEnv_; }

  void setExitEnv(Variable* env)
  {
    assert(std::find(variables_.cbegin(), variables_.cend(), env) != variables_.cend());
    exitEnv_ = env;
  }

  ////////////////////////////////////////////////////////////
  // Undefined value

  Variable* undefined() const
  { assert(undefined_); return undefined_; }

  void setUndefined(Variable* undefined)
  {
    assert(std::find(variables_.cbegin(), variables_.cend(), undefined) != variables_.cend());
    undefined_ = undefined;
  }

  ////////////////////////////////////////////////////////////
  // Debugging tools

  std::string debugPrint() const;
  std::string debugPrintDotHeader() const;
  std::string debugPrintAsDot() const;
  std::string debugPrintBlock(Block* block) const;
  std::string debugPrintVariables() const;

  bool checkSanity() const;
  bool checkSanityAndPrintErrors() const;
  bool checkSsaAndPrintErrors() const;

private:

  friend class CodeDuplicator;

  std::vector<Block*> blocks_;
  std::vector<Variable*> variables_;

  std::vector<Variable*> inputs_;
  Variable* output_;

  Block* entryBlock_;
  Block* exitBlock_;

  Variable* entryEnv_;
  Variable* exitEnv_;

  Variable* undefined_;
};

RBJIT_NAMESPACE_END
