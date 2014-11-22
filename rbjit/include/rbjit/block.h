#pragma once
#include <vector>
#include <algorithm>
#include <cassert>
#ifdef RBJIT_DEBUG // used in BlockHeader
#include <string>
#endif

#include "rbjit/common.h"
#include "rbjit/opcode.h"

RBJIT_NAMESPACE_BEGIN

class Block {
public:

  Block() : index_(-1) {}

#ifdef RBJIT_DEBUG
  Block(const char* name) : index_(-1), debugName_(name) {}
#else
  Block(const char* name) : index_(-1) {}
#endif

  ////////////////////////////////////////////////////////////
  // Index

  int index() const { return index_; }
  void setIndex(int i) { index_ = i; }

  ////////////////////////////////////////////////////////////
  // Opcodes

  typedef std::vector<Opcode*>::iterator Iterator;
  typedef std::vector<Opcode*>::const_iterator ConstIterator;

  void addOpcode(Opcode* op) { opcodes_.push_back(op); }
  void insertOpcode(Iterator i, Opcode* op) { opcodes_.insert(i, op); }
  Iterator removeOpcode(Iterator i) { return opcodes_.erase(i); }
  Iterator removeOpcode(Opcode* op)
  {
    auto i = std::find(begin(), end(), op);
    assert(i != end());
    return opcodes_.erase(i);
  }

  bool containsOpcode(Opcode* op)
  { return std::find(opcodes_.begin(), opcodes_.end(), op) != opcodes_.end(); }
  Block* splitAt(Iterator i, bool deleteOpcode);

  Iterator begin() { return opcodes_.begin(); }
  Iterator end() { return opcodes_.end(); }
  ConstIterator begin() const { return opcodes_.begin(); }
  ConstIterator end() const { return opcodes_.end(); }
  ConstIterator cbegin() const { return opcodes_.cbegin(); }
  ConstIterator cend() const { return opcodes_.cend(); }

  OpcodeTerminator* footer() const { return dynamic_cast<OpcodeTerminator*>(opcodes_.back()); }

  bool visitEachOpcode(OpcodeVisitor* visitor);

  ////////////////////////////////////////////////////////////
  // Edges

  Block* nextBlock() const { return footer()->nextBlock(); }
  Block* nextAltBlock() const { return footer()->nextAltBlock(); }

  void setNextBlock(Block* block) { footer()->setNextBlock(block); }
  void setNextAltBlock(Block* block) { footer()->setNextAltBlock(block); }

  ////////////////////////////////////////////////////////////
  // Backedges

  typedef std::vector<Block*>::iterator BackedgeIterator;
  typedef std::vector<Block*>::const_iterator ConstBackedgeIterator;

  BackedgeIterator backedgeBegin() { return backedges_.begin(); }
  BackedgeIterator backedgeEnd() { return backedges_.end(); }
  ConstBackedgeIterator backedgeBegin() const { return backedges_.begin(); }
  ConstBackedgeIterator backedgeEnd() const { return backedges_.end(); }
  ConstBackedgeIterator constBackedgeBegin() const { return backedges_.cbegin(); }
  ConstBackedgeIterator constBackedgeEnd() const { return backedges_.cend(); }

  bool containsBackedge(Block* block) const
  {
    return std::find(backedgeBegin(), backedgeEnd(), block) != backedgeEnd();
  }

  void addBackedge(Block* block)
  {
    assert(std::find(backedges_.begin(), backedges_.end(), block) == backedges_.end());
    backedges_.push_back(block);
  }

  void removeBackedge(BackedgeIterator i)
  {
    backedges_.erase(i);
  }

  void removeBackedge(Block* block)
  {
    BackedgeIterator i = std::find(backedges_.begin(), backedges_.end(), block);
    assert(i != backedges_.end());
    backedges_.erase(i);
  }

  void updateBackedge(Block* oldBlock, Block* newBlock)
  {
    BackedgeIterator i = std::find(backedges_.begin(), backedges_.end(), oldBlock);
    assert(i != backedges_.end());
    *i = newBlock;
  }

  size_t backedgeCount() const { return backedges_.size(); }

  int backedgeIndexOf(Block* block)
  {
    int count = 0;
    for (auto i = backedges_.cbegin(), end = backedges_.cend(); i != end; ++i, ++count) {
      if (*i == block) {
        return count;
      }
    }
    return -1;
  }

  ////////////////////////////////////////////////////////////
  // Debugging tools

  // Block name
#ifdef RBJIT_DEBUG
  void setDebugName(const char* name)
  { debugName_ = std::string(name); }
  const char* debugName() const { return debugName_.c_str(); }
#else
  void setDebugName(const char* name) {}
  const char* debugName() const { return ""; }
#endif

  std::string debugPrint() const;

private:

  int index_;

  std::vector<Opcode*> opcodes_;
  std::vector<Block*> backedges_;

#ifdef RBJIT_DEBUG
  std::string debugName_;
#endif

};

RBJIT_NAMESPACE_END
