#include "rbjit/definfo.h"
#include "rbjit/variable.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// DefSite

void
DefSite::clearDefSite()
{
  if (next_) {
    next_->clearDefSite();
  }
  delete this;
}

////////////////////////////////////////////////////////////
// DefInfo

DefInfo::~DefInfo()
{
  if (defSite_.next()) {
    defSite_.next()->clearDefSite();
  }
}

void
DefInfo::addDefSite(Block* block)
{
  // defCount should be increased whichever the block is already added
  ++defCount_;

  // return if the block is already defined
  for (DefSite* ds = &defSite_; ds; ds = ds->next()) {
    if (ds->defBlock() == block) {
      return;
    }
  }

  defSite_.addDefSite(block);
  if (defCount_ > 1) {
    local_ = false;
  }
}

////////////////////////////////////////////////////////////
// DefInfoMap

DefInfoMap::~DefInfoMap()
{
  for (auto i = map_.begin(), end = map_.end(); i != end; ++i) {
    delete i->second;
  }
}

void
DefInfoMap::updateDefSite(Variable* v, Block* defBlock, Opcode* defOpcode)
{
  v->setDefBlock(defBlock);
  v->setDefOpcode(defOpcode);

  auto i = map_.find(v);
  if (i != map_.end()) {
    i->second->addDefSite(defBlock);
  }
  else {
    DefInfo* defInfo = new DefInfo(defBlock);
    map_.insert(std::make_pair(v, defInfo));
  }
}

std::string
DefInfoMap::debugPrint() const
{
  std::string out = stringFormat("[DefInfoMap: %Ix]\n", this);

  for (auto i = map_.cbegin(), end = map_.cend(); i != end; ++i) {
    Variable* v = i->first;
    DefInfo* defInfo = i->second;
    out += stringFormat("%Ix: defInfo=%Ix defCount=%d local=%s defSite=",
      v, defInfo, defInfo->defCount(), defInfo->isLocal() ? "true" : "false");
    for (const DefSite* s = defInfo->defSite(); s; s = s->next()) {
      out += stringFormat("%Ix ", s);
    }
    out += '\n';
  }

  return out;
}

RBJIT_NAMESPACE_END
