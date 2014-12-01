#pragma once

#include "rbjit/common.h"
#include "rbjit/rubytypes.h"
#include "rbjit/mri/decl.h"

RBJIT_NAMESPACE_BEGIN

namespace mri {

class Core {
public:

  static rb_iseq_t* findBlockISeq(const rb_iseq_t* parentISeq, const RNode* nodeIter);

  static VALUE callWithBlock();

private:

  Core();

};

} // namespace mri

RBJIT_NAMESPACE_END
