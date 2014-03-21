#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include "rbjit/chunkallocator.h"

RBJIT_NAMESPACE_BEGIN

ChunkAllocator::ChunkAllocator(size_t chunkSize)
  : chunkSize_((chunkSize + PAGE_SIZE_UNIT - 1) & (PAGE_SIZE_UNIT - 1))
{
  createNewChunk();
}

ChunkAllocator::~ChunkAllocator()
{
  clear();
}

void*
ChunkAllocator::allocate(size_t size)
{
  if (size > chunkSize_) {
    return allocateLargeSize(size);
  }

  char* q = p_;
  p_ += size;
  if (p_ <= limit_) {
    return q;
  }

  createNewChunk();
  q = p_;
  p_ += size;

  return q;
}

void
ChunkAllocator::createNewChunk()
{
  char* p = (char*)VirtualAlloc(
    0, chunkSize_, MEM_COMMIT, PAGE_READWRITE);
  if (p == 0) {
    throw std::bad_alloc();
  }

  chunks_.push_back(p);

  p_ = p;
  limit_ = p + chunkSize_;
}

char*
ChunkAllocator::allocateLargeSize(size_t size)
{
  char* p = new char[size];
  largeAllocs_.push_back(p);
  return p;
}

void
ChunkAllocator::clear()
{
  for (auto i = chunks_.begin(), end = chunks_.end(), i != end; ++i) {
    VirtualFree(*i, chunkSize_, MEM_RELEASE);
  }

  for (auto i = largeAllocs_.begin(), end = largeAllocs_.end(), i != end; ++i) {
    delete *i;
  };

  chunks_.clear();
  largeAllocs_.clear();
}

RBJIT_NAMESPACE_END
