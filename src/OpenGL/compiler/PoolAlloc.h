// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _POOL_ALLOC_INCLUDED_
#define _POOL_ALLOC_INCLUDED_

#include <cstddef>
#ifndef DISABLE_DEBUG
#include <cstring>
#endif
#include <vector>

#if !defined(DISABLE_POOL_ALLOC)
//
// This class defines an allocator that can be used to efficiently
// allocate a large number of small requests for heap memory, with the
// intention that they are not individually deallocated, but rather
// collectively deallocated at one time.
//
// If we are using guard blocks, we must track each indivual allocation.
// If we aren't using guard blocks, these never get instantiated.
//

class TAllocation
{
public:
	TAllocation(size_t size, unsigned char* mem, TAllocation* prev = 0) :
		size(size), mem(mem), prevAlloc(prev)
	{
		// If we are using guard blocks, all allocations are bracketed:
		//   [allocationHeader][initialGuardBlock][userData][finalGuardBlock]
		#ifndef DISABLE_DEBUG
		memset(preGuard(),  guardBlockBeginVal, guardBlockSize);
		memset(data(),      userDataFill,       size);
		memset(postGuard(), guardBlockEndVal,   guardBlockSize);
		#endif
	}

	void check() const
	{
		checkGuardBlock(preGuard(),  guardBlockBeginVal, "before");
		checkGuardBlock(postGuard(), guardBlockEndVal,   "after");
	}

	void checkAllocList() const;

	// Return total size needed to accomodate user buffer of 'size',
	// plus our tracking data.
	inline static size_t allocationSize(size_t size) { return size + 2 * guardBlockSize + headerSize(); }

	// Offset from surrounding buffer to get to user data buffer.
	inline static unsigned char* offsetAllocation(unsigned char* m) { return m + guardBlockSize + headerSize(); }

private:
	void checkGuardBlock(unsigned char* blockMem, unsigned char val, const char* locText) const;

	// Find offsets to pre and post guard blocks, and user data buffer
	unsigned char* preGuard()  const { return mem + headerSize(); }
	unsigned char* data()      const { return preGuard() + guardBlockSize; }
	unsigned char* postGuard() const { return data() + size; }

	size_t size;            // size of the user data area
	unsigned char* mem;     // beginning of our allocation
	TAllocation* prevAlloc; // prior allocation in the chain

	const static unsigned char guardBlockBeginVal;
	const static unsigned char guardBlockEndVal;
	const static unsigned char userDataFill;

	const static size_t guardBlockSize;
	#ifndef DISABLE_DEBUG
	inline static size_t headerSize() { return sizeof(TAllocation); }
	#else
	inline static size_t headerSize() { return 0; }
	#endif
};
#endif

//
// There are several stacks. One is to track the pushing and popping of the
// user. The others are simply a repositories of free pages or used pages.
//
// Page stacks are linked together with a simple header at the beginning
// of each allocation obtained from the underlying OS.
// Individual allocations are kept for future re-use.
//
class TPoolAllocator
{
public:
	TPoolAllocator(int growthIncrement = 8 * 1024, int allocationAlignment = 16);

	// Don't call the destructor just to free up the memory, call pop()
	~TPoolAllocator();

	// Call push() to establish a new place to pop memory too.
	void push();

	// Call pop() to free all memory allocated since the last call to push(),
	// or if no last call to push, frees all memory since first allocation.
	void pop();

	// Call popAll() to free all memory allocated.
	void popAll();

	// Call allocate() to actually acquire memory. Returns 0 if no memory
	// available, otherwise a properly aligned pointer to 'numBytes' of memory.
	void* allocate(size_t numBytes);

	// There is no deallocate. The point of this class is that
	// deallocation can be skipped by the user of it, as the model
	// of use is to simultaneously deallocate everything at once
	// by calling pop(), and to not have to solve memory leak problems.

private:
	size_t alignment; // all returned allocations will be aligned at this granularity, which will be a power of 2
	size_t alignmentMask;

#if !defined(DISABLE_POOL_ALLOC)
	friend struct tHeader;

	struct tHeader
	{
		tHeader(tHeader* nextPage, size_t pageCount) :
			nextPage(nextPage),
			pageCount(pageCount)
			#ifndef DISABLE_DEBUG
		  , lastAllocation(0)
			#endif
		{
		}

		~tHeader()
		{
			#ifndef DISABLE_DEBUG
			if(lastAllocation)
				lastAllocation->checkAllocList();
			#endif
		}

		tHeader* nextPage;
		size_t pageCount;
		#ifndef DISABLE_DEBUG
		TAllocation* lastAllocation;
		#endif
	};

	struct tAllocState
	{
		size_t offset;
		tHeader* page;
	};
	typedef std::vector<tAllocState> tAllocStack;

	// Track allocations if and only if we're using guard blocks.
	void* initializeAllocation(tHeader* block, unsigned char* memory, size_t numBytes)
	{
		#ifndef DISABLE_DEBUG
		new(memory) TAllocation(numBytes, memory, block->lastAllocation);
		block->lastAllocation = reinterpret_cast<TAllocation*>(memory);
		#endif
		return TAllocation::offsetAllocation(memory);
	}

	size_t pageSize;          // granularity of allocation
	size_t headerSkip;        // amount of memory to skip to make room for the header
	size_t currentPageOffset; // next offset in top of inUseList to allocate from
	tHeader* freeList;        // list of popped memory
	tHeader* inUseList;       // list of all memory currently being used
	tAllocStack mStack;       // stack of where to allocate from

	int numCalls;
	size_t totalBytes;

#else
	std::vector<std::vector<void *>> mStack;
#endif

	TPoolAllocator& operator=(const TPoolAllocator&); // don't allow assignment operator
	TPoolAllocator(const TPoolAllocator&);            // don't allow default copy constructor
};

//
// There could potentially be many pools with pops happening at
// different times. But a simple use is to have a global pop
// with everyone using the same global allocator.
//
extern TPoolAllocator* GetGlobalPoolAllocator();
extern void SetGlobalPoolAllocator(TPoolAllocator* poolAllocator);

//
// This STL compatible allocator is intended to be used as the allocator
// parameter to templatized STL containers, like vector and map.
// It will use the pools for allocation, and not do any deallocation,
// but will still do destruction.
//
template<class T>
class pool_allocator
{
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;

	template<class Other>
	struct rebind
	{
		typedef pool_allocator<Other> other;
	};
	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { return &x; }

	pool_allocator() : allocator(GetGlobalPoolAllocator()) {}
	pool_allocator(TPoolAllocator& a) : allocator(&a) {}
	pool_allocator(const pool_allocator<T>& p) : allocator(p.allocator) {}

	template <class Other>
	pool_allocator<T>& operator=(const pool_allocator<Other>& p)
	{
	  allocator = p.allocator;
	  return *this;
	}

	template<class Other>
	pool_allocator(const pool_allocator<Other>& p) : allocator(&p.getAllocator()) {}

	pointer allocate(size_type n) { return reinterpret_cast<pointer>(getAllocator().allocate(n * sizeof(T))); }
	pointer allocate(size_type n, const void*) { return reinterpret_cast<pointer>(getAllocator().allocate(n * sizeof(T))); }
	void deallocate(pointer, size_type) {}

	void construct(pointer p, const T& val) { new ((void*)p) T(val); }
	void destroy(pointer p) { p->T::~T(); }

	bool operator==(const pool_allocator& rhs) const { return &getAllocator() == &rhs.getAllocator(); }
	bool operator!=(const pool_allocator& rhs) const { return &getAllocator() != &rhs.getAllocator(); }

	size_type max_size() const { return static_cast<size_type>(-1) / sizeof(T); }
	size_type max_size(int size) const { return static_cast<size_type>(-1) / size; }

	void setAllocator(TPoolAllocator *a) { allocator = a; }
	TPoolAllocator& getAllocator() const { return *allocator; }

protected:
	TPoolAllocator *allocator;
};

#endif
