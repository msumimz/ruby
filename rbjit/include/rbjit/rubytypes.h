// Declarations of the minimal subset of the MRI's types
//
// The MRI's header files directory or indirectly included by ruby.h, define
// tons of macros that have very common names without any prefixes, and are
// likely to conflict with other codes and libraries.
//
// Example: 'TYPE' in ruby/ruby.h, 'ssize_t' in config.h, and 'accept', 'read',
// and 'send' in win32.h.
//
// This header file declares the minimal set of the types to allow access to
// the MRI's type system without confliction.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// The following definitions should match with those in ruby/ruby.h and node.h
typedef uintptr_t VALUE;
typedef uintptr_t ID;
struct RNode;
typedef struct RNode NODE;

#ifdef __cplusplus
}
#endif
