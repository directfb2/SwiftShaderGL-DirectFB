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

#ifndef _COMMON_INCLUDED_
#define _COMMON_INCLUDED_

#include "PoolAlloc.h"

#include <map>
#include <sstream>

struct TSourceLoc
{
	int first_file;
	int first_line;
	int last_file;
	int last_line;
};

//
// Put POOL_ALLOCATOR_NEW_DELETE in base classes to make them use this scheme.
//
#define POOL_ALLOCATOR_NEW_DELETE()                                                \
	void* operator new(size_t s) { return GetGlobalPoolAllocator()->allocate(s); }   \
	void* operator new(size_t, void *_Where) { return (_Where); }                    \
	void operator delete(void*) {}                                                   \
	void operator delete(void *, void *) {}                                          \
	void* operator new[](size_t s) { return GetGlobalPoolAllocator()->allocate(s); } \
	void* operator new[](size_t, void *_Where) { return (_Where); }                  \
	void operator delete[](void*) {}                                                 \
	void operator delete[](void *, void *) {}

//
// Pool version of string.
//
typedef pool_allocator<char> TStringAllocator;
typedef std::basic_string <char, std::char_traits<char>, TStringAllocator> TString;
typedef std::basic_ostringstream<char, std::char_traits<char>, TStringAllocator> TStringStream;
inline TString* NewPoolTString(const char* s)
{
	void* memory = GetGlobalPoolAllocator()->allocate(sizeof(TString));
	return new(memory) TString(s);
}

//
// Persistent string memory. Should only be used for strings that survive
// across compilations.
//
#define TPersistString std::string
#define TPersistStringStream std::ostringstream

//
// Pool allocator versions of vectors and maps.
//
template <class T> class TVector : public std::vector<T, pool_allocator<T> > {
public:
	typedef typename std::vector<T, pool_allocator<T> >::size_type size_type;
	TVector() : std::vector<T, pool_allocator<T> >() {}
	TVector(const pool_allocator<T>& a) : std::vector<T, pool_allocator<T> >(a) {}
	TVector(size_type i): std::vector<T, pool_allocator<T> >(i) {}
};

template <class K, class D, class CMP = std::less<K> >
class TMap : public std::map<K, D, CMP, pool_allocator<std::pair<const K, D> > >
{
public:
	typedef pool_allocator<std::pair<const K, D> > tAllocator;
	TMap() : std::map<K, D, CMP, tAllocator>() {}
	TMap(const tAllocator& a) : std::map<K, D, CMP, tAllocator>(std::map<K, D, CMP, tAllocator>::key_compare(), a) {}
};

#endif
