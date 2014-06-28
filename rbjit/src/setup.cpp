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

extern "C" {
#include "ruby.h"
#include "node.h" // rb_parser_dump_tree
}

extern "C" {
void Init_rbjit();
}

using namespace rbjit;

////////////////////////////////////////////////////////////
// Development tools

static VALUE
debugbreak(int argc, VALUE *argv, VALUE obj)
{
#ifdef _WIN32
  if (IsDebuggerPresent()) {
    DebugBreak();
  }
#endif
  return Qnil;
}

static VALUE
dumptree(VALUE self, VALUE cls, VALUE methodName)
{
  mri::MethodEntry me(cls, mri::Symbol(methodName).id());
  mri::MethodDefinition def = me.methodDefinition();

  if (!def.hasAstNode()) {
    rb_raise(rb_eArgError, "method does not have the source code to be dumped");
  }

  return rb_parser_dump_tree(def.astNode(), 0);
}

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

  mi->compile();

  return Qnil;
}

void
Init_rbjit()
{
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
#endif

  IdStore::setup();
  PrimitiveStore::setup();
  NativeCompiler::setup();

  rb_define_method(rb_cObject, "debugbreak", (VALUE (*)(...))debugbreak, 0);
  rb_define_method(rb_cObject, "dumptree", (VALUE (*)(...))dumptree, 2);
  rb_define_method(rb_cObject, "precompile", (VALUE (*)(...))precompile, 2);

  CMethodInfo::construct(
    mri::Class::fixnumClass(), "+", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::fixnumClass()),
                          TypeExactClass::create(mri::Class::bignumClass())));
    CMethodInfo::construct(
    mri::Class::bignumClass(), "+", false,
    TypeSelection::create(TypeExactClass::create(mri::Class::fixnumClass()),
                          TypeExactClass::create(mri::Class::bignumClass())));
}

