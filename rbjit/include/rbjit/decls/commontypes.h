// Declarations of the minimal subset of the MRI types
//
// The MRI header files included directly or indirectly through ruby.h, define
// tons of global macros that have very common names without any prefixes, and
// are likely to conflict with third-party codes and libraries.
//
// Example: 'TYPE', 'SIGNED_VALUE' in ruby/ruby.h, 'ssize_t' in config.h, and
// 'accept', 'read', and 'send' in win32.h.
//
// The purposes of this and the other files in decls/*.h are the following:
//
// (1) Declare the minimal set of the MRI types to allow access to the MRI type
// system without defining any unsafe macros
//
// (2) By duplicating type definitions, make the compiler automatically detect
// significant changes in the mainstream implementation

#pragma once

extern "C" {

// ruby/ruby.h
typedef uintptr_t VALUE;
typedef uintptr_t ID;

// node.h
struct RNode;
typedef struct RNode NODE;

// vm_core.h
typedef struct rb_vm_struct rb_vm_t;

typedef struct rb_thread_struct rb_thread_t;
typedef struct rb_control_frame_struct rb_control_frame_t;
typedef struct rb_block_struct rb_block_t;

typedef struct rb_iseq_struct rb_iseq_t;
typedef struct rb_call_info_struct rb_call_info_t;

// method.h
typedef struct rb_method_definition_struct rb_method_definition_t;
typedef struct rb_method_entry_struct rb_method_entry_t;

typedef unsigned long rb_num_t;
typedef unsigned long long rb_serial_t;

// st.h
typedef struct st_table st_table;

}
