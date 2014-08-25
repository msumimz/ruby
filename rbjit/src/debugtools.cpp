#include <intrin.h> // Avoid warning that _interlockedOr is not declared

#include <cstdarg>
#include "rbjit/common.h"
#include "rbjit/debugprint.h"
#include "rbjit/rubymethod.h"

#include "ruby.h"
extern "C" {
#include "node.h"
#include "vm_core.h"
#include "method.h"

void rb_vmdebug_proc_dump_raw(rb_proc_t *proc);
}

#ifdef _x64
# define PTRF "% 16Ix"
# define SPCF "                "
#else
# define PTRF "% 8Ix"
# define SPCF "        "
#endif

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// Helper function

static std::string
getObjectRepr(VALUE value)
{
  if (!value) {
    return "nullptr";
  }

  VALUE cls = CLASS_OF(value);
  if (!cls ||
      !(rb_type_p(cls, T_CLASS) ||
        rb_type_p(cls, T_ICLASS) ||
        rb_type_p(cls, T_MODULE))) {
    return "bogus class";
  }

  switch (rb_type(value)) {
  case T_CLASS:
    return stringFormat("Class '%s'", rb_class2name(value));
  case T_MODULE:
    return stringFormat("Module '%s'", rb_class2name(value));
  case T_NIL: return "nil";
  case T_TRUE: return "true";
  case T_FALSE: return "false";
  case T_SYMBOL:
    return stringFormat(":%s", rb_id2name(SYM2ID(value)));
  case T_FIXNUM:
    return stringFormat("Fixnum %ld", FIX2LONG(value));
  case T_FLOAT:
    return stringFormat("Float %g", rb_float_value(value));
  case T_STRING:
    return stringFormat("String '%s'", RSTRING_PTR(value));
  default:
    return rb_class2name(cls);
  }
}

////////////////////////////////////////////////////////////
// MRI debugging methods

static std::string dumpOutput;

extern "C" int
fprintfOverloaded(FILE* out, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  dumpOutput += stringFormatVarargs(format, args);
  return 1;
}

static VALUE
rbjit_log_object_stack_mri()
{
  dumpOutput.clear();
  rb_vmdebug_stack_dump_raw(GET_THREAD(), GET_THREAD()->cfp);
  RBJIT_DPRINT(dumpOutput);
  return Qnil;
}

static VALUE
rbjit_log_proc_mri(VALUE self, VALUE proc)
{
  rb_proc_t* p;
  GetProcPtr(proc, p);
  if (p) {
    dumpOutput.clear();
    rb_vmdebug_proc_dump_raw(p);
    RBJIT_DPRINT(dumpOutput);
  }
  else {
    RBJIT_DPRINT("proc is null\n");
  }
  return Qnil;
}

static std::string
getCRefChain(NODE* cref)
{
  if (!cref || NIL_P(cref)) {
    return "()";
  }

  std::string out = "(";
  for (; cref; cref = cref->nd_next) {
    VALUE cls = cref->nd_clss;
    if (NIL_P(cls)) {
      out += "nil";
    }
    else if (rb_type(cls) == T_CLASS || rb_type(cls) == T_MODULE) {
      out += '\'';
      out += rb_class2name(cls);
      out += '\'';
    }
    else {
      out += getObjectRepr(cls);
    }

    if (cref->flags & NODE_FL_CREF_PUSHED_BY_EVAL) {
      out += "<E>";
    }

    if (cref->flags & NODE_FL_CREF_OMOD_SHARED) {
      out += "<O>";
    }

    if (cref->nd_next) {
      out += ' ';
    }
  }
  out += ')';

  return out;
}

static std::string
getCRefChain(rb_iseq_t* iseq)
{
  if (!iseq) {
    return "()";
  }

  return getCRefChain(iseq->cref_stack);
}

////////////////////////////////////////////////////////////
// Print a single control frame

static std::string
debugPrintSingleControlFrame(rb_control_frame_t* cf)
{
  // Frame type
  const char* type;
  switch (VM_FRAME_TYPE(cf)) {
  case VM_FRAME_MAGIC_TOP: type = "TOP"; break;
  case VM_FRAME_MAGIC_METHOD: type = "METHOD"; break;
  case VM_FRAME_MAGIC_CLASS: type = "CLASS"; break;
  case VM_FRAME_MAGIC_BLOCK: type = "BLOCK"; break;
  case VM_FRAME_MAGIC_CFUNC: type = "CFUNC"; break;
  case VM_FRAME_MAGIC_PROC: type = "PROC"; break;
  case VM_FRAME_MAGIC_LAMBDA: type = "LAMBDA"; break;
  case VM_FRAME_MAGIC_IFUNC: type = "IFUNC"; break;
  case VM_FRAME_MAGIC_EVAL: type = "EVAL"; break;
  case 0: type = "------"; break;
  default: type = "(none)";
  }

  // Frame name
  std::string name;
  if (cf->iseq != 0) {
    if (RUBY_VM_IFUNC_P(cf->iseq)) {
      name = "<ifunc>";
    }
    else {
      ptrdiff_t pc = cf->pc - cf->iseq->iseq_encoded;
      const char* iseq_name = RSTRING_PTR(cf->iseq->location.label);
      int line = rb_vm_get_sourceline(cf);
      if (line) {
        name = stringFormat("%s (%s:%d)", iseq_name, RSTRING_PTR(cf->iseq->location.path), line);
      }
    }
  }
  else if (cf->me) {
    const char* iseq_name = rb_id2name(cf->me->def->original_id);
    name = stringFormat("%s", iseq_name);
  }

  std::string selfRepr = getObjectRepr(cf->self);
  std::string klassRepr = getObjectRepr(cf->klass);
  std::string cref = getCRefChain(cf->iseq);
  std::string crefBlock = getCRefChain(cf->block_iseq);
  std::string out = stringFormat(
    "----------------------------------------[%Ix: %s]\n"
    "VALUE *pc                   " PTRF "\n"
    "VALUE *sp                   " PTRF "\n"
    "rb_iseq_t *iseq             " PTRF " normal=%s cref=%s\n"
    "VALUE flag                  " PTRF " type=%s finish=%s\n"
    "VALUE self                  " PTRF " (%s)\n"
    "VALUE klass                 " PTRF " (%s)\n"
    "VALUE *ep                   " PTRF " -> %Ix LEP=%s\n"
    "rb_iseq_t *block_iseq       " PTRF " cref=%s\n"
    "VALUE proc                  " PTRF "\n"
    "const rb_method_entry_t *me " PTRF "\n",
    cf, name.c_str(),
    cf->pc,
    cf->sp,
    cf->iseq,
    RUBY_VM_NORMAL_ISEQ_P(cf->iseq) ? "true" : "false",
    cref.c_str(),
    cf->flag,
    type,
    VM_FRAME_TYPE_FINISH_P(cf) ? "true" : "false",
    cf->self, selfRepr.c_str(),
    cf->klass, klassRepr.c_str(),
    cf->ep,
    cf->ep[0],
    VM_EP_LEP_P(cf->ep) ? "true" : "false",
    cf->block_iseq,
    crefBlock.c_str(),
    cf->proc,
    cf->me
  );

  if (VM_EP_LEP_P(cf->ep)) {
    rb_block_t* b = VM_EP_BLOCK_PTR(cf->ep);
    if (b) {
      std::string cref = getCRefChain(b->iseq);
      out += stringFormat(
	"[rb_block_t %Ix]\n"
	"  VALUE self      " PTRF "\n"
	"  VALUE klass     " PTRF "\n"
	"  VALUE *ep       " PTRF "\n"
	"  rb_iseq_t *iseq " PTRF " cref=%s\n"
	"  VALUE proc      " PTRF "\n",
	b,
	b->self,
	b->klass,
	b->ep,
	b->iseq,
        cref.c_str(),
	b->proc
      );
    }
  }
  else {
    std::string cref = getCRefChain((NODE*)cf->ep[-1]);
    out += stringFormat(
      "[ep]\n"
      "  ep[ 0] prev frame : " PTRF " -> %Ix\n"
      "  ep[-1] CREF       : " PTRF " cref=%s\n",
      cf->ep[0],
      VM_EP_PREV_EP(cf->ep),
      cf->ep[-1],
      cref.c_str()
    );
  }

  return out;
}

////////////////////////////////////////////////////////////
// Print current control frames

static std::string
debugPrintCurrentControlFrames()
{
  rb_thread_t* th = GET_THREAD();
  rb_control_frame_t* cfp = th->cfp;

  std::string out;
  while ((void*)cfp < (void*)(th->stack + th->stack_size)) {
    out += debugPrintSingleControlFrame(cfp);
    ++cfp;
  }
  return out;
}

////////////////////////////////////////////////////////////
// Print object stack

static std::string
debugPrintObjectStack()
{
  rb_thread_t* th = GET_THREAD();
  rb_control_frame_t* cfp = th->cfp;
  VALUE *sp = cfp->sp;
  VALUE *p, *st, *t;

  std::string out;
  for (st = th->stack, p = sp - 1; p >= st; --p) {
    out += stringFormat("%04ld:" PTRF ": " PTRF, (long)(p - st), p, *p);

    t = (VALUE *)*p;
    if (th->stack <= t && t < sp) {
      out += stringFormat(" (=offset %ld)", (long)((VALUE *)GC_GUARDED_PTR_REF(t) - th->stack));
    }

    out += " (";
    out += getObjectRepr(*p).c_str();
    out += ")";

    out += "\n";
  }

  return out;
}

////////////////////////////////////////////////////////////
// Print rb_proc_t

static std::string
debugPrintProc(rb_proc_t* proc)
{
  std::string cref = getCRefChain(proc->block.iseq);
  std::string out = stringFormat(
    "[rb_proc_t %Ix]\n"
    "rb_block_t block\n"
    "  VALUE self       " PTRF "\n"
    "  VALUE klass      " PTRF "\n"
    "  VALUE *ep        " PTRF "\n"
    "  rb_iseq_t *iseq  " PTRF " cref=%s\n"
    "  VALUE proc       " PTRF "\n"
    "VALUE envval       " PTRF "\n"
    "VALUE blockprocval " PTRF "\n"
    "int safe_level     " PTRF "\n"
    "int is_from_method " PTRF "\n"
    "int is_lambda      " PTRF "\n",
    proc,
    proc->block.self,
    proc->block.klass,
    proc->block.ep,
    proc->block.iseq,
    cref,
    proc->block.proc,
    proc->envval,
    proc->blockprocval,
    proc->safe_level,
    proc->is_from_method,
    proc->is_lambda
    );

  // Env
  rb_env_t* env;
  GetEnvPtr(proc->envval, env);
  while (env) {
    out += stringFormat(
      "[rb_env_t %Ix]\n"
      "VALUE *env        " PTRF "\n"
      "int env_size      " PTRF "\n"
      "int local_size    " PTRF "\n"
      "VALUE prev_envval " PTRF "\n"
      "rb_block_t block  " PTRF "\n",
      env,
      env->env,
      env->env_size,
      env->local_size,
      env->prev_envval,
      env->block
      );

    // Object stack
    out += stringFormat("  ----------\n");
    for (int i = 0; i < env->env_size; ++i) {
      VALUE value = env->env[i];
      std::string valueRepr = getObjectRepr(value);
      out += stringFormat("  %s%04d:" PTRF ": " PTRF " (%s)\n",
	env->env + i == proc->block.ep ? "*" : " ",
	-env->local_size + i,
	env->env + i,
	value,
	valueRepr.c_str());
    }

    if (env->prev_envval != 0) {
      GetEnvPtr(env->prev_envval, env);
    }
    else {
      env = 0;
    }
  }

  return out;
}

////////////////////////////////////////////////////////////
// Method definitions

static VALUE
rbjit_debug_break()
{
#ifdef _WIN32
  if (IsDebuggerPresent()) {
    DebugBreak();
  }
#endif
  return Qnil;
}

static VALUE
rbjit_dump_tree(VALUE self, VALUE cls, VALUE methodName)
{
  mri::MethodEntry me(cls, mri::Symbol(methodName).id());
  mri::MethodDefinition def = me.methodDefinition();

  if (!def.hasAstNode()) {
    rb_raise(rb_eArgError, "method does not have the source code to be dumped");
  }

  return rb_parser_dump_tree(def.astNode(), 0);
}

static VALUE
rbjit_log_control_frames()
{
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT("rb_control_frame_t\n");
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT(debugPrintCurrentControlFrames());
  return Qnil;
}

static VALUE
rbjit_log_object_stack()
{
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT("Object stack\n");
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT(debugPrintObjectStack());
  return Qnil;
}

static VALUE
rbjit_log_proc(VALUE self, VALUE procValue)
{
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT("rb_proc_t\n");
  RBJIT_DPRINT_BAR();

  rb_proc_t* proc;
  GetProcPtr(procValue, proc);
  RBJIT_DPRINT(debugPrintProc(proc));
  return Qnil;
}

extern "C" void
Init_rbjitDebug()
{
  // MRI debugging methods
  rb_define_method(rb_cObject, "rbjit_log_object_stack_mri", (VALUE (*)(...))rbjit_log_object_stack_mri, 0);
  rb_define_method(rb_cObject, "rbjit_log_proc_mri", (VALUE (*)(...))rbjit_log_proc_mri, 1);

  rb_define_method(rb_cObject, "rbjit_debug_break", (VALUE (*)(...))rbjit_debug_break, 0);
  rb_define_method(rb_cObject, "rbjit_dump_tree", (VALUE (*)(...))rbjit_dump_tree, 2);

  rb_define_method(rb_cObject, "rbjit_log_control_frames", (VALUE (*)(...))rbjit_log_control_frames, 0);
  rb_define_method(rb_cObject, "rbjit_log_object_stack", (VALUE (*)(...))rbjit_log_object_stack, 0);
  rb_define_method(rb_cObject, "rbjit_log_proc", (VALUE (*)(...))rbjit_log_proc, 1);
}

RBJIT_NAMESPACE_END
