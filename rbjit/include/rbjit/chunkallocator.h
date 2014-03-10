#include <vector>
#include <cassert>
#include <limits>
#include "rbjit/common.h"

RBJIT_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////
// ChunkAllocator
//
// Combination of variable-size allocations and a total deallocation

class ChunkAllocator {
public:

  enum {
    PAGE_SIZE_UNIT = 4 * 1024
  };

  ChunkAllocator(size_t chunkSize);
  ~ChunkAllocator();

  size_t chunkSize() const { return chunkSize_; }

  void* allocate(size_t size);
  void clear();

private:

  void createNewChunk();
  char* allocateLargeSize(size_t size);

  char* p_;
  char* limit_;
  size_t chunkSize_;
  std::vector<char*> chunks_;
  std::vector<char*> largeAllocs_;

};

////////////////////////////////////////////////////////////
// ChunkAllocatorHolder
//
// Holds global ChunkAllocator instances identified by ID and CHUNK_SIZE

template <int ID, size_t CHUNK_SIZE>
class ChunkAllocatorHolder {
public:
  enum { UNIQUE_ID = ID };

  static ChunkAllocator* instance() {
    static ChunkAllocator alloc(CHUNK_SIZE);
    return &alloc;
  }

  static void* allocate(size_t size)
  { return instance()->allocate(size); }

  static void clear()
  { instance()->clear(); }

};

////////////////////////////////////////////////////////////
// StlChunkAllocator
//
// STL adaptor for ChunkAllocator

template <int ID, class T, size_t CHUNK_SIZE = 16 * 1024>
class StlChunkAllocator {
public:

  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;

  template <class U>
  struct rebind {
    typedef StlChunkAllocator<ID, U, CHUNK_SIZE> other;
  };

  StlChunkAllocator() throw() {}
  StlChunkAllocator(const StlChunkAllocator&) throw() {}

  template <class U>
  StlChunkAllocator(const StlChunkAllocator<ID, U, CHUNK_SIZE>&) throw() {}

  ~StlChunkAllocator() throw() {}

  pointer
  allocate(size_type num, const void* hint = 0)
  {
    return (pointer)ChunkAllocatorHolder<ID, CHUNK_SIZE>::allocate(num * sizeof(T));
  }

  void
  construct(pointer p, const T& value)
  {
    new ((void*)p) T(value);
  }

  void
  deallocate(pointer p, size_type num)
  {
    // do nothing
  }

  void
  destroy(pointer p)
  {
    p->~T();
  }

  pointer address(reference value) const { return &value; }
  const_pointer address(const_reference value) const { return &value; }

  size_type
  max_size() const throw()
  {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }
};

template <int ID1, int ID2, class T1, class T2, size_t CHUNK_SIZE1, size_t CHUNK_SIZE2>
bool operator==(const StlChunkAllocator<ID1, T1, CHUNK_SIZE1>&, const StlChunkAllocator<ID2, T2, CHUNK_SIZE2>&) throw() { return true; }

template <int ID1, int ID2, class T1, class T2, size_t CHUNK_SIZE1, size_t CHUNK_SIZE2>
bool operator!=(const StlChunkAllocator<ID1, T1, CHUNK_SIZE1>&, const StlChunkAllocator<ID2, T2, CHUNK_SIZE2>&) throw() { return false; }

RBJIT_NAMESPACE_END
