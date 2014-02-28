#pragma once

#ifdef RBJIT_DEBUG

#include <fstream>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

class DebugPrint {
public:

  DebugPrint(const char* file);

  void print(const char* file, int line, const char* func, const char* msg);
  void print(const char* file, int line, const char* func, std::string msg)
  { print(file, line, func, msg.c_str()); }

  void println(const char* file, int line, const char* func, const char* msg);
  void println(const char* file, int line, const char* func, std::string msg)
  { println(file, line, func, msg.c_str()); }

  static DebugPrint* instance() { return &instance_; }

private:

  static DebugPrint instance_;

  std::ofstream out_;

};

#define RBJIT_DPRINT(msg) DebugPrint::instance()->print(__FILE__, __LINE__, __FUNCTION__, msg)
#define RBJIT_DPRINTLN(msg) DebugPrint::instance()->println(__FILE__, __LINE__, __FUNCTION__, msg)

RBJIT_NAMESPACE_END

#endif // RBJIT_DEBUG
