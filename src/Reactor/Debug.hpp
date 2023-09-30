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

#ifndef rr_Debug_hpp
#define rr_Debug_hpp

namespace rr {

// Outputs text to stderr.
void warn(const char *format, ...) __attribute__((format(printf, 1, 2)));

// Outputs text to stderr, and calls abort().
void abort(const char *format, ...) __attribute__((format(printf, 1, 2)));

}

#ifndef DISABLE_DEBUG
#define DABORT(message, ...) rr::abort("%s:%d ABORT: " message "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DABORT(message, ...) rr::warn("%s:%d WARNING: " message "\n", __FILE__, __LINE__, ##__VA_ARGS__);
#endif

// A macro asserting a condition.
// If the condition fails, the condition and message are passed to DABORT().
#ifndef DISABLE_DEBUG
#define ASSERT_MSG(expression, format, ...) do { \
	if(!(expression)) { \
		DABORT("ASSERT(%s): " format "\n", #expression, ##__VA_ARGS__); \
	} } while(0)
#else // Silence unused variable warnings without evaluating the expressions.
#define ASSERT_MSG(expression, format, ...) do { \
		(void)sizeof((int)(bool)(expression)); \
		(void)sizeof(format); \
	} while(0)
#endif

// A macro asserting a condition.
// If the condition fails, the condition is passed to DABORT().
#ifndef DISABLE_DEBUG
#define ASSERT(expression) do { \
	if(!(expression)) { \
		DABORT("ASSERT(%s)\n", #expression); \
	} } while(0)
#else // Silence unused variable warnings without evaluating the expressions.
#	define ASSERT(expression) do { \
		(void)sizeof((int)(bool)(expression)); \
	} while(0)
#endif

// A macro to indicate functionality currently unimplemented.
#define UNIMPLEMENTED(format, ...) \
	DABORT("UNIMPLEMENTED: " format, ##__VA_ARGS__);

// A macro for code which should never be reached, even with misbehaving applications.
#define UNREACHABLE(format, ...) \
	DABORT("UNREACHABLE: " format, ##__VA_ARGS__)

#endif
