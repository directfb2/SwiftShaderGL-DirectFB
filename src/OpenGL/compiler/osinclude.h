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

#ifndef _OSINCLUDE_INCLUDED_
#define _OSINCLUDE_INCLUDED_

#include <cassert>
#include <pthread.h>

typedef pthread_key_t OS_TLSIndex;

#define OS_INVALID_TLS_INDEX (static_cast<OS_TLSIndex>(-1))

//
// Thread Local Storage operations
//
OS_TLSIndex OS_AllocTLSIndex();
bool OS_SetTLSValue(OS_TLSIndex nIndex, void *lpvValue);
bool OS_FreeTLSIndex(OS_TLSIndex nIndex);

inline void* OS_GetTLSValue(OS_TLSIndex nIndex)
{
	assert(nIndex != OS_INVALID_TLS_INDEX);

	return pthread_getspecific(nIndex);
}

#endif
