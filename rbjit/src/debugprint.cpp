#ifdef RBJIT_DEBUG

#include <rbjit/debugprint.h>

RBJIT_NAMESPACE_BEGIN

DebugPrint DebugPrint::instance_("rbjit_debug.log");

DebugPrint::DebugPrint(const char* file)
  : out_(file)
{}

void
DebugPrint::print(const char* file, int line, const char* func, const char* msg)
{
  out_ << msg << std::flush;
}

void
DebugPrint::println(const char* file, int line, const char* func, const char* msg)
{
  out_ << msg << std::endl;
}

void
DebugPrint::printBar()
{
  out_ << "============================================================\n";
}

RBJIT_NAMESPACE_END

#endif // RBJIT_DEBUG
