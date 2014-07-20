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
  const int BUFFER_SIZE = 2048;
  char buf[BUFFER_SIZE];
  vsnprintf(buf, BUFFER_SIZE, format, args);
  buf[BUFFER_SIZE-1] = 0;

  return std::string(buf);
}

RBJIT_NAMESPACE_END
