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

#ifndef sw_Types_hpp
#define sw_Types_hpp

#include <cstdint>
#include <limits>
#include <type_traits>

// GCC warns against bitfields not fitting the entire range of an enum with a fixed underlying type of unsigned int,
// which gets promoted to an error with -Werror and cannot be suppressed.
// However, GCC already defaults to using unsigned int as the underlying type of an unscoped enum
// without a fixed underlying type. So we can just omit it.
#if defined(__GNUC__) && !defined(__clang__)
namespace {enum E {}; static_assert(!std::numeric_limits<std::underlying_type<E>::type>::is_signed, "expected unscoped enum whose underlying type is not fixed to be unsigned");}
#define ENUM_UNDERLYING_TYPE_UNSIGNED_INT
#else
#define ENUM_UNDERLYING_TYPE_UNSIGNED_INT : unsigned int
#endif

#define ALIGN(bytes, type) type __attribute__((aligned(bytes)))

namespace sw {

typedef ALIGN(1, uint8_t) byte;
typedef ALIGN(8, uint8_t) byte8[8];
typedef ALIGN(2, uint16_t) word;
typedef ALIGN(8, uint16_t) word4[4];
typedef ALIGN(16, uint16_t) word8[8];
typedef ALIGN(4, uint32_t) dword;
typedef ALIGN(8, uint32_t) dword2[2];
typedef ALIGN(16, uint32_t) dword4[4];
typedef ALIGN(8, uint64_t) qword;
typedef ALIGN(16, uint64_t) qword2[2];

typedef ALIGN(8, short) short4[4];
typedef ALIGN(16, short) short8[8];
typedef ALIGN(8, int) int2[2];

typedef ALIGN(8, float) float2[2];

ALIGN(16, struct int4
{
	int x;
	int y;
	int z;
	int w;

	int &operator[](int i)
	{
		return (&x)[i];
	}

	const int &operator[](int i) const
	{
		return (&x)[i];
	}

	bool operator!=(const int4 &rhs)
	{
		return x != rhs.x || y != rhs.y || z != rhs.z || w != rhs.w;
	}

	bool operator==(const int4 &rhs)
	{
		return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
	}
});

ALIGN(16, struct float4
{
	float x;
	float y;
	float z;
	float w;

	float &operator[](int i)
	{
		return (&x)[i];
	}

	const float &operator[](int i) const
	{
		return (&x)[i];
	}

	bool operator!=(const float4 &rhs)
	{
		return x != rhs.x || y != rhs.y || z != rhs.z || w != rhs.w;
	}

	bool operator==(const float4 &rhs)
	{
		return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
	}
});

inline float4 vector(float x, float y, float z, float w)
{
	float4 v;

	v.x = x;
	v.y = y;
	v.z = z;
	v.w = w;

	return v;
}

inline float4 replicate(float f)
{
	float4 v;

	v.x = f;
	v.y = f;
	v.z = f;
	v.w = f;

	return v;
}

}

#endif
