#include "rbjit/rubycore.h"

extern "C" {
#include "vm_core.h"
}

RBJIT_NAMESPACE_BEGIN

namespace mri {

rb_iseq_t*
Core::findBlockISeq(const rb_iseq_t* parentISeq, const RNode* nodeIter)
{
  for (int i = 0; i < parentISeq->callinfo_size; ++i) {
    rb_call_info_t* ci = parentISeq->callinfo_entries + i;
    if (ci->blockiseq->node == nodeIter) {
      return ci->blockiseq;
    }
  }
  return nullptr;
}

}

RBJIT_NAMESPACE_END