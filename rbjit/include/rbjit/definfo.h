#pragma once
#include <unordered_map>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class Block;
class Variable;
class Opcode;

class DefSite {
public:

  DefSite(Block* defBlock, DefSite* next)
    : defBlock_(defBlock), next_(next)
  {}

  Block* defBlock() const { return defBlock_; }
  DefSite* next() const { return next_; }

  void addDefSite(Block* defBlock)
  {
    if (defBlock_ == 0) {
      defBlock_ = defBlock;
    }
    else {
      next_ = new DefSite(defBlock, next_);
    }
  }

  void clearDefSite();

private:


  Block* defBlock_;
  DefSite* next_;

};

class DefInfo {
public:

  DefInfo() : defSite_(0, 0), defCount_(0), local_(true) {}

  DefInfo(Block* defBlock)
    : defSite_(defBlock, 0), defCount_(1), local_(true) {}

  ~DefInfo();

  const DefSite* defSite() const { return &defSite_; }
  void addDefSite(Block* block);

  int defCount() const { return defCount_; }
  void increaseDefCount() { ++defCount_; }
  void decreaseDefCount() { --defCount_; }

  bool isLocal() const { return local_; }
  void setLocal(bool local) { local_ = local; }

private:

  // The block where the variable is defined.
  // Used to judge whether the variable is local.
  DefSite defSite_;

  // The number of definition sites.
  // Used to judge whether renaming is necessary.
  int defCount_;

  // True if every def and use is located in the same block
  bool local_;
};

class DefInfoMap {
public:

  ~DefInfoMap();

  void updateDefSite(Variable* v, Block* defBlock, Opcode* defOpcode);

  DefInfo* find(Variable* v)
  {
    auto i = map_.find(v);
    return i != map_.end() ? i->second : nullptr;
  }

private:

  // TODO: Use an array
  std::unordered_map<Variable*, DefInfo*> map_;
};

RBJIT_NAMESPACE_END
