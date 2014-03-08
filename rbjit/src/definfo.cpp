#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

DefInfo::~DefInfo()
{
  if (defSite_.next()) {
    defSite_.next()->clearDefSite();
  }
}

void
DefSite::clearDefSite()
{
  if (next_) {
    next_->clearDefSite();
  }
  delete this;
}

void
DefInfo::addDefSite(BlockHeader* block)
{
  // return if already defined
  for (DefSite* ds = &defSite_; ds; ds = ds->next()) {
    if (ds->defBlock() == block) {
      return;
    }
  }

  defSite_.addDefSite(block);
  ++defCount_;
}

RBJIT_NAMESPACE_END
