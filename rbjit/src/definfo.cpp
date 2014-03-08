#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

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
