#pragma once

#include "rbjit/rubyobject.h"

extern "C" {
struct RNode;
}

RBJIT_NAMESPACE_BEGIN

class MethodInfo;

namespace mri {

class MethodDefinition;

// Wrapper for the MRI's method_entry_t
//
// In method.h:
// typedef struct rb_method_entry_struct {
//     rb_method_flag_t flag;
//     char mark;
//     rb_method_definition_t *def;
//     ID called_id;
//     VALUE klass;                    /* should be mark */
// } rb_method_entry_t;
//
// In vm_eval.c
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

  // Look up a method and retrieve a method entry
  MethodEntry(Class cls, Id id);

  // Copy contructor
  MethodEntry(void* me) : me_(me) {}

  // Accessors
  MethodDefinition methodDefinition() const;
  Id methodName() const;

  bool canCall(CallType callType, Object self);

private:

  void* me_;
};

// Wrapper for rb_method_definition_t
//
// In method.h:
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
// } rb_method_definition_t;
class MethodDefinition {
public:

  MethodDefinition(void* def) : def_(def) {}

  bool hasAstNode() const;
  RNode* astNode() const;
  int argc() const;

  void setMethodInfo(MethodInfo* mi);

private:

  void* def_;
};

} // namespace mri

RBJIT_NAMESPACE_END
