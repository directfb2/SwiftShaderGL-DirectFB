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

#include "Memory.hpp"

#include "Debug.hpp"

#if defined(ENABLE_NAMED_MMAP)
#include <cstdlib>
#endif
#include <cstring>
#include <unistd.h>

namespace sw {

namespace {

#if !defined(ENABLE_NAMED_MMAP)
struct Allocation
{
	unsigned char *block;
};
#endif

}

size_t memoryPageSize()
{
	static int pageSize = 0;

	if(pageSize == 0)
	{
		pageSize = sysconf(_SC_PAGESIZE);
	}

	return pageSize;
}

void *allocate(size_t bytes, size_t alignment)
{
	void *memory = nullptr;

	ASSERT((alignment & (alignment - 1)) == 0); // Power of 2 alignment.

#if defined(ENABLE_NAMED_MMAP)
	if(alignment < sizeof(void*))
	{
		memory = malloc(bytes);
	}
	else
	{
		posix_memalign(&memory, alignment, bytes);
	}
#else
	unsigned char *block = new unsigned char[bytes + sizeof(unsigned char*) + alignment];
	if(block)
	{
		unsigned char *aligned = (unsigned char*)((uintptr_t)(block + sizeof(Allocation) + alignment - 1) & -(intptr_t)alignment);
		Allocation *allocation = (Allocation*)(aligned - sizeof(Allocation));
		allocation->block = block;
		memory = aligned;
	}
#endif

	if(memory)
	{
		memset(memory, 0, bytes);
	}

	return memory;
}

void deallocate(void *memory)
{
#if defined(ENABLE_NAMED_MMAP)
	free(memory);
#else
	if(memory)
	{
		unsigned char *aligned = (unsigned char*)memory;
		Allocation *allocation = (Allocation*)(aligned - sizeof(Allocation));
		delete[] allocation->block;
	}
#endif
}

void clear(uint16_t *memory, uint16_t element, size_t count)
{
	#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(MEMORY_SANITIZER)
	__asm__ __volatile__("rep stosw" : "+D"(memory), "+c"(count) : "a"(element) : "memory");
	#else
	for(size_t i = 0; i < count; i++)
	{
		memory[i] = element;
	}
	#endif
}

void clear(uint32_t *memory, uint32_t element, size_t count)
{
	#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(MEMORY_SANITIZER)
	__asm__ __volatile__("rep stosl" : "+D"(memory), "+c"(count) : "a"(element) : "memory");
	#else
	for(size_t i = 0; i < count; i++)
	{
		memory[i] = element;
	}
	#endif
}

}
