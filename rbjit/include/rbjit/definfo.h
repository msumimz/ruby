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

private:

  BlockHeader* defBlock_;
  DefSite* next_;

};

class DefInfo {
public:

  DefInfo() : defSite_(0, 0), defCount_(0) {}

  DefInfo(BlockHeader* defBlock)
    : defSite_(defBlock, 0), defCount_(1) {}

  const DefSite* defSite() const { return &defSite_; }
  int defCount() const { return defCount_; }

  void addDefSite(BlockHeader* block);

private:

  // The block where the variable is defined
  // Used to judge whether the variable is local
  DefSite defSite_;

  // The number of definition sites to be used to judge whether renaming is necessary
  // Each definition in the same block should be counted
  int defCount_;
};

RBJIT_NAMESPACE_END
