#include <intrin.h> // suppress warning in ruby_atomic.h
#include <string>

#include "rbjit/methodinfo.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/primitivestore.h"
#include "rbjit/rubymethod.h"
#include "rbjit/typeconstraint.h"

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
  mri::MethodEntry me(mri::Class(cls), mri::Symbol(methodName).id());
  mri::MethodDefinition def = me.methodDefinition();

  if (!def.hasAstNode()) {
    rb_raise(rb_eArgError, "method does not have the source code to be compiled");
  }

  PrecompiledMethodInfo* mi =
    new PrecompiledMethodInfo(def.astNode(), mri::Symbol(methodName).name());
  def.setMethodInfo(mi);

  mi->compile();
  void* func = mi->methodBody();

  const char* oldName = mri::Symbol(methodName).name();
  std::string newName(oldName);
  newName += "_orig";
  rb_define_alias(cls, newName.c_str(), oldName);
  rb_define_method(cls, oldName, (VALUE (*)(...))func, def.argc());

  return Qnil;
}

void
Init_rbjit()
{
  PrimitiveStore::setup();
  NativeCompiler::setup();

  rb_define_method(rb_cObject, "debugbreak", (VALUE (*)(...))debugbreak, 0);
  rb_define_method(rb_cObject, "dumptree", (VALUE (*)(...))dumptree, 2);
  rb_define_method(rb_cObject, "precompile", (VALUE (*)(...))precompile, 2);

  MethodInfo::addToNativeMethod(mri::Class::fixnumClass(), "+",
    MethodInfo::NO, // hasDef
    MethodInfo::NO, // hasEval
    new TypeSelection(new TypeExactClass(mri::Class::fixnumClass()),
                      new TypeExactClass(mri::Class::bignumClass())));
}

