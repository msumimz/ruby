#include <cstdio>
#include <cstdarg>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

std::string
stringFormat(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  return stringFormatVarargs(format, args);
}

std::string
stringFormatVarargs(const char* format, va_list args)
{
  char buf[256];
  vsnprintf(buf, 255, format, args);
  buf[255] = 0;

  return std::string(buf);
}

RBJIT_NAMESPACE_END
