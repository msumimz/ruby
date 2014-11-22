#pragma once

#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class CompilationCoodinator {
public:

  typedef *(VALUE CompiledMethod(rb_control_frame_t* cfp, int argc, VALUE contextClass, rb_method_entry_t* me)

  static void registerWithMethod(mri::Class cls, ID name);
  static void registerWithMethod(mri::MethodEntry me);

  CompilationCoordinator(mri::MethodEntry me);


