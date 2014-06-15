#pragma once
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class BlockHeader;

class DefSite {
public:

  DefSite(BlockHeader* defBlock, DefSite* next)
    : defBlock_(defBlock), next_(next)
  {}

  BlockHeader* defBlock() const { return defBlock_; }
  DefSite* next() const { return next_; }

  void addDefSite(BlockHeader* defBlock)
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


  BlockHeader* defBlock_;
  DefSite* next_;

};

class DefInfo {
public:

  DefInfo() : defSite_(0, 0), defCount_(0), local_(true) {}

  DefInfo(BlockHeader* defBlock)
    : defSite_(defBlock, 0), defCount_(1), local_(true) {}

  ~DefInfo();

  const DefSite* defSite() const { return &defSite_; }
  void addDefSite(BlockHeader* block);

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

RBJIT_NAMESPACE_END
