// Definitions of a minimal set of types of the MRI's type system
//
// The MRI's header files, ruby.h and the ones indirectly included by ruby.h,
// define tons of macros of very common names without any prefixes, and are
// thus likely to conflict with other libraries.
//
// Example: 'TYPE' in ruby/ruby.h, 'ssize_t' in config.h, and 'accept', 'read',
// and 'send' in win32.h.
//
// This header file defines a minimal set of the types in ruby.h to allow
// access to the MRI's type system without confliction.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t VALUE;
typedef size_t ID;
struct RNode;

#ifdef __cplusplus
}
#endif
