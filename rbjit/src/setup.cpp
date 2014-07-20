#include <crtdbg.h>
#include <intrin.h> // suppress warning in ruby_atomic.h
#include <string>

#include "rbjit/methodinfo.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/primitivestore.h"
#include "rbjit/idstore.h"
#include "rbjit/rubymethod.h"
#include "rbjit/typeconstraint.h"
#include "rbjit/controlflowgraph.h"
#include  "rbjit/cfgbuilder.h" // UnsupportedSyntaxException

#include "ruby.h"
#include "node.h" // rb_parser_dump_tree

using namespace rbjit;

////////////////////////////////////////////////////////////
// Development tools

static VALUE
precompile(VALUE self, VALUE cls, VALUE methodName)
{
  PrecompiledMethodInfo* mi = PrecompiledMethodInfo::construct(mri::Class(cls), mri::Symbol(methodName).id());
  if (!mi) {
    rb_raise(rb_eArgError, "method does not have the source code to be compiled");
  }

  // Preserve the existing method definition by aliasing
  const char* oldName = mri::Symbol(methodName).name();
  std::string newName(oldName);
  newName += "_orig";
  rb_define_alias(cls, newName.c_str(), oldName);

  try {
    mi->compile();
  }
  catch (UnsupportedSyntaxException& e) {
    mi->release();
    rb_raise(rb_eArgError, e.what());
  }

  return Qnil;
}

extern "C" void
Init_rbjitSetup()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
#endif

  try {
    IdStore::setup();
    PrimitiveStore::setup();
    NativeCompiler::setup();
  }
  catch (std::exception& e) {
    // This is a build/setup error, and the message should be shown in the GUI environment (rubyw).
    ::MessageBoxA(nullptr, e.what(), "rbjit initialization error", MB_OK);
    exit(1);
  }
}

extern "C" void
Init_rbjitMethodDefinitions()
{
  VALUE c = rb_define_module("Jit");
  rb_define_module_function(c, "precompile", (VALUE (*)(...))precompile, 2);

  CMethodInfo::construct(
    mri::Class::fixnumClass(), "+", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::fixnumClass()),
                          TypeExactClass::create(mri::Class::bignumClass())));
  CMethodInfo::construct(
    mri::Class::fixnumClass(), "<=", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::trueClass()),
                          TypeExactClass::create(mri::Class::falseClass())));
  CMethodInfo::construct(
    mri::Class::fixnumClass(), "<", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::trueClass()),
                          TypeExactClass::create(mri::Class::falseClass())));
  CMethodInfo::construct(
    mri::Class::bignumClass(), "+", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::fixnumClass()),
                          TypeExactClass::create(mri::Class::bignumClass())));
  CMethodInfo::construct(
    mri::Class::bignumClass(), "<=", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::trueClass()),
                          TypeExactClass::create(mri::Class::falseClass())));
  CMethodInfo::construct(
    mri::Class::bignumClass(), "<", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::trueClass()),
                          TypeExactClass::create(mri::Class::falseClass())));
}

