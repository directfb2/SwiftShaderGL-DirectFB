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

#ifndef sw_Debug_hpp
#define sw_Debug_hpp

#include <cassert>

#ifndef DISABLE_DEBUG
namespace sw {

void trace(const char *format, ...);
inline void trace() {}

}
#endif

#ifndef DISABLE_DEBUG
#define TRACE(message, ...) sw::trace("[0x%0.8X]%s(" message ")\n", this, __FUNCTION__, ##__VA_ARGS__)
#else
#define TRACE(message, ...) ((void)0)
#endif

#ifndef DISABLE_DEBUG
#define ASSERT(expression) {if(!(expression)) sw::trace("\t! Assert failed in %s(%d): " #expression "\n", __FUNCTION__, __LINE__); assert(expression);}
#else
#define ASSERT assert
#endif

#ifndef DISABLE_DEBUG
#define UNIMPLEMENTED(...) do { \
	sw::trace("\t! Unimplemented: %s(%d): ", __FUNCTION__, __LINE__); \
	sw::trace(__VA_ARGS__); \
	sw::trace("\n"); \
	assert(false); \
} while(0)
#else
#define UNIMPLEMENTED(...) ((void)0)
#endif

#endif
