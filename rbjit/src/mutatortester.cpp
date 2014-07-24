#include "rbjit/mutatortester.h"
#include "rbjit/idstore.h"
#include "rbjit/methodinfo.h"

#include "ruby.h"

extern "C" {
#include "method.h" // vm_method_type_t
}

RBJIT_NAMESPACE_BEGIN

MutatorTester::MutatorTester()
{
  static const PredefinedId MUTATORS[] = {
    ID_eval,
    ID_instance_eval,
    ID_instance_exec,
    ID___send__,
    ID_send,
    ID_public_send,
    ID_module_exec,
    ID_class_exec,
    ID_module_eval,
    ID_class_eval,
    ID_load,
    ID_require,
    ID_gem,
    ID_NULL
  };

  for (int i = 0; MUTATORS[i] != ID_NULL; ++i) {
    mutators_.insert(IdStore::get(MUTATORS[i]));
  }
}

bool
MutatorTester::isMutator(mri::MethodEntry me)
{
  MethodInfo* mi = me.methodInfo();
  if (mi) {
    return mi->isMutator();
  }

  switch (me.methodDefinition().methodType()) {
  case VM_METHOD_TYPE_ATTRSET:
  case VM_METHOD_TYPE_IVAR:
    return false;

  case VM_METHOD_TYPE_CFUNC:
  case VM_METHOD_TYPE_OPTIMIZED:
    return  mutators_.find(me.methodName()) != mutators_.end();

  default:
    // This code path should not be reached, but defensively returns true.
    return true;
  }
}

void
MutatorTester::addMutatorAlias(ID name)
{
  mutators_.insert(name);
}

extern "C" void
rbjit_addMutatorAlias(rb_method_entry_t* me, ID newName)
{
  MutatorTester* mt = MutatorTester::instance();
  if (mt->isMutator(mri::MethodEntry(me))) {
    MutatorTester::instance()->addMutatorAlias(newName);
  }
}

RBJIT_NAMESPACE_END
