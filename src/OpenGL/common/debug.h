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

#ifndef DEBUG_H
#define DEBUG_H

#ifndef DISABLE_DEBUG
#include <cassert>

#define TRACE_OUTPUT_FILE "debug.txt"

namespace es {

void trace(const char *format, ...);
inline void trace() {}

}
#endif

// A macro to output a trace of a function call and its arguments to the debugging log.
#ifndef DISABLE_DEBUG
#define TRACE(message, ...) es::trace("trace: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define TRACE(message, ...) (void(0))
#endif

// A macro to output a function call and its arguments to the debugging log, to denote an item in need of fixing.
#ifndef DISABLE_DEBUG
#define FIXME(message, ...) do {es::trace("fixme: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); assert(false);} while(false)
#else
#define FIXME(message, ...) (void(0))
#endif

// A macro to output a function call and its arguments to the debugging log, in case of error.
#ifndef DISABLE_DEBUG
#define ERR(message, ...) do {es::trace("err: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); assert(false);} while(false)
#else
#define ERR(message, ...) (void(0))
#endif

// A macro asserting a condition and outputting failures to the debug log.
#ifndef DISABLE_DEBUG
#define ASSERT(expression) do { \
	if(!(expression)) { \
		ERR("\t! Assert failed in %s(%d): "#expression"\n", __FUNCTION__, __LINE__); \
		assert(expression); \
	} } while(0)
#else
#define ASSERT(expression) (void(0))
#endif

// A macro asserting a condition and outputting failures when in debug mode, or return when in release mode.
#define ASSERT_OR_RETURN(expression) do { \
	if(!(expression)) { \
		ASSERT(expression); \
		return; \
	} } while(0)

// A macro to indicate unimplemented functionality.
#ifndef DISABLE_DEBUG
#define UNIMPLEMENTED(...) do { \
	es::trace("\t! Unimplemented: %s(%d): ", __FUNCTION__, __LINE__); \
	es::trace(__VA_ARGS__); \
	es::trace("\n"); \
	assert(false); \
	} while(0)
#else
#define UNIMPLEMENTED(...) FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro for code which is not expected to be reached under valid assumptions
#ifndef DISABLE_DEBUG
#define UNREACHABLE(value) do { \
	ERR("\t! Unreachable case reached: %s(%d). %s: %d\n", __FUNCTION__, __LINE__, #value, value); \
	assert(false); \
	} while(0)
#else
#define UNREACHABLE(value) ERR("\t! Unreachable reached: %s(%d). %s: %d\n", __FUNCTION__, __LINE__, #value, value)
#endif

#endif
