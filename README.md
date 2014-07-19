Rbjit
===

Rbjit is a LLVM-based JIT-complication engine for the Ruby programming
language.

Overview
---

Rbjit extends Ruby by adding a JIT-compilation engine. Basically, Rbjit
executes your programs by the interpreter as the standard Ruby. When indicated,
it compiles a specific method and replaces its definition with the compiled
code, as if it is a method of an extension library written by C.

The major advantage of Rbjit is compatibility. As long as your program is
running on the interpreter, Rbjit makes no difference compared to the standard
Ruby. Once compiled, it "just runs faster". Rbjit will stop compiling and fail
over to the interpreter if the compiled code can't yield the same result as the
interpreted code, so you would never notice any incompatibility, except
performance.

Furthermore, Rbjit has been implemented with minimal changes to the standard
Ruby code base. Thus Rbjit can easily follow the latest releases of the
standard Ruby.

The goal of the project is to develop a drop-in replacement for the standard
Ruby installation. When automatic JIT compilation is enabled, every Ruby
programs will be able to gain reasonable performance increase with little cost.

Project Status
---

*This project is in alpha status and not available for non-developers.*

Installation
---

Perhaps it would be better to experience the standard Ruby build process
beforehand.

Prerequisites:

- Windows XP or later (32 bit version)
- Visual Studio 2010 or later
- LLVM 3.4
- git (to obtain the source code)

First, install LLVM 3.4 according to [LLVM Installation Guide for Windows].

Open "Visual Studio Command Prompt" and type the following commands:

```sh
>git clone https://github.com/msumimz/ruby/tree/rbjit
>cd ruby
>win32\configure.bat --llvm-prefix=c:\llvm\llvm-3.4
>nmake
>nmake test
>nmake install
```

The `win32\configure.bat` build script provides the additional option
`--llvm-prefix` to specify an installation location of LLVM.

As a result, the same command is built as Ruby, that is, `ruby`. The `ruby` command built with Rbjit displays `rbjit` in the end of the version string, as shown below:

```sh
>ruby -v
ruby 2.1.0p0 (2013-12-25 revision 44417) [i386-mswin32_100] rbjit
```

Linux version will be available soon.

For developers:<br/>
In the `rbjit/vc10` folder of the project tree, there is a Visual Studio
solution file named `rbjit.sln`. This file is configured to build `miniruby`
instead of the full installation.  The debug build of this `miniruby` is set up
to output a log file named `rbjit_debug.log` to record a detailed compilation
process. You can use this file to investigate the behavior of the program.

[LLVM Installation Guide for Windows]: http://www.llvm.org/docs/GettingStartedVS.html

How to Use
---

Rbjit is compatible with the stanard Ruby, and you can use Rbjit in the same way.

In the current version, JIT compilation is not automatically enabled. To
compile a method, use the module function `precompile` defined in the module
`Jit`.

```ruby
def m
  puts "hello rbjit"
end

Jit.precompile Object, :m

m   # => "hello rbjit"
```

By the above code, the method `m` will be compiled and run in native code. If
Rbjit fails to compile, it will raise an exception.

Supported Syntax
---

The following syntax constructs are supported by the JIT compiler in the current version:

- Simple local variable assignment
- Numeric and symbol literals
- Method calls with simple fixed-size arguments
- Operators which result in method calls, such as `+`, `-`, and so on
- `self`, `true`, `false`, `nil`
- `if`, `unless`, `while`
- `&&`, `||`, `and`, `or`
- `return`

License
----

Copyright &copy; 2014 msumimz.<br/>
Licensed under the MIT License.
