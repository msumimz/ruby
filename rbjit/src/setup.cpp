#include <intrin.h> // supress warning in ruby_atomic.h

extern "C" {
#include "ruby/ruby.h"
#include "method.h" // rb_method_entry_t, rb_method_definition_t
#include "vm_core.h" // rb_iseq_t
#include "node.h" // rb_parser_dump_tree
}

#include "rbjit/common.h"

extern "C" {
void Init_rbjit();
}

////////////////////////////////////////////////////////////
// Development tools

static VALUE
debugbreak(int argc, VALUE *argv, VALUE obj)
{
  __asm { int 3 }
  return Qnil;
}

static VALUE
dumptree(VALUE self, VALUE cls, VALUE methodName)
{
  rb_method_entry_t* me = rb_method_entry(cls, SYM2ID(methodName), 0);

  rb_method_definition_t* def = me->def;
  if (def->type != VM_METHOD_TYPE_ISEQ) {
    rb_raise(rb_eArgError, "method does not have iseq");
  }

  NODE* node = def->body.iseq->node;

  return rb_parser_dump_tree(node, 0);
}

void
Init_rbjit()
{
  rb_define_method(rb_cObject, "debugbreak", (VALUE (*)(...))debugbreak, 0);
  rb_define_method(rb_cObject, "dumptree", (VALUE (*)(...))dumptree, 2);
}

