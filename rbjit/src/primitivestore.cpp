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

void
PrimitiveStore::load(void** pAddress, size_t* pSize)
{
  HRSRC resHandle = FindResource(nullptr, MAKEINTRESOURCE(IDR_PRIMITIVES1), TEXT("PRIMITIVES"));
  if (!resHandle) {
    abort();
  }

  HGLOBAL res = LoadResource(nullptr, resHandle);
  if (!res) {
    abort();
  }

  *pAddress = LockResource(res);
  *pSize = SizeofResource(nullptr, resHandle);
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
