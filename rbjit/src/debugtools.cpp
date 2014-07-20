#include <intrin.h> // Avoid warning that _interlockedOr is not declared

#include <cstdarg>
#include "rbjit/common.h"
#include "rbjit/debugprint.h"
#include "rbjit/rubymethod.h"

#ifdef _x64
# define PTRF "% 16Ix"
# define SPCF "                "
#else
# define PTRF "% 8Ix"
# define SPCF "        "
#endif

extern "C" {
#include "vm_core.h"
void rb_vmdebug_proc_dump_raw(rb_proc_t *proc);
}

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// Helper function

static const char*
getClassName(VALUE value)
{
  VALUE cls = CLASS_OF(value);
  if (!cls ||
      !(rb_type_p(cls, T_CLASS) ||
        rb_type_p(cls, T_ICLASS) ||
        rb_type_p(cls, T_MODULE))) {
    return "bogus class";
  }
  return rb_class2name(cls);
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
rbjit_log_ruby_stack_mri()
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

  std::string out = stringFormat(
    "----------------------------------------[%Ix: %s]\n"
    "VALUE *pc                   " PTRF "\n"
    "VALUE *sp                   " PTRF "\n"
    "rb_iseq_t *iseq             " PTRF " normal=%s\n"
    "VALUE flag                  " PTRF " type=%s finish=%s\n"
    "VALUE self                  " PTRF " (%s)\n"
    "VALUE klass                 " PTRF " (%s)\n"
    "VALUE *ep                   " PTRF " LEP=%s\n"
    "rb_iseq_t *block_iseq       " PTRF "\n"
    "VALUE proc                  " PTRF "\n"
    "const rb_method_entry_t *me " PTRF "\n",
    cf, name.c_str(),
    cf->pc,
    cf->sp,
    cf->iseq,
    RUBY_VM_NORMAL_ISEQ_P(cf->iseq) ? "true" : "false",
    cf->flag,
    type,
    VM_FRAME_TYPE_FINISH_P(cf) ? "true" : "false",
    cf->self, getClassName(cf->self),
    cf->klass, getClassName(cf->klass),
    cf->ep,
    VM_EP_LEP_P(cf->ep) ? "true" : "false",
    cf->block_iseq,
    cf->proc,
    cf->me
  );

  if (VM_EP_LEP_P(cf->ep)) {
    rb_block_t* b = VM_EP_BLOCK_PTR(cf->ep);
    if (b) {
      out += stringFormat(
	"[rb_block_t %Ix]\n"
	"  VALUE self      " PTRF "\n"
	"  VALUE klass     " PTRF "\n"
	"  VALUE *ep       " PTRF "\n"
	"  rb_iseq_t *iseq " PTRF "\n"
	"  VALUE proc      " PTRF "\n",
	b,
	b->self,
	b->klass,
	b->ep,
	b->iseq,
	b->proc
      );
    }
  }
  else {
    out += stringFormat(
      "[ep]\n"
      "  ep[ 0] prev frame : " PTRF " LEP=%s\n"
      "  ep[-1] CREF       : " PTRF "\n",
      VM_EP_PREV_EP(cf->ep),
      VM_EP_LEP_P(VM_EP_PREV_EP(cf->ep)) ? "true" : "false",
      cf->ep[-1]
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
// Print current interpreter stack frames

static std::string
debugPrintStackFrames()
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
      out += stringFormat(" (= %ld)", (long)((VALUE *)GC_GUARDED_PTR_REF(t) - th->stack));
    }

    out += " (";
    out += getClassName(*p);
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
  std::string out = stringFormat(
    "[rb_proc_t %Ix]\n"
    "rb_block_t block\n"
    "  VALUE self       " PTRF "\n"
    "  VALUE klass      " PTRF "\n"
    "  VALUE *ep        " PTRF " LEP=%s\n"
    "  rb_iseq_t *iseq  " PTRF "\n"
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
    VM_EP_LEP_P(proc->block.ep) ? "true" : "false",
    proc->block.iseq,
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
      out += stringFormat("  %s%04d:" PTRF ": " PTRF " (%s)\n",
        env->env +i == proc->block.ep ? "*" : " ",
        -env->local_size + i,
        env->env + i,
        value,
        getClassName(value));
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
rbjit_log_ruby_stack()
{
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT("Interperter stack frames\n");
  RBJIT_DPRINT_BAR();
  RBJIT_DPRINT(debugPrintStackFrames());
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
  rb_define_method(rb_cObject, "rbjit_log_ruby_stack_mri", (VALUE (*)(...))rbjit_log_ruby_stack_mri, 0);
  rb_define_method(rb_cObject, "rbjit_log_proc_mri", (VALUE (*)(...))rbjit_log_proc_mri, 1);

  rb_define_method(rb_cObject, "rbjit_debug_break", (VALUE (*)(...))rbjit_debug_break, 0);
  rb_define_method(rb_cObject, "rbjit_dump_tree", (VALUE (*)(...))rbjit_dump_tree, 2);

  rb_define_method(rb_cObject, "rbjit_log_control_frames", (VALUE (*)(...))rbjit_log_control_frames, 0);
  rb_define_method(rb_cObject, "rbjit_log_ruby_stack", (VALUE (*)(...))rbjit_log_ruby_stack, 0);
  rb_define_method(rb_cObject, "rbjit_log_proc", (VALUE (*)(...))rbjit_log_proc, 1);
}

RBJIT_NAMESPACE_END
