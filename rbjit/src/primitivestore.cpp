#define NOMINMAX
#include <windows.h>
#include "primitives/resource.h"
#include "rbjit/primitivestore.h"
#include "rbjit/nativecompiler.h"
#include "rbjit/rubyobject.h"

#include "llvm/IR/Module.h"

RBJIT_NAMESPACE_BEGIN

PrimitiveStore* PrimitiveStore::instance_ = 0;

void
PrimitiveStore::setup()
{
  instance_ = new PrimitiveStore;
}

static __declspec(noinline) HINSTANCE
getDllModuleHandle()
{
  // Find the module handle in which the caller function resides.
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(_ReturnAddress(), &mbi, sizeof(mbi))) {
    // HINSTANCE is actually the base address of the module.
    return (HINSTANCE)mbi.AllocationBase;
  }
  return NULL;
}

void
PrimitiveStore::load(void** pAddress, size_t* pSize)
{
  HINSTANCE handle = getDllModuleHandle();

  HRSRC resHandle = FindResource(handle, MAKEINTRESOURCE(IDR_PRIMITIVES1), TEXT("PRIMITIVES"));
  if (!resHandle) {
    throw std::runtime_error("[Bug] FindResource failed; Can't load primitives");
  }

  HGLOBAL res = LoadResource(handle, resHandle);
  if (!res) {
    throw std::runtime_error("[Bug] LoadResource failed; Can't load primitives");
  }

  *pAddress = LockResource(res);
  *pSize = SizeofResource(handle, resHandle);
}

void
PrimitiveStore::buildLookupMap(llvm::Module* module)
{
  auto i = module->begin();
  auto end = module->end();
  for (; i != end; ++i) {
    names_.insert(mri::Id(i->getName().data()));
  }
}

bool
PrimitiveStore::isPrimitive(ID name) const
{
  return names_.find(name) != names_.end();
}

RBJIT_NAMESPACE_END
