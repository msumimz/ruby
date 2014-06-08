// Declarations of the minimal subset of the MRI's types
//
// The MRI's header files, ruby.h and those indirectly included by ruby.h,
// define tons of macros that have very common names without any prefixes, and
// are thus likely to conflict with other libraries.
//
// Example: 'TYPE' in ruby/ruby.h, 'ssize_t' in config.h, and 'accept', 'read',
// and 'send' in win32.h.
//
// This header file declares the minimal set of types to allow access to the
// MRI's type system without confliction.

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
