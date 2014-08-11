#pragma once
#include "rbjit/common.h"
#include "rbjit/rubytypes.h"

RBJIT_NAMESPACE_BEGIN

namespace mri {

////////////////////////////////////////////////////////////
// IdTable
//
// Helper class to treat the node nd_tbl

class IdTable {
public:

  // Call with node->nd_tbl
  IdTable(ID* tbl)
    : size_(tbl ? (size_t)*tbl++ : 0), table_(tbl)
    {}

  size_t size() const { return size_; }
  ID idAt(size_t i) const { return table_[i]; }

private:

  size_t size_;
  ID* table_;
};

} // namespace mri

RBJIT_NAMESPACE_END
