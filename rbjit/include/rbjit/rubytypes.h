// Declarations of the minimal subset of the MRI types
//
// The MRI header files included directly or indirectly through ruby.h, define
// tons of global macros that have very common names without any prefixes, and
// are likely to conflict with third-party codes and libraries.
//
// Example: 'TYPE', 'SIGNED_VALUE' in ruby/ruby.h, 'ssize_t' in config.h, and
// 'accept', 'read', and 'send' in win32.h.
//
// The purpose of this file is to declare the minimal set of the MRI types to
// allow access to the MRI type system without defining any unsafe macros.

#pragma once

extern "C" {

// ruby/ruby.h
typedef uintptr_t VALUE;
typedef uintptr_t ID;

// node.h
struct RNode;
typedef struct RNode NODE;

}
