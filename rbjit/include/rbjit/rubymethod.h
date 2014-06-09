#pragma once

#include "rbjit/rubyobject.h"

extern "C" {
typedef struct rb_method_entry_struct rb_method_entry_t;
typedef struct rb_method_definition_struct rb_method_definition_t;
}

RBJIT_NAMESPACE_BEGIN

class MethodInfo;

namespace mri {

class MethodDefinition;

////////////////////////////////////////////////////////////
// MethodEntry
// Wrapper for the MRI's method_entry_t

// In method.h:
// typedef struct rb_method_entry_struct {
//     rb_method_flag_t flag;
//     char mark;
//     rb_method_definition_t *def;
//     ID called_id;
//     VALUE klass;                    /* should be mark */
// } rb_method_entry_t;
//
// In vm_eval.c:
// typedef enum call_type {
//     CALL_PUBLIC,
//     CALL_FCALL,
//     CALL_VCALL,
//     CALL_TYPE_MAX
// } call_type;

class MethodEntry {
public:

  enum CallType {
    CALL_PUBLIC,
    CALL_FCALL,
    CALL_VCALL,
    CALL_TYPE_MAX
  };

  MethodEntry() : me_(nullptr) {}

  // Look up a method and retrieve a method entry
  MethodEntry(VALUE cls, ID id);

  // Copy contructor
  MethodEntry(rb_method_entry_t* me) : me_(me) {}

  // Equality test
  bool operator==(const MethodEntry& other) const { return me_ == other.me_; }

  // Accessors
  rb_method_entry_t* methodEntry() const { return me_; }
  bool isNull() const { return me_ == nullptr; }
  MethodDefinition methodDefinition() const;
  ID methodName() const;
  Class class_() const;

  bool canCall(CallType callType, Object self);

  VALUE call(VALUE receiver, ID methodName, int argc, const VALUE* argv, VALUE defClass);
  VALUE call(VALUE receiver, int argc, const VALUE* argv);

private:

  rb_method_entry_t* me_;
};

////////////////////////////////////////////////////////////
// MethodDefinition
// Wrapper for the MRI's rb_method_definition_t

// In method.h:
//
// typedef enum {
//     VM_METHOD_TYPE_ISEQ,
//     VM_METHOD_TYPE_CFUNC,
//     VM_METHOD_TYPE_ATTRSET,
//     VM_METHOD_TYPE_IVAR,
//     VM_METHOD_TYPE_BMETHOD,
//     VM_METHOD_TYPE_ZSUPER,
//     VM_METHOD_TYPE_UNDEF,
//     VM_METHOD_TYPE_NOTIMPLEMENTED,
//     VM_METHOD_TYPE_OPTIMIZED, /* Kernel#send, Proc#call, etc */
//     VM_METHOD_TYPE_MISSING,   /* wrapper for method_missing(id) */
//     VM_METHOD_TYPE_REFINED,
//
//     END_OF_ENUMERATION(VM_METHOD_TYPE)
// } rb_method_type_t;
//
// typedef struct rb_method_definition_struct {
//     rb_method_type_t type; /* method type */
//     ID original_id;
//     union {
//         rb_iseq_t * const iseq;            /* should be mark */
//         rb_method_cfunc_t cfunc;
//         rb_method_attr_t attr;
//         const VALUE proc;                 /* should be mark */
//         enum method_optimized_type {
//             OPTIMIZED_METHOD_TYPE_SEND,
//             OPTIMIZED_METHOD_TYPE_CALL,
//
//             OPTIMIZED_METHOD_TYPE__MAX
//         } optimize_type;
//         struct rb_method_entry_struct *orig_me;
//     } body;
//     int alias_count;
//    // rbjit: JIT Compilation information
//    // (defined in rbjit/methodinfo.h)
//    void* jit_method_info;
// } rb_method_definition_t;

class MethodDefinition {
public:

  enum {
    VM_METHOD_TYPE_ISEQ,
    VM_METHOD_TYPE_CFUNC,
    VM_METHOD_TYPE_ATTRSET,
    VM_METHOD_TYPE_IVAR,
    VM_METHOD_TYPE_BMETHOD,
    VM_METHOD_TYPE_ZSUPER,
    VM_METHOD_TYPE_UNDEF,
    VM_METHOD_TYPE_NOTIMPLEMENTED,
    VM_METHOD_TYPE_OPTIMIZED, /* Kernel#send, Proc#call, etc */
    VM_METHOD_TYPE_MISSING,   /* wrapper for method_missing(id) */
    VM_METHOD_TYPE_REFINED
  };

  MethodDefinition(rb_method_definition_t* def) : def_(def) {}

  bool hasAstNode() const;
  RNode* astNode() const;
  int argc() const;

  MethodInfo* methodInfo() const;
  void setMethodInfo(MethodInfo* mi);

private:

  rb_method_definition_t* def_;
};

} // namespace mri

RBJIT_NAMESPACE_END
