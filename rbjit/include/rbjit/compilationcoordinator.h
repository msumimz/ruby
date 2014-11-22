#pragma once

#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

// Coordinates the whole compilation process for each method
class JitCompiler {
public:

  JitCompiler();

  static void registerWithMethod(mri::Class cls, ID name);

  void compile();

  
