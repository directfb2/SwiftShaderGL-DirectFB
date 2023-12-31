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

#include "PoolAlloc.h"

#include "osinclude.h"

#include <cstdint>
#if !defined(DISABLE_POOL_ALLOC)
#ifndef DISABLE_DEBUG
#include <cstdio>
#endif
#else
#include <cstdlib>
#endif

OS_TLSIndex PoolIndex = OS_INVALID_TLS_INDEX;

bool InitializePoolIndex()
{
	assert(PoolIndex == OS_INVALID_TLS_INDEX);

	PoolIndex = OS_AllocTLSIndex();
	return PoolIndex != OS_INVALID_TLS_INDEX;
}

void FreePoolIndex()
{
	assert(PoolIndex != OS_INVALID_TLS_INDEX);

	OS_FreeTLSIndex(PoolIndex);
	PoolIndex = OS_INVALID_TLS_INDEX;
}

TPoolAllocator* GetGlobalPoolAllocator()
{
	assert(PoolIndex != OS_INVALID_TLS_INDEX);
	return static_cast<TPoolAllocator*>(OS_GetTLSValue(PoolIndex));
}

void SetGlobalPoolAllocator(TPoolAllocator* poolAllocator)
{
	assert(PoolIndex != OS_INVALID_TLS_INDEX);
	OS_SetTLSValue(PoolIndex, poolAllocator);
}

TPoolAllocator::TPoolAllocator(int growthIncrement, int allocationAlignment) :
	alignment(allocationAlignment)
#if !defined(DISABLE_POOL_ALLOC)
	, pageSize(growthIncrement),
	freeList(0),
	inUseList(0),
	numCalls(0),
	totalBytes(0)
#endif
{
	size_t minAlign = sizeof(void*);
	alignment &= ~(minAlign - 1);
	if(alignment < minAlign)
	{
		alignment = minAlign;
	}

	size_t a = 1;
	while(a < alignment)
	{
		a <<= 1;
	}
	alignment = a;
	alignmentMask = a - 1;

#if !defined(DISABLE_POOL_ALLOC)
	// Don't allow page sizes we know are smaller than all common OS page sizes.
	if(pageSize < 4 * 1024)
		pageSize = 4 * 1024;

	// A large currentPageOffset indicates that a new page needs to be obtained to allocate memory.
	currentPageOffset = pageSize;

	// Align header skip
	headerSkip = minAlign;
	if(headerSkip < sizeof(tHeader))
	{
		headerSkip = (sizeof(tHeader) + alignmentMask) & ~alignmentMask;
	}
#else
	mStack.push_back({});
#endif
}

TPoolAllocator::~TPoolAllocator()
{
#if !defined(DISABLE_POOL_ALLOC)
	while(inUseList)
	{
		tHeader* next = inUseList->nextPage;
		inUseList->~tHeader();
		delete [] reinterpret_cast<char*>(inUseList);
		inUseList = next;
	}

	// We should not check the guard blocks here, because we did it already
	// when the block was placed into the free list.
	while(freeList)
	{
		tHeader* next = freeList->nextPage;
		delete [] reinterpret_cast<char*>(freeList);
		freeList = next;
	}
#else
	for(auto& allocs : mStack)
	{
		for(auto alloc : allocs)
		{
			free(alloc);
		}
	}

	mStack.clear();
#endif
}

#if !defined(DISABLE_POOL_ALLOC)
const unsigned char TAllocation::guardBlockBeginVal = 0xfb;
const unsigned char TAllocation::guardBlockEndVal   = 0xfe;
const unsigned char TAllocation::userDataFill       = 0xcd;

#ifndef DISABLE_DEBUG
const size_t TAllocation::guardBlockSize = 16;
#else
const size_t TAllocation::guardBlockSize = 0;
#endif

void TAllocation::checkGuardBlock(unsigned char* blockMem, unsigned char val, const char* locText) const
{
	#ifndef DISABLE_DEBUG
	for(size_t x = 0; x < guardBlockSize; x++)
	{
		if(blockMem[x] != val)
		{
			fprintf(stderr, "PoolAlloc: Damage %s %zu byte allocation at 0x%p\n", locText, size, data());
			assert(false);
		}
	}
	#endif
}
#endif

void TPoolAllocator::push()
{
#if !defined(DISABLE_POOL_ALLOC)
	tAllocState state = { currentPageOffset, inUseList };

	mStack.push_back(state);

	// Indicate there is no current page to allocate from.
	currentPageOffset = pageSize;
#else
	mStack.push_back({});
#endif
}

void TPoolAllocator::pop()
{
	if(mStack.size() < 1)
		return;

#if !defined(DISABLE_POOL_ALLOC)
	tHeader* page = mStack.back().page;
	currentPageOffset = mStack.back().offset;

	while(inUseList != page)
	{
		inUseList->~tHeader();

		tHeader* nextInUse = inUseList->nextPage;
		if(inUseList->pageCount > 1)
		{
			delete [] reinterpret_cast<char*>(inUseList);
		}
		else
		{
			inUseList->nextPage = freeList;
			freeList = inUseList;
		}
		inUseList = nextInUse;
	}

	mStack.pop_back();
#else
	for(auto alloc : mStack.back())
	{
		free(alloc);
	}

	mStack.pop_back();
#endif
}

void TPoolAllocator::popAll()
{
	while(mStack.size() > 0)
		pop();
}

void* TPoolAllocator::allocate(size_t numBytes)
{
#if !defined(DISABLE_POOL_ALLOC)
	// Just keep some interesting statistics.
	++numCalls;
	totalBytes += numBytes;

	// allocationSize is the total size including guard blocks.
	size_t allocationSize = TAllocation::allocationSize(numBytes);
	// Detect integer overflow.
	if(allocationSize < numBytes)
		return 0;

	// Do the allocation.
	if(allocationSize <= pageSize - currentPageOffset)
	{
		unsigned char* memory = reinterpret_cast<unsigned char *>(inUseList) + currentPageOffset;
		currentPageOffset += allocationSize;
		currentPageOffset = (currentPageOffset + alignmentMask) & ~alignmentMask;

		return initializeAllocation(inUseList, memory, numBytes);
	}

	if(allocationSize > pageSize - headerSkip)
	{
		// Do a multi-page allocation.
		size_t numBytesToAlloc = allocationSize + headerSkip;
		// Detect integer overflow.
		if(numBytesToAlloc < allocationSize)
			return 0;

		tHeader* memory = reinterpret_cast<tHeader*>(::new char[numBytesToAlloc]);
		if(memory == 0)
			return 0;

		// Use placement-new to initialize header.
		new(memory) tHeader(inUseList, (numBytesToAlloc + pageSize - 1) / pageSize);
		inUseList = memory;

		currentPageOffset = pageSize; // make next allocation come from a new page

		// No guard blocks for multi-page allocations (yet).
		return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(memory) + headerSkip);
	}

	// Need a simple page to allocate from.
	tHeader* memory;
	if(freeList)
	{
		memory = freeList;
		freeList = freeList->nextPage;
	}
	else
	{
		memory = reinterpret_cast<tHeader*>(::new char[pageSize]);
		if(memory == 0)
			return 0;
	}

	// Use placement-new to initialize header.
	new(memory) tHeader(inUseList, 1);
	inUseList = memory;

	unsigned char* ret = reinterpret_cast<unsigned char *>(inUseList) + headerSkip;
	currentPageOffset = (headerSkip + allocationSize + alignmentMask) & ~alignmentMask;

	return initializeAllocation(inUseList, ret, numBytes);
#else
	void *alloc = malloc(numBytes + alignmentMask);
	mStack.back().push_back(alloc);

	intptr_t intAlloc = reinterpret_cast<intptr_t>(alloc);
	intAlloc = (intAlloc + alignmentMask) & ~alignmentMask;
	return reinterpret_cast<void *>(intAlloc);
#endif
}

#if !defined(DISABLE_POOL_ALLOC)
//
// Check all allocations in a list for damage by calling check on each.
//
void TAllocation::checkAllocList() const
{
	for(const TAllocation* alloc = this; alloc != 0; alloc = alloc->prevAlloc)
		alloc->check();
}
#endif
