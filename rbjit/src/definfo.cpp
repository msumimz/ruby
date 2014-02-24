#include "rbjit/definfo.h"

RBJIT_NAMESPACE_BEGIN

void
DefInfo::addDefSite(BlockHeader* block)
{
  ++defCount_;

  // return if already defined
  for (DefSite* ds = &defSite_; ds; ds = ds->next()) {
    if (ds->defBlock() == block) {
      return;
    }
  }

  defSite_.addDefSite(block);
}

RBJIT_NAMESPACE_END
