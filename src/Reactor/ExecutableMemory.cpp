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

#include "ExecutableMemory.hpp"

#include "Debug.hpp"

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace rr {

namespace {

#if !defined(ENABLE_NAMED_MMAP)
struct Allocation
{
	unsigned char *block;
};
#endif

int permissionsToMmapProt(int permissions)
{
	int result = 0;
	if(permissions & PERMISSION_READ)
	{
		result |= PROT_READ;
	}
	if(permissions & PERMISSION_WRITE)
	{
		result |= PROT_WRITE;
	}
	if(permissions & PERMISSION_EXECUTE)
	{
		result |= PROT_EXEC;
	}
	return result;
}

#if defined(ENABLE_NAMED_MMAP)
int memfd_create(const char *name, unsigned int flags)
{
	#if __aarch64__
	#define __NR_memfd_create 279
	#elif __arm__
	#define __NR_memfd_create 279
	#elif __i386__
	#define __NR_memfd_create 356
	#elif __x86_64__
	#define __NR_memfd_create 319
	#endif
	return syscall(__NR_memfd_create, name, flags);
}

int anonymousFd()
{
	static int fd = memfd_create("SwiftShader JIT", 0);
	return fd;
}

void ensureAnonFileSize(int anonFd, size_t length)
{
	static size_t fileSize = 0;
	if(length > fileSize)
	{
		ftruncate(anonFd, length);
		fileSize = length;
	}
}
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

inline uintptr_t roundUp(uintptr_t x, uintptr_t m)
{
	ASSERT(m > 0 && (m & (m - 1)) == 0); // Power of 2 alignment.
	return (x + m - 1) & ~(m - 1);
}

void *allocateMemoryPages(size_t bytes, int permissions, bool need_exec)
{
	size_t pageSize = memoryPageSize();
	size_t length = roundUp(bytes, pageSize);
	void *mapping = nullptr;

#if defined(ENABLE_NAMED_MMAP)
	int flags = MAP_PRIVATE;

	int anonFd = anonymousFd();
	if(anonFd == -1)
	{
		flags |= MAP_ANONYMOUS;
	}
	else
	{
		ensureAnonFileSize(anonFd, length);
	}

	mapping = mmap(nullptr, length, permissionsToMmapProt(permissions), flags, anonFd, 0);

	if(mapping == MAP_FAILED)
	{
		mapping = nullptr;
	}
#else
	unsigned char *block = new unsigned char[length + sizeof(unsigned char*) + pageSize];
	if(block)
	{
		unsigned char *aligned = (unsigned char*)((uintptr_t)(block + sizeof(unsigned char*) + pageSize - 1) & -(intptr_t)pageSize);
		Allocation *allocation = (Allocation*)(aligned - sizeof(Allocation));
		allocation->block = block;
		mapping = aligned;
	}

	if(mapping)
	{
		memset(mapping, 0, length);
	}

	protectMemoryPages(mapping, length, permissions);
#endif

	return mapping;
}

void protectMemoryPages(void *memory, size_t bytes, int permissions)
{
	if(bytes == 0)
		return;

	bytes = roundUp(bytes, memoryPageSize());

	int result = mprotect(memory, bytes, permissionsToMmapProt(permissions));
	ASSERT(result == 0);
}

void deallocateMemoryPages(void *memory, size_t bytes)
{
#if defined(ENABLE_NAMED_MMAP)
	size_t pageSize = memoryPageSize();
	size_t length = (bytes + pageSize - 1) & ~(pageSize - 1);
	int result = munmap(memory, length);
	ASSERT(result == 0);
#else
	int result = mprotect(memory, bytes, PROT_READ | PROT_WRITE);
	ASSERT(result == 0);
	if(memory)
	{
		unsigned char *aligned = (unsigned char*)memory;
		Allocation *allocation = (Allocation*)(aligned - sizeof(unsigned char*));
		delete[] allocation->block;
	}
#endif
}

}
