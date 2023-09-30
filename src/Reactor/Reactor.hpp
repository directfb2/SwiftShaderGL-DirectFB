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

#ifndef rr_Reactor_hpp
#define rr_Reactor_hpp

#include "Nucleus.hpp"
#include "Traits.hpp"

#include <unordered_set>

#ifdef ENABLE_RR_DEBUG_INFO
// Functions used for generating JIT debug info.
namespace rr {

// Update the current source location for debug.
void EmitDebugLocation();
// Bind value to its symbolic name taken from the backtrace.
void EmitDebugVariable(class Value *value);
// Flush any pending variable bindings before the line ends.
void FlushDebug();

}

#define RR_DEBUG_INFO_UPDATE_LOC() EmitDebugLocation()
#define RR_DEBUG_INFO_EMIT_VAR(value) EmitDebugVariable(value)
#define RR_DEBUG_INFO_FLUSH() FlushDebug()
#else
#define RR_DEBUG_INFO_UPDATE_LOC()
#define RR_DEBUG_INFO_EMIT_VAR(value)
#define RR_DEBUG_INFO_FLUSH()
#endif

#ifdef ENABLE_RR_PRINT
namespace rr {

int DebugPrintf(const char *format, ...);

}
#endif

namespace rr {

std::string BackendName();

class Bool;
class Byte8;
class Float;
class Float4;
class Int2;
class Int4;
class Long;
class Short4;
class UInt;
class UInt4;
class UShort4;

template<class T>
class RValue;

class Void
{
public:
	static Type *type();
};

class Variable
{
	friend class Nucleus;

	Variable &operator=(const Variable &) = delete;

public:
	void materialize() const;

	Value *loadValue() const;
	Value *storeValue(Value *value) const;

	Value *getBaseAddress() const;
	Value *getElementPointer(Value *index, bool unsignedIndex) const;

	virtual Type *getType() const = 0;

protected:
	Variable();
	Variable(const Variable &) = default;

	virtual ~Variable();

private:
	static void materializeAll();
	static void killUnmaterialized();

	virtual Value *allocate() const;

	static thread_local std::unordered_set<const Variable *> *unmaterializedVariables;

	mutable Value *rvalue = nullptr;
	mutable Value *address = nullptr;
};

template<class T>
class LValue : public Variable
{
public:
	LValue();

	RValue<Pointer<T>> operator&();

	RValue<T> load() const
	{
		return RValue<T>(this->loadValue());
	}

	RValue<T> store(RValue<T> rvalue) const
	{
		this->storeValue(rvalue.value());

		return rvalue;
	}

	Type *getType() const override
	{
		return T::type();
	}

	// self() returns the this pointer to this LValue<T> object.
	// This function exists because operator&() is overloaded.
	inline LValue<T> *self() { return this; }
};

template<class T>
class Reference
{
public:
	using reference_underlying_type = T;

	explicit Reference(Value *pointer, int alignment = 1);

	RValue<T> operator=(RValue<T> rhs) const;
	RValue<T> operator=(const Reference<T> &ref) const;
	RValue<T> operator+=(RValue<T> rhs) const;
	RValue<Pointer<T>> operator&() const { return RValue<Pointer<T>>(address); }

	Value *loadValue() const;
	RValue<T> load() const;
	int getAlignment() const;

private:
	Value *address;

	const int alignment;
};

template<class T>
struct BoolLiteral
{
	struct type;
};

template<>
struct BoolLiteral<Bool>
{
	typedef bool type;
};

template<class T>
struct IntLiteral
{
	struct type;
};

template<>
struct IntLiteral<Int>
{
	typedef int type;
};

template<>
struct IntLiteral<UInt>
{
	typedef unsigned int type;
};

template<>
struct IntLiteral<Long>
{
	typedef int64_t type;
};

template<class T>
struct FloatLiteral
{
	struct type;
};

template<>
struct FloatLiteral<Float>
{
	typedef float type;
};

template<class T>
class RValue
{
public:
	using rvalue_underlying_type = T;

	explicit RValue(Value *rvalue);

	RValue(const RValue<T> &rvalue);
	RValue(const T &lvalue);
	RValue(typename BoolLiteral<T>::type i);
	RValue(typename IntLiteral<T>::type i);
	RValue(typename FloatLiteral<T>::type f);
	RValue(const Reference<T> &rhs);

	// Rvalues cannot be assigned to: "(a + b) = c;"
	RValue<T> &operator=(const RValue<T> &) = delete;

	Value *value() const { return val; }

private:
	Value *const val;
};

template<typename T>
class Argument
{
public:
	explicit Argument(Value *val) : val(val) {}

	RValue<T> rvalue() const { return RValue<T>(val); }

private:
	Value *const val;
};

class Bool : public LValue<Bool>
{
public:
	Bool() = default;
	Bool(bool x);
	Bool(RValue<Bool> rhs);

	RValue<Bool> operator=(RValue<Bool> rhs);
	RValue<Bool> operator=(const Bool &rhs);

	static Type *type();
};

RValue<Bool> operator!(RValue<Bool> val);
RValue<Bool> operator&&(RValue<Bool> lhs, RValue<Bool> rhs);
RValue<Bool> operator||(RValue<Bool> lhs, RValue<Bool> rhs);

class Byte : public LValue<Byte>
{
public:
	explicit Byte(RValue<Int> cast);

	Byte() = default;
	Byte(int x);

	static Type *type();
};

RValue<Bool> operator!=(RValue<Byte> lhs, RValue<Byte> rhs);

class SByte : public LValue<SByte>
{
public:
	explicit SByte(RValue<Int> cast);

	SByte() = default;
	SByte(signed char x);

	static Type *type();
};

class Short : public LValue<Short>
{
public:
	explicit Short(RValue<Int> cast);

	Short() = default;
	Short(short x);

	static Type *type();
};

RValue<Bool> operator==(RValue<Short> lhs, RValue<Short> rhs);

class UShort : public LValue<UShort>
{
public:
	explicit UShort(RValue<UInt> cast);
	explicit UShort(RValue<Int> cast);

	UShort() = default;
	UShort(unsigned short x);
	UShort(const Reference<UShort> &rhs);

	RValue<UShort> operator=(RValue<UShort> rhs);

	static Type *type();
};

RValue<UShort> operator&(RValue<UShort> lhs, RValue<UShort> rhs);
RValue<UShort> operator|(RValue<UShort> lhs, RValue<UShort> rhs);
RValue<UShort> operator>>(RValue<UShort> lhs, RValue<UShort> rhs);

class Byte4 : public LValue<Byte4>
{
public:
	explicit Byte4(RValue<Byte8> cast);

	Byte4() = default;
	Byte4(const Reference<Byte4> &rhs);

	static Type *type();
};

class SByte4 : public LValue<SByte4>
{
public:
	SByte4() = default;

	static Type *type();
};

class Byte8 : public LValue<Byte8>
{
public:
	Byte8() = default;
	Byte8(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6, uint8_t x7);
	Byte8(const Byte8 &rhs);
	Byte8(const Reference<Byte8> &rhs);

	RValue<Byte8> operator=(RValue<Byte8> rhs);
	RValue<Byte8> operator=(const Byte8 &rhs);
	RValue<Byte8> operator=(const Reference<Byte8> &rhs);

	static Type *type();
};

RValue<Byte8> operator+(RValue<Byte8> lhs, RValue<Byte8> rhs);
RValue<Byte8> operator-(RValue<Byte8> lhs, RValue<Byte8> rhs);
RValue<Byte8> operator&(RValue<Byte8> lhs, RValue<Byte8> rhs);
RValue<Byte8> operator|(RValue<Byte8> lhs, RValue<Byte8> rhs);
RValue<Byte8> operator^(RValue<Byte8> lhs, RValue<Byte8> rhs);
RValue<Byte8> operator+=(Byte8 &lhs, RValue<Byte8> rhs);
RValue<Byte8> operator&=(Byte8 &lhs, RValue<Byte8> rhs);
RValue<Byte8> operator|=(Byte8 &lhs, RValue<Byte8> rhs);
RValue<Byte8> operator^=(Byte8 &lhs, RValue<Byte8> rhs);

RValue<Byte8> AddSat(RValue<Byte8> x, RValue<Byte8> y);
RValue<Byte8> SubSat(RValue<Byte8> x, RValue<Byte8> y);
RValue<Short4> Unpack(RValue<Byte4> x);
RValue<Short4> Unpack(RValue<Byte4> x, RValue<Byte4> y);
RValue<Short4> UnpackLow(RValue<Byte8> x, RValue<Byte8> y);
RValue<Short4> UnpackHigh(RValue<Byte8> x, RValue<Byte8> y);
RValue<Int> SignMask(RValue<Byte8> x);
RValue<Byte8> CmpEQ(RValue<Byte8> x, RValue<Byte8> y);

class SByte8 : public LValue<SByte8>
{
public:
	SByte8() = default;

	static Type *type();
};

RValue<Int> SignMask(RValue<SByte8> x);
RValue<Byte8> CmpGT(RValue<SByte8> x, RValue<SByte8> y);

class Byte16 : public LValue<Byte16>
{
public:
	Byte16() = default;

	static Type *type();
};

class SByte16 : public LValue<SByte16>
{
public:
	SByte16() = default;

	static Type *type();
};

class Short2 : public LValue<Short2>
{
public:
	explicit Short2(RValue<Short4> cast);

	static Type *type();
};

class UShort2 : public LValue<UShort2>
{
public:
	explicit UShort2(RValue<UShort4> cast);

	static Type *type();
};

class Short4 : public LValue<Short4>
{
public:
	explicit Short4(RValue<Int> cast);
	explicit Short4(RValue<Int4> cast);

	Short4() = default;
	Short4(short xyzw);
	Short4(short x, short y, short z, short w);
	Short4(RValue<Short4> rhs);
	Short4(const Short4 &rhs);
	Short4(const Reference<Short4> &rhs);
	Short4(RValue<UShort4> rhs);

	RValue<Short4> operator=(RValue<Short4> rhs);
	RValue<Short4> operator=(const Short4 &rhs);
	RValue<Short4> operator=(const Reference<Short4> &rhs);
	RValue<Short4> operator=(RValue<UShort4> rhs);
	RValue<Short4> operator=(const UShort4 &rhs);

	static Type *type();
};

RValue<Short4> operator+(RValue<Short4> lhs, RValue<Short4> rhs);
RValue<Short4> operator-(RValue<Short4> lhs, RValue<Short4> rhs);
RValue<Short4> operator*(RValue<Short4> lhs, RValue<Short4> rhs);
RValue<Short4> operator&(RValue<Short4> lhs, RValue<Short4> rhs);
RValue<Short4> operator|(RValue<Short4> lhs, RValue<Short4> rhs);
RValue<Short4> operator^(RValue<Short4> lhs, RValue<Short4> rhs);
RValue<Short4> operator<<(RValue<Short4> lhs, unsigned char rhs);
RValue<Short4> operator>>(RValue<Short4> lhs, unsigned char rhs);
RValue<Short4> operator+=(Short4 &lhs, RValue<Short4> rhs);
RValue<Short4> operator&=(Short4 &lhs, RValue<Short4> rhs);
RValue<Short4> operator|=(Short4 &lhs, RValue<Short4> rhs);
RValue<Short4> operator<<=(Short4 &lhs, unsigned char rhs);
RValue<Short4> operator>>=(Short4 &lhs, unsigned char rhs);
RValue<Short4> operator-(RValue<Short4> val);
RValue<Short4> operator~(RValue<Short4> val);

RValue<Short4> RoundShort4(RValue<Float4> cast);
RValue<Short4> Max(RValue<Short4> x, RValue<Short4> y);
RValue<Short4> Min(RValue<Short4> x, RValue<Short4> y);
RValue<Short4> AddSat(RValue<Short4> x, RValue<Short4> y);
RValue<Short4> SubSat(RValue<Short4> x, RValue<Short4> y);
RValue<Short4> MulHigh(RValue<Short4> x, RValue<Short4> y);
RValue<Int2> MulAdd(RValue<Short4> x, RValue<Short4> y);
RValue<SByte8> PackSigned(RValue<Short4> x, RValue<Short4> y);
RValue<Byte8> PackUnsigned(RValue<Short4> x, RValue<Short4> y);
RValue<Int2> UnpackLow(RValue<Short4> x, RValue<Short4> y);
RValue<Int2> UnpackHigh(RValue<Short4> x, RValue<Short4> y);
RValue<Short4> Swizzle(RValue<Short4> x, uint16_t select);
RValue<Short4> Insert(RValue<Short4> val, RValue<Short> element, int i);
RValue<Short> Extract(RValue<Short4> val, int i);
RValue<Short4> CmpGT(RValue<Short4> x, RValue<Short4> y);
RValue<Short4> CmpEQ(RValue<Short4> x, RValue<Short4> y);

class UShort4 : public LValue<UShort4>
{
public:
	explicit UShort4(RValue<Int4> cast);
	explicit UShort4(RValue<Float4> cast, bool saturate = false);

	UShort4() = default;
	UShort4(unsigned short xyzw);
	UShort4(RValue<UShort4> rhs);
	UShort4(const UShort4 &rhs);
	UShort4(const Reference<UShort4> &rhs);
	UShort4(RValue<Short4> rhs);

	RValue<UShort4> operator=(RValue<UShort4> rhs);
	RValue<UShort4> operator=(const UShort4 &rhs);
	RValue<UShort4> operator=(const Reference<UShort4> &rhs);
	RValue<UShort4> operator=(RValue<Short4> rhs);
	RValue<UShort4> operator=(const Short4 &rhs);

	static Type *type();
};

RValue<UShort4> operator+(RValue<UShort4> lhs, RValue<UShort4> rhs);
RValue<UShort4> operator-(RValue<UShort4> lhs, RValue<UShort4> rhs);
RValue<UShort4> operator*(RValue<UShort4> lhs, RValue<UShort4> rhs);
RValue<UShort4> operator<<(RValue<UShort4> lhs, unsigned char rhs);
RValue<UShort4> operator>>(RValue<UShort4> lhs, unsigned char rhs);
RValue<UShort4> operator~(RValue<UShort4> val);

RValue<UShort4> Max(RValue<UShort4> x, RValue<UShort4> y);
RValue<UShort4> Min(RValue<UShort4> x, RValue<UShort4> y);
RValue<UShort4> AddSat(RValue<UShort4> x, RValue<UShort4> y);
RValue<UShort4> SubSat(RValue<UShort4> x, RValue<UShort4> y);
RValue<UShort4> MulHigh(RValue<UShort4> x, RValue<UShort4> y);

class Short8 : public LValue<Short8>
{
public:
	Short8() = default;
	Short8(short c);
	Short8(short c0, short c1, short c2, short c3, short c4, short c5, short c6, short c7);

	static Type *type();
};

RValue<Short8> operator+(RValue<Short8> lhs, RValue<Short8> rhs);

RValue<Int4> Abs(RValue<Int4> x);

class UShort8 : public LValue<UShort8>
{
public:
	UShort8() = default;
	UShort8(const Reference<UShort8> &rhs);
	UShort8(RValue<UShort4> lo, RValue<UShort4> hi);

	RValue<UShort8> operator=(const UShort8 &rhs);
	RValue<UShort8> operator=(const Reference<UShort8> &rhs);

	static Type *type();
};

class Int : public LValue<Int>
{
public:
	Int(Argument<Int> argument);

	explicit Int(RValue<Byte> cast);
	explicit Int(RValue<SByte> cast);
	explicit Int(RValue<Short> cast);
	explicit Int(RValue<UShort> cast);
	explicit Int(RValue<Int2> cast);
	explicit Int(RValue<Float> cast);

	Int() = default;
	Int(int x);
	Int(RValue<Int> rhs);
	Int(RValue<UInt> rhs);
	Int(const Int &rhs);
	Int(const UInt &rhs);
	Int(const Reference<Int> &rhs);
	Int(const Reference<UInt> &rhs);

	RValue<Int> operator=(int rhs);
	RValue<Int> operator=(RValue<Int> rhs);
	RValue<Int> operator=(const Int &rhs);
	RValue<Int> operator=(const Reference<Int> &rhs);

	static Type *type();
};

RValue<Int> operator+(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator-(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator*(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator/(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator%(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator&(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator|(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator^(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator<<(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator>>(RValue<Int> lhs, RValue<Int> rhs);
RValue<Int> operator+=(Int &lhs, RValue<Int> rhs);
RValue<Int> operator-=(Int &lhs, RValue<Int> rhs);
RValue<Int> operator*=(Int &lhs, RValue<Int> rhs);
RValue<Int> operator&=(Int &lhs, RValue<Int> rhs);
RValue<Int> operator|=(Int &lhs, RValue<Int> rhs);
RValue<Int> operator<<=(Int &lhs, RValue<Int> rhs);
RValue<Int> operator-(RValue<Int> val);
RValue<Int> operator~(RValue<Int> val);
RValue<Int> operator++(Int &val, int); // Post-increment
RValue<Int> operator--(Int &val, int); // Post-decrement
const Int &operator--(Int &val);       // Pre-decrement
RValue<Bool> operator<(RValue<Int> lhs, RValue<Int> rhs);
RValue<Bool> operator>(RValue<Int> lhs, RValue<Int> rhs);
RValue<Bool> operator>=(RValue<Int> lhs, RValue<Int> rhs);
RValue<Bool> operator!=(RValue<Int> lhs, RValue<Int> rhs);
RValue<Bool> operator==(RValue<Int> lhs, RValue<Int> rhs);

RValue<Int> Max(RValue<Int> x, RValue<Int> y);
RValue<Int> Min(RValue<Int> x, RValue<Int> y);
RValue<Int> Clamp(RValue<Int> x, RValue<Int> min, RValue<Int> max);
RValue<Int> RoundInt(RValue<Float> cast);

class Long : public LValue<Long>
{
public:
	Long() = default;
	Long(RValue<Long> rhs);

	RValue<Long> operator=(int64_t rhs);
	RValue<Long> operator=(RValue<Long> rhs);

	static Type *type();
};

RValue<Long> operator+(RValue<Long> lhs, RValue<Long> rhs);
RValue<Long> operator-(RValue<Long> lhs, RValue<Long> rhs);
RValue<Long> operator+=(Long &lhs, RValue<Long> rhs);

class UInt : public LValue<UInt>
{
public:
	explicit UInt(RValue<UShort> cast);

	UInt() = default;
	UInt(int x);
	UInt(unsigned int x);
	UInt(RValue<UInt> rhs);
	UInt(RValue<Int> rhs);
	UInt(const UInt &rhs);
	UInt(const Reference<UInt> &rhs);

	RValue<UInt> operator=(unsigned int rhs);
	RValue<UInt> operator=(RValue<UInt> rhs);
	RValue<UInt> operator=(RValue<Int> rhs);

	static Type *type();
};

RValue<UInt> operator+(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<UInt> operator*(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<UInt> operator&(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<UInt> operator|(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<UInt> operator<<(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<UInt> operator>>(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<UInt> operator+=(UInt &lhs, RValue<UInt> rhs);
RValue<UInt> operator|=(UInt &lhs, RValue<UInt> rhs);
RValue<UInt> operator-(RValue<UInt> val);
RValue<UInt> operator++(UInt &val, int); // Post-increment
RValue<UInt> operator--(UInt &val, int); // Post-decrement
RValue<Bool> operator<(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<Bool> operator>(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<Bool> operator!=(RValue<UInt> lhs, RValue<UInt> rhs);
RValue<Bool> operator==(RValue<UInt> lhs, RValue<UInt> rhs);

RValue<UInt> Max(RValue<UInt> x, RValue<UInt> y);
RValue<UInt> Min(RValue<UInt> x, RValue<UInt> y);

class Int2 : public LValue<Int2>
{
public:
	explicit Int2(RValue<Int4> cast);

	Int2() = default;
	Int2(RValue<Int2> rhs);
	Int2(RValue<Int> lo, RValue<Int> hi);

	RValue<Int2> operator=(RValue<Int2> rhs);

	static Type *type();
};

RValue<Short4> UnpackLow(RValue<Int2> x, RValue<Int2> y);
RValue<Short4> UnpackHigh(RValue<Int2> x, RValue<Int2> y);
RValue<Int> Extract(RValue<Int2> val, int i);
RValue<Int2> Insert(RValue<Int2> val, RValue<Int> element, int i);

class UInt2 : public LValue<UInt2>
{
public:
	UInt2() = default;
	UInt2(RValue<UInt2> rhs);
	UInt2(const Reference<UInt2> &rhs);

	RValue<UInt2> operator=(RValue<UInt2> rhs);
	RValue<UInt2> operator=(const Reference<UInt2> &rhs);

	static Type *type();
};

RValue<UInt2> operator&(RValue<UInt2> lhs, RValue<UInt2> rhs);
RValue<UInt2> operator|(RValue<UInt2> lhs, RValue<UInt2> rhs);
RValue<UInt2> operator&=(UInt2 &lhs, RValue<UInt2> rhs);
RValue<UInt2> operator~(RValue<UInt2> val);

template<class T>
struct Scalar;

template<class Vector4>
struct XYZW;

template<class Vector4, int T>
class Swizzle2
{
	friend Vector4;

public:
	operator RValue<Vector4>() const;

private:
	Vector4 *parent;
};

template<class Vector4, int T>
class Swizzle4
{
public:
	operator RValue<Vector4>() const;

private:
	Vector4 *parent;
};

template<class Vector4, int T>
class SwizzleMask4
{
	friend XYZW<Vector4>;

public:
	operator RValue<Vector4>() const;

	RValue<Vector4> operator=(RValue<Vector4> rhs);
	RValue<Vector4> operator=(RValue<typename Scalar<Vector4>::Type> rhs);

private:
	Vector4 *parent;
};

template<>
struct Scalar<Float4>
{
	using Type = Float;
};

template<>
struct Scalar<Int4>
{
	using Type = Int;
};

template<>
struct Scalar<UInt4>
{
	using Type = UInt;
};

template<class Vector4, int T>
class SwizzleMask1
{
public:
	operator RValue<typename Scalar<Vector4>::Type>() const;
	operator RValue<Vector4>() const;

	RValue<Vector4> operator=(float x);
	RValue<Vector4> operator=(RValue<Vector4> rhs);
	RValue<Vector4> operator=(RValue<typename Scalar<Vector4>::Type> rhs);

private:
	Vector4 *parent;
};

template<class Vector4, int T>
class SwizzleMask2
{
	friend class Float4;

public:
	operator RValue<Vector4>() const;

	RValue<Vector4> operator=(RValue<Vector4> rhs);

private:
	Float4 *parent;
};

template<class Vector4>
struct XYZW
{
	friend Vector4;

private:
	XYZW(Vector4 *parent)
	{
		xyzw.parent = parent;
	}

public:
	union
	{
		SwizzleMask1<Vector4, 0x0000> x;
		SwizzleMask1<Vector4, 0x1111> y;
		SwizzleMask1<Vector4, 0x2222> z;
		SwizzleMask1<Vector4, 0x3333> w;
		Swizzle2<Vector4, 0x0000> xx;
		Swizzle2<Vector4, 0x1000> yx;
		Swizzle2<Vector4, 0x2000> zx;
		Swizzle2<Vector4, 0x3000> wx;
		SwizzleMask2<Vector4, 0x0111> xy;
		Swizzle2<Vector4, 0x1111> yy;
		Swizzle2<Vector4, 0x2111> zy;
		Swizzle2<Vector4, 0x3111> wy;
		SwizzleMask2<Vector4, 0x0222> xz;
		SwizzleMask2<Vector4, 0x1222> yz;
		Swizzle2<Vector4, 0x2222> zz;
		Swizzle2<Vector4, 0x3222> wz;
		SwizzleMask2<Vector4, 0x0333> xw;
		SwizzleMask2<Vector4, 0x1333> yw;
		SwizzleMask2<Vector4, 0x2333> zw;
		Swizzle2<Vector4, 0x3333> ww;
		Swizzle4<Vector4, 0x0000> xxx;
		Swizzle4<Vector4, 0x1000> yxx;
		Swizzle4<Vector4, 0x2000> zxx;
		Swizzle4<Vector4, 0x3000> wxx;
		Swizzle4<Vector4, 0x0100> xyx;
		Swizzle4<Vector4, 0x1100> yyx;
		Swizzle4<Vector4, 0x2100> zyx;
		Swizzle4<Vector4, 0x3100> wyx;
		Swizzle4<Vector4, 0x0200> xzx;
		Swizzle4<Vector4, 0x1200> yzx;
		Swizzle4<Vector4, 0x2200> zzx;
		Swizzle4<Vector4, 0x3200> wzx;
		Swizzle4<Vector4, 0x0300> xwx;
		Swizzle4<Vector4, 0x1300> ywx;
		Swizzle4<Vector4, 0x2300> zwx;
		Swizzle4<Vector4, 0x3300> wwx;
		Swizzle4<Vector4, 0x0011> xxy;
		Swizzle4<Vector4, 0x1011> yxy;
		Swizzle4<Vector4, 0x2011> zxy;
		Swizzle4<Vector4, 0x3011> wxy;
		Swizzle4<Vector4, 0x0111> xyy;
		Swizzle4<Vector4, 0x1111> yyy;
		Swizzle4<Vector4, 0x2111> zyy;
		Swizzle4<Vector4, 0x3111> wyy;
		Swizzle4<Vector4, 0x0211> xzy;
		Swizzle4<Vector4, 0x1211> yzy;
		Swizzle4<Vector4, 0x2211> zzy;
		Swizzle4<Vector4, 0x3211> wzy;
		Swizzle4<Vector4, 0x0311> xwy;
		Swizzle4<Vector4, 0x1311> ywy;
		Swizzle4<Vector4, 0x2311> zwy;
		Swizzle4<Vector4, 0x3311> wwy;
		Swizzle4<Vector4, 0x0022> xxz;
		Swizzle4<Vector4, 0x1022> yxz;
		Swizzle4<Vector4, 0x2022> zxz;
		Swizzle4<Vector4, 0x3022> wxz;
		SwizzleMask4<Vector4, 0x0122> xyz;
		Swizzle4<Vector4, 0x1122> yyz;
		Swizzle4<Vector4, 0x2122> zyz;
		Swizzle4<Vector4, 0x3122> wyz;
		Swizzle4<Vector4, 0x0222> xzz;
		Swizzle4<Vector4, 0x1222> yzz;
		Swizzle4<Vector4, 0x2222> zzz;
		Swizzle4<Vector4, 0x3222> wzz;
		Swizzle4<Vector4, 0x0322> xwz;
		Swizzle4<Vector4, 0x1322> ywz;
		Swizzle4<Vector4, 0x2322> zwz;
		Swizzle4<Vector4, 0x3322> wwz;
		Swizzle4<Vector4, 0x0033> xxw;
		Swizzle4<Vector4, 0x1033> yxw;
		Swizzle4<Vector4, 0x2033> zxw;
		Swizzle4<Vector4, 0x3033> wxw;
		SwizzleMask4<Vector4, 0x0133> xyw;
		Swizzle4<Vector4, 0x1133> yyw;
		Swizzle4<Vector4, 0x2133> zyw;
		Swizzle4<Vector4, 0x3133> wyw;
		SwizzleMask4<Vector4, 0x0233> xzw;
		SwizzleMask4<Vector4, 0x1233> yzw;
		Swizzle4<Vector4, 0x2233> zzw;
		Swizzle4<Vector4, 0x3233> wzw;
		Swizzle4<Vector4, 0x0333> xww;
		Swizzle4<Vector4, 0x1333> yww;
		Swizzle4<Vector4, 0x2333> zww;
		Swizzle4<Vector4, 0x3333> www;
		Swizzle4<Vector4, 0x0000> xxxx;
		Swizzle4<Vector4, 0x1000> yxxx;
		Swizzle4<Vector4, 0x2000> zxxx;
		Swizzle4<Vector4, 0x3000> wxxx;
		Swizzle4<Vector4, 0x0100> xyxx;
		Swizzle4<Vector4, 0x1100> yyxx;
		Swizzle4<Vector4, 0x2100> zyxx;
		Swizzle4<Vector4, 0x3100> wyxx;
		Swizzle4<Vector4, 0x0200> xzxx;
		Swizzle4<Vector4, 0x1200> yzxx;
		Swizzle4<Vector4, 0x2200> zzxx;
		Swizzle4<Vector4, 0x3200> wzxx;
		Swizzle4<Vector4, 0x0300> xwxx;
		Swizzle4<Vector4, 0x1300> ywxx;
		Swizzle4<Vector4, 0x2300> zwxx;
		Swizzle4<Vector4, 0x3300> wwxx;
		Swizzle4<Vector4, 0x0010> xxyx;
		Swizzle4<Vector4, 0x1010> yxyx;
		Swizzle4<Vector4, 0x2010> zxyx;
		Swizzle4<Vector4, 0x3010> wxyx;
		Swizzle4<Vector4, 0x0110> xyyx;
		Swizzle4<Vector4, 0x1110> yyyx;
		Swizzle4<Vector4, 0x2110> zyyx;
		Swizzle4<Vector4, 0x3110> wyyx;
		Swizzle4<Vector4, 0x0210> xzyx;
		Swizzle4<Vector4, 0x1210> yzyx;
		Swizzle4<Vector4, 0x2210> zzyx;
		Swizzle4<Vector4, 0x3210> wzyx;
		Swizzle4<Vector4, 0x0310> xwyx;
		Swizzle4<Vector4, 0x1310> ywyx;
		Swizzle4<Vector4, 0x2310> zwyx;
		Swizzle4<Vector4, 0x3310> wwyx;
		Swizzle4<Vector4, 0x0020> xxzx;
		Swizzle4<Vector4, 0x1020> yxzx;
		Swizzle4<Vector4, 0x2020> zxzx;
		Swizzle4<Vector4, 0x3020> wxzx;
		Swizzle4<Vector4, 0x0120> xyzx;
		Swizzle4<Vector4, 0x1120> yyzx;
		Swizzle4<Vector4, 0x2120> zyzx;
		Swizzle4<Vector4, 0x3120> wyzx;
		Swizzle4<Vector4, 0x0220> xzzx;
		Swizzle4<Vector4, 0x1220> yzzx;
		Swizzle4<Vector4, 0x2220> zzzx;
		Swizzle4<Vector4, 0x3220> wzzx;
		Swizzle4<Vector4, 0x0320> xwzx;
		Swizzle4<Vector4, 0x1320> ywzx;
		Swizzle4<Vector4, 0x2320> zwzx;
		Swizzle4<Vector4, 0x3320> wwzx;
		Swizzle4<Vector4, 0x0030> xxwx;
		Swizzle4<Vector4, 0x1030> yxwx;
		Swizzle4<Vector4, 0x2030> zxwx;
		Swizzle4<Vector4, 0x3030> wxwx;
		Swizzle4<Vector4, 0x0130> xywx;
		Swizzle4<Vector4, 0x1130> yywx;
		Swizzle4<Vector4, 0x2130> zywx;
		Swizzle4<Vector4, 0x3130> wywx;
		Swizzle4<Vector4, 0x0230> xzwx;
		Swizzle4<Vector4, 0x1230> yzwx;
		Swizzle4<Vector4, 0x2230> zzwx;
		Swizzle4<Vector4, 0x3230> wzwx;
		Swizzle4<Vector4, 0x0330> xwwx;
		Swizzle4<Vector4, 0x1330> ywwx;
		Swizzle4<Vector4, 0x2330> zwwx;
		Swizzle4<Vector4, 0x3330> wwwx;
		Swizzle4<Vector4, 0x0001> xxxy;
		Swizzle4<Vector4, 0x1001> yxxy;
		Swizzle4<Vector4, 0x2001> zxxy;
		Swizzle4<Vector4, 0x3001> wxxy;
		Swizzle4<Vector4, 0x0101> xyxy;
		Swizzle4<Vector4, 0x1101> yyxy;
		Swizzle4<Vector4, 0x2101> zyxy;
		Swizzle4<Vector4, 0x3101> wyxy;
		Swizzle4<Vector4, 0x0201> xzxy;
		Swizzle4<Vector4, 0x1201> yzxy;
		Swizzle4<Vector4, 0x2201> zzxy;
		Swizzle4<Vector4, 0x3201> wzxy;
		Swizzle4<Vector4, 0x0301> xwxy;
		Swizzle4<Vector4, 0x1301> ywxy;
		Swizzle4<Vector4, 0x2301> zwxy;
		Swizzle4<Vector4, 0x3301> wwxy;
		Swizzle4<Vector4, 0x0011> xxyy;
		Swizzle4<Vector4, 0x1011> yxyy;
		Swizzle4<Vector4, 0x2011> zxyy;
		Swizzle4<Vector4, 0x3011> wxyy;
		Swizzle4<Vector4, 0x0111> xyyy;
		Swizzle4<Vector4, 0x1111> yyyy;
		Swizzle4<Vector4, 0x2111> zyyy;
		Swizzle4<Vector4, 0x3111> wyyy;
		Swizzle4<Vector4, 0x0211> xzyy;
		Swizzle4<Vector4, 0x1211> yzyy;
		Swizzle4<Vector4, 0x2211> zzyy;
		Swizzle4<Vector4, 0x3211> wzyy;
		Swizzle4<Vector4, 0x0311> xwyy;
		Swizzle4<Vector4, 0x1311> ywyy;
		Swizzle4<Vector4, 0x2311> zwyy;
		Swizzle4<Vector4, 0x3311> wwyy;
		Swizzle4<Vector4, 0x0021> xxzy;
		Swizzle4<Vector4, 0x1021> yxzy;
		Swizzle4<Vector4, 0x2021> zxzy;
		Swizzle4<Vector4, 0x3021> wxzy;
		Swizzle4<Vector4, 0x0121> xyzy;
		Swizzle4<Vector4, 0x1121> yyzy;
		Swizzle4<Vector4, 0x2121> zyzy;
		Swizzle4<Vector4, 0x3121> wyzy;
		Swizzle4<Vector4, 0x0221> xzzy;
		Swizzle4<Vector4, 0x1221> yzzy;
		Swizzle4<Vector4, 0x2221> zzzy;
		Swizzle4<Vector4, 0x3221> wzzy;
		Swizzle4<Vector4, 0x0321> xwzy;
		Swizzle4<Vector4, 0x1321> ywzy;
		Swizzle4<Vector4, 0x2321> zwzy;
		Swizzle4<Vector4, 0x3321> wwzy;
		Swizzle4<Vector4, 0x0031> xxwy;
		Swizzle4<Vector4, 0x1031> yxwy;
		Swizzle4<Vector4, 0x2031> zxwy;
		Swizzle4<Vector4, 0x3031> wxwy;
		Swizzle4<Vector4, 0x0131> xywy;
		Swizzle4<Vector4, 0x1131> yywy;
		Swizzle4<Vector4, 0x2131> zywy;
		Swizzle4<Vector4, 0x3131> wywy;
		Swizzle4<Vector4, 0x0231> xzwy;
		Swizzle4<Vector4, 0x1231> yzwy;
		Swizzle4<Vector4, 0x2231> zzwy;
		Swizzle4<Vector4, 0x3231> wzwy;
		Swizzle4<Vector4, 0x0331> xwwy;
		Swizzle4<Vector4, 0x1331> ywwy;
		Swizzle4<Vector4, 0x2331> zwwy;
		Swizzle4<Vector4, 0x3331> wwwy;
		Swizzle4<Vector4, 0x0002> xxxz;
		Swizzle4<Vector4, 0x1002> yxxz;
		Swizzle4<Vector4, 0x2002> zxxz;
		Swizzle4<Vector4, 0x3002> wxxz;
		Swizzle4<Vector4, 0x0102> xyxz;
		Swizzle4<Vector4, 0x1102> yyxz;
		Swizzle4<Vector4, 0x2102> zyxz;
		Swizzle4<Vector4, 0x3102> wyxz;
		Swizzle4<Vector4, 0x0202> xzxz;
		Swizzle4<Vector4, 0x1202> yzxz;
		Swizzle4<Vector4, 0x2202> zzxz;
		Swizzle4<Vector4, 0x3202> wzxz;
		Swizzle4<Vector4, 0x0302> xwxz;
		Swizzle4<Vector4, 0x1302> ywxz;
		Swizzle4<Vector4, 0x2302> zwxz;
		Swizzle4<Vector4, 0x3302> wwxz;
		Swizzle4<Vector4, 0x0012> xxyz;
		Swizzle4<Vector4, 0x1012> yxyz;
		Swizzle4<Vector4, 0x2012> zxyz;
		Swizzle4<Vector4, 0x3012> wxyz;
		Swizzle4<Vector4, 0x0112> xyyz;
		Swizzle4<Vector4, 0x1112> yyyz;
		Swizzle4<Vector4, 0x2112> zyyz;
		Swizzle4<Vector4, 0x3112> wyyz;
		Swizzle4<Vector4, 0x0212> xzyz;
		Swizzle4<Vector4, 0x1212> yzyz;
		Swizzle4<Vector4, 0x2212> zzyz;
		Swizzle4<Vector4, 0x3212> wzyz;
		Swizzle4<Vector4, 0x0312> xwyz;
		Swizzle4<Vector4, 0x1312> ywyz;
		Swizzle4<Vector4, 0x2312> zwyz;
		Swizzle4<Vector4, 0x3312> wwyz;
		Swizzle4<Vector4, 0x0022> xxzz;
		Swizzle4<Vector4, 0x1022> yxzz;
		Swizzle4<Vector4, 0x2022> zxzz;
		Swizzle4<Vector4, 0x3022> wxzz;
		Swizzle4<Vector4, 0x0122> xyzz;
		Swizzle4<Vector4, 0x1122> yyzz;
		Swizzle4<Vector4, 0x2122> zyzz;
		Swizzle4<Vector4, 0x3122> wyzz;
		Swizzle4<Vector4, 0x0222> xzzz;
		Swizzle4<Vector4, 0x1222> yzzz;
		Swizzle4<Vector4, 0x2222> zzzz;
		Swizzle4<Vector4, 0x3222> wzzz;
		Swizzle4<Vector4, 0x0322> xwzz;
		Swizzle4<Vector4, 0x1322> ywzz;
		Swizzle4<Vector4, 0x2322> zwzz;
		Swizzle4<Vector4, 0x3322> wwzz;
		Swizzle4<Vector4, 0x0032> xxwz;
		Swizzle4<Vector4, 0x1032> yxwz;
		Swizzle4<Vector4, 0x2032> zxwz;
		Swizzle4<Vector4, 0x3032> wxwz;
		Swizzle4<Vector4, 0x0132> xywz;
		Swizzle4<Vector4, 0x1132> yywz;
		Swizzle4<Vector4, 0x2132> zywz;
		Swizzle4<Vector4, 0x3132> wywz;
		Swizzle4<Vector4, 0x0232> xzwz;
		Swizzle4<Vector4, 0x1232> yzwz;
		Swizzle4<Vector4, 0x2232> zzwz;
		Swizzle4<Vector4, 0x3232> wzwz;
		Swizzle4<Vector4, 0x0332> xwwz;
		Swizzle4<Vector4, 0x1332> ywwz;
		Swizzle4<Vector4, 0x2332> zwwz;
		Swizzle4<Vector4, 0x3332> wwwz;
		Swizzle4<Vector4, 0x0003> xxxw;
		Swizzle4<Vector4, 0x1003> yxxw;
		Swizzle4<Vector4, 0x2003> zxxw;
		Swizzle4<Vector4, 0x3003> wxxw;
		Swizzle4<Vector4, 0x0103> xyxw;
		Swizzle4<Vector4, 0x1103> yyxw;
		Swizzle4<Vector4, 0x2103> zyxw;
		Swizzle4<Vector4, 0x3103> wyxw;
		Swizzle4<Vector4, 0x0203> xzxw;
		Swizzle4<Vector4, 0x1203> yzxw;
		Swizzle4<Vector4, 0x2203> zzxw;
		Swizzle4<Vector4, 0x3203> wzxw;
		Swizzle4<Vector4, 0x0303> xwxw;
		Swizzle4<Vector4, 0x1303> ywxw;
		Swizzle4<Vector4, 0x2303> zwxw;
		Swizzle4<Vector4, 0x3303> wwxw;
		Swizzle4<Vector4, 0x0013> xxyw;
		Swizzle4<Vector4, 0x1013> yxyw;
		Swizzle4<Vector4, 0x2013> zxyw;
		Swizzle4<Vector4, 0x3013> wxyw;
		Swizzle4<Vector4, 0x0113> xyyw;
		Swizzle4<Vector4, 0x1113> yyyw;
		Swizzle4<Vector4, 0x2113> zyyw;
		Swizzle4<Vector4, 0x3113> wyyw;
		Swizzle4<Vector4, 0x0213> xzyw;
		Swizzle4<Vector4, 0x1213> yzyw;
		Swizzle4<Vector4, 0x2213> zzyw;
		Swizzle4<Vector4, 0x3213> wzyw;
		Swizzle4<Vector4, 0x0313> xwyw;
		Swizzle4<Vector4, 0x1313> ywyw;
		Swizzle4<Vector4, 0x2313> zwyw;
		Swizzle4<Vector4, 0x3313> wwyw;
		Swizzle4<Vector4, 0x0023> xxzw;
		Swizzle4<Vector4, 0x1023> yxzw;
		Swizzle4<Vector4, 0x2023> zxzw;
		Swizzle4<Vector4, 0x3023> wxzw;
		SwizzleMask4<Vector4, 0x0123> xyzw;
		Swizzle4<Vector4, 0x1123> yyzw;
		Swizzle4<Vector4, 0x2123> zyzw;
		Swizzle4<Vector4, 0x3123> wyzw;
		Swizzle4<Vector4, 0x0223> xzzw;
		Swizzle4<Vector4, 0x1223> yzzw;
		Swizzle4<Vector4, 0x2223> zzzw;
		Swizzle4<Vector4, 0x3223> wzzw;
		Swizzle4<Vector4, 0x0323> xwzw;
		Swizzle4<Vector4, 0x1323> ywzw;
		Swizzle4<Vector4, 0x2323> zwzw;
		Swizzle4<Vector4, 0x3323> wwzw;
		Swizzle4<Vector4, 0x0033> xxww;
		Swizzle4<Vector4, 0x1033> yxww;
		Swizzle4<Vector4, 0x2033> zxww;
		Swizzle4<Vector4, 0x3033> wxww;
		Swizzle4<Vector4, 0x0133> xyww;
		Swizzle4<Vector4, 0x1133> yyww;
		Swizzle4<Vector4, 0x2133> zyww;
		Swizzle4<Vector4, 0x3133> wyww;
		Swizzle4<Vector4, 0x0233> xzww;
		Swizzle4<Vector4, 0x1233> yzww;
		Swizzle4<Vector4, 0x2233> zzww;
		Swizzle4<Vector4, 0x3233> wzww;
		Swizzle4<Vector4, 0x0333> xwww;
		Swizzle4<Vector4, 0x1333> ywww;
		Swizzle4<Vector4, 0x2333> zwww;
		Swizzle4<Vector4, 0x3333> wwww;
	};
};

class Int4 : public LValue<Int4>, public XYZW<Int4>
{
public:
	explicit Int4(RValue<Byte4> cast);
	explicit Int4(RValue<SByte4> cast);
	explicit Int4(RValue<Float4> cast);
	explicit Int4(RValue<Short4> cast);
	explicit Int4(RValue<UShort4> cast);

	Int4();
	Int4(int xyzw);
	Int4(int x, int y, int z, int w);
	Int4(RValue<Int4> rhs);
	Int4(const Int4 &rhs);
	Int4(const Reference<Int4> &rhs);
	Int4(RValue<UInt4> rhs);
	Int4(RValue<Int> rhs);
	Int4(const Int &rhs);

	RValue<Int4> operator=(RValue<Int4> rhs);
	RValue<Int4> operator=(const Int4 &rhs);
	RValue<Int4> operator=(const Reference<Int4> &rhs);

	static Type *type();

private:
	void constant(int x, int y, int z, int w);
};

RValue<Int4> operator+(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator-(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator*(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator/(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator%(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator&(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator|(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator^(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator<<(RValue<Int4> lhs, unsigned char rhs);
RValue<Int4> operator>>(RValue<Int4> lhs, unsigned char rhs);
RValue<Int4> operator<<(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator>>(RValue<Int4> lhs, RValue<Int4> rhs);
RValue<Int4> operator+=(Int4 &lhs, RValue<Int4> rhs);
RValue<Int4> operator-=(Int4 &lhs, RValue<Int4> rhs);
RValue<Int4> operator*=(Int4 &lhs, RValue<Int4> rhs);
RValue<Int4> operator&=(Int4 &lhs, RValue<Int4> rhs);
RValue<Int4> operator^=(Int4 &lhs, RValue<Int4> rhs);
RValue<Int4> operator-(RValue<Int4> val);
RValue<Int4> operator~(RValue<Int4> val);
inline RValue<Int4> operator+(RValue<Int> lhs, RValue<Int4> rhs) { return Int4(lhs) + rhs; }
inline RValue<Int4> operator+(RValue<Int4> lhs, RValue<Int> rhs) { return lhs + Int4(rhs); }

RValue<Int4> CmpEQ(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> CmpLT(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> CmpLE(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> CmpNEQ(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> CmpNLT(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> CmpNLE(RValue<Int4> x, RValue<Int4> y);
inline RValue<Int4> CmpGT(RValue<Int4> x, RValue<Int4> y) { return CmpNLE(x, y); }
inline RValue<Int4> CmpGE(RValue<Int4> x, RValue<Int4> y) { return CmpNLT(x, y); }
RValue<Int4> Max(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> Min(RValue<Int4> x, RValue<Int4> y);
RValue<Int4> RoundInt(RValue<Float4> cast);
RValue<Short8> PackSigned(RValue<Int4> x, RValue<Int4> y);
RValue<UShort8> PackUnsigned(RValue<Int4> x, RValue<Int4> y);
RValue<Int> Extract(RValue<Int4> val, int i);
RValue<Int4> Insert(RValue<Int4> val, RValue<Int> element, int i);
RValue<Int> SignMask(RValue<Int4> x);
RValue<Int4> Swizzle(RValue<Int4> x, uint16_t select);

class UInt4 : public LValue<UInt4>, public XYZW<UInt4>
{
public:
	explicit UInt4(RValue<Float4> cast);

	UInt4();
	UInt4(int xyzw);
	UInt4(RValue<UInt4> rhs);
	UInt4(const UInt4 &rhs);
	UInt4(const Reference<UInt4> &rhs);
	UInt4(RValue<Int4> rhs);
	UInt4(RValue<UInt2> lo, RValue<UInt2> hi);

	RValue<UInt4> operator=(RValue<UInt4> rhs);
	RValue<UInt4> operator=(const UInt4 &rhs);
	RValue<UInt4> operator=(const Reference<UInt4> &rhs);

	static Type *type();

private:
	void constant(int x, int y, int z, int w);
};

RValue<UInt4> operator+(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator-(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator*(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator/(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator%(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator&(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator|(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator^(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator<<(RValue<UInt4> lhs, unsigned char rhs);
RValue<UInt4> operator>>(RValue<UInt4> lhs, unsigned char rhs);
RValue<UInt4> operator>>(RValue<UInt4> lhs, RValue<UInt4> rhs);
RValue<UInt4> operator+=(UInt4 &lhs, RValue<UInt4> rhs);
RValue<UInt4> operator&=(UInt4 &lhs, RValue<UInt4> rhs);
RValue<UInt4> operator~(RValue<UInt4> val);

RValue<UInt4> CmpEQ(RValue<UInt4> x, RValue<UInt4> y);
RValue<UInt4> CmpLT(RValue<UInt4> x, RValue<UInt4> y);
RValue<UInt4> CmpLE(RValue<UInt4> x, RValue<UInt4> y);
RValue<UInt4> CmpNEQ(RValue<UInt4> x, RValue<UInt4> y);
RValue<UInt4> CmpNLT(RValue<UInt4> x, RValue<UInt4> y);
RValue<UInt4> CmpNLE(RValue<UInt4> x, RValue<UInt4> y);
inline RValue<UInt4> CmpGT(RValue<UInt4> x, RValue<UInt4> y) { return CmpNLE(x, y); }
inline RValue<UInt4> CmpGE(RValue<UInt4> x, RValue<UInt4> y) { return CmpNLT(x, y); }
RValue<UInt4> Min(RValue<UInt4> x, RValue<UInt4> y);
RValue<UInt4> MulHigh(RValue<UInt4> x, RValue<UInt4> y);

class Float : public LValue<Float>
{
public:
	explicit Float(RValue<Int> cast);
	explicit Float(RValue<UInt> cast);

	Float() = default;
	Float(float x);
	Float(RValue<Float> rhs);
	Float(const Float &rhs);
	Float(const Reference<Float> &rhs);

	template<int T>
	Float(const SwizzleMask1<Float4, T> &rhs);

	RValue<Float> operator=(RValue<Float> rhs);
	RValue<Float> operator=(const Float &rhs);
	RValue<Float> operator=(const Reference<Float> &rhs);

	template<int T>
	RValue<Float> operator=(const SwizzleMask1<Float4, T> &rhs);

	static Type *type();
};

RValue<Float> operator+(RValue<Float> lhs, RValue<Float> rhs);
RValue<Float> operator-(RValue<Float> lhs, RValue<Float> rhs);
RValue<Float> operator*(RValue<Float> lhs, RValue<Float> rhs);
RValue<Float> operator/(RValue<Float> lhs, RValue<Float> rhs);
RValue<Float> operator+=(Float &lhs, RValue<Float> rhs);
RValue<Float> operator-=(Float &lhs, RValue<Float> rhs);
RValue<Float> operator*=(Float &lhs, RValue<Float> rhs);
RValue<Float> operator-(RValue<Float> val);
RValue<Bool> operator<(RValue<Float> lhs, RValue<Float> rhs);
RValue<Bool> operator<=(RValue<Float> lhs, RValue<Float> rhs);
RValue<Bool> operator>(RValue<Float> lhs, RValue<Float> rhs);
RValue<Bool> operator>=(RValue<Float> lhs, RValue<Float> rhs);
RValue<Bool> operator!=(RValue<Float> lhs, RValue<Float> rhs);
RValue<Bool> operator==(RValue<Float> lhs, RValue<Float> rhs);

RValue<Float> Abs(RValue<Float> x);
RValue<Float> Max(RValue<Float> x, RValue<Float> y);
RValue<Float> Min(RValue<Float> x, RValue<Float> y);
RValue<Float> Rcp_pp(RValue<Float> val, bool exactAtPow2 = false);
RValue<Float> Frac(RValue<Float> x);
RValue<Float> Floor(RValue<Float> x);

class Float2 : public LValue<Float2>
{
public:
	explicit Float2(RValue<Float4> cast);

	Float2() = default;

	static Type *type();
};

class Float4 : public LValue<Float4>, public XYZW<Float4>
{
public:
	explicit Float4(RValue<Byte4> cast);
	explicit Float4(RValue<SByte4> cast);
	explicit Float4(RValue<Short4> cast);
	explicit Float4(RValue<UShort4> cast);
	explicit Float4(RValue<Int4> cast);
	explicit Float4(RValue<UInt4> cast);

	Float4();
	Float4(float xyzw);
	Float4(float x, float y, float z, float w);
	Float4(RValue<Float4> rhs);
	Float4(const Float4 &rhs);
	Float4(const Reference<Float4> &rhs);
	Float4(RValue<Float> rhs);
	Float4(const Float &rhs);
	Float4(const Reference<Float> &rhs);

	template<int T>
	Float4(const SwizzleMask1<Float4, T> &rhs);
	template<int T>
	Float4(const Swizzle4<Float4, T> &rhs);
	template<int X, int Y>
	Float4(const Swizzle2<Float4, X> &x, const Swizzle2<Float4, Y> &y);
	template<int X, int Y>
	Float4(const SwizzleMask2<Float4, X> &x, const Swizzle2<Float4, Y> &y);
	template<int X, int Y>
	Float4(const Swizzle2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y);
	template<int X, int Y>
	Float4(const SwizzleMask2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y);

	RValue<Float4> operator=(float replicate);
	RValue<Float4> operator=(RValue<Float4> rhs);
	RValue<Float4> operator=(const Float4 &rhs);
	RValue<Float4> operator=(const Reference<Float4> &rhs);
	RValue<Float4> operator=(RValue<Float> rhs);
	RValue<Float4> operator=(const Reference<Float> &rhs);

	template<int T>
	RValue<Float4> operator=(const SwizzleMask1<Float4, T> &rhs);
	template<int T>
	RValue<Float4> operator=(const Swizzle4<Float4, T> &rhs);

	static Type *type();

private:
	void constant(float x, float y, float z, float w);
};

RValue<Float4> operator+(RValue<Float4> lhs, RValue<Float4> rhs);
RValue<Float4> operator-(RValue<Float4> lhs, RValue<Float4> rhs);
RValue<Float4> operator*(RValue<Float4> lhs, RValue<Float4> rhs);
RValue<Float4> operator/(RValue<Float4> lhs, RValue<Float4> rhs);
RValue<Float4> operator+=(Float4 &lhs, RValue<Float4> rhs);
RValue<Float4> operator-=(Float4 &lhs, RValue<Float4> rhs);
RValue<Float4> operator*=(Float4 &lhs, RValue<Float4> rhs);
RValue<Float4> operator/=(Float4 &lhs, RValue<Float4> rhs);
RValue<Float4> operator-(RValue<Float4> val);

RValue<Float4> Abs(RValue<Float4> x);
RValue<Float4> Max(RValue<Float4> x, RValue<Float4> y);
RValue<Float4> Min(RValue<Float4> x, RValue<Float4> y);
RValue<Float4> Rcp_pp(RValue<Float4> val, bool exactAtPow2 = false);
RValue<Float4> RcpSqrt_pp(RValue<Float4> val);
RValue<Float4> Sqrt(RValue<Float4> x);
RValue<Float4> Insert(RValue<Float4> val, RValue<Float> element, int i);
RValue<Float> Extract(RValue<Float4> x, int i);
RValue<Float4> Swizzle(RValue<Float4> x, uint16_t select);
RValue<Float4> ShuffleLowHigh(RValue<Float4> x, RValue<Float4> y, uint16_t imm);
RValue<Float4> UnpackLow(RValue<Float4> x, RValue<Float4> y);
RValue<Float4> UnpackHigh(RValue<Float4> x, RValue<Float4> y);
RValue<Float4> Mask(Float4 &lhs, RValue<Float4> rhs, uint16_t select);
RValue<Int4> CmpEQ(RValue<Float4> x, RValue<Float4> y);
RValue<Int4> CmpLT(RValue<Float4> x, RValue<Float4> y);
RValue<Int4> CmpLE(RValue<Float4> x, RValue<Float4> y);
RValue<Int4> CmpNEQ(RValue<Float4> x, RValue<Float4> y);
RValue<Int4> CmpNLT(RValue<Float4> x, RValue<Float4> y);
RValue<Int4> CmpNLE(RValue<Float4> x, RValue<Float4> y);
inline RValue<Int4> CmpGT(RValue<Float4> x, RValue<Float4> y) { return CmpNLE(x, y); }
inline RValue<Int4> CmpGE(RValue<Float4> x, RValue<Float4> y) { return CmpNLT(x, y); }
RValue<Int4> IsInf(RValue<Float4> x);
RValue<Int4> IsNan(RValue<Float4> x);
RValue<Float4> Round(RValue<Float4> x);
RValue<Float4> Trunc(RValue<Float4> x);
RValue<Float4> Frac(RValue<Float4> x);
RValue<Float4> Floor(RValue<Float4> x);
RValue<Float4> Ceil(RValue<Float4> x);

template<class T>
class Pointer : public LValue<Pointer<T>>
{
public:
	template<class S>
	Pointer(RValue<Pointer<S>> pointerS, int alignment = 1) : alignment(alignment)
	{
		Value *pointerT = Nucleus::createBitCast(pointerS.value(), Nucleus::getPointerType(T::type()));
		this->storeValue(pointerT);
	}

	template<class S>
	Pointer(const Pointer<S> &pointer, int alignment = 1) : alignment(alignment)
	{
		Value *pointerS = pointer.loadValue();
		Value *pointerT = Nucleus::createBitCast(pointerS, Nucleus::getPointerType(T::type()));
		this->storeValue(pointerT);
	}

	Pointer(Argument<Pointer<T>> argument);

	Pointer();
	Pointer(RValue<Pointer<T>> rhs);
	Pointer(const Pointer<T> &rhs);
	Pointer(const Reference<Pointer<T>> &rhs);
	Pointer(std::nullptr_t);

	RValue<Pointer<T>> operator=(RValue<Pointer<T>> rhs);
	RValue<Pointer<T>> operator=(const Pointer<T> &rhs);
	RValue<Pointer<T>> operator=(const Reference<Pointer<T>> &rhs);
	RValue<Pointer<T>> operator=(std::nullptr_t);

	Reference<T> operator*();
	Reference<T> operator[](int index);
	Reference<T> operator[](unsigned int index);
	Reference<T> operator[](RValue<Int> index);
	Reference<T> operator[](RValue<UInt> index);

	static Type *type();

private:
	const int alignment;
};

RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, int offset);
RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, RValue<Int> offset);
RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, RValue<UInt> offset);
RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, int offset);
RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, RValue<Int> offset);
RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, int offset);
RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, RValue<Int> offset);
RValue<Pointer<Byte>> operator-=(Pointer<Byte> &lhs, RValue<Int> offset);

template<class T, int S = 1>
class Array : public LValue<T>
{
public:
	Array(int size = S);

	Reference<T> operator[](int index);
	Reference<T> operator[](unsigned int index);
	Reference<T> operator[](RValue<Int> index);
	Reference<T> operator[](RValue<UInt> index);

	// self() returns the this pointer to this Array object.
	// This function exists because operator&() is overloaded by LValue<T>.
	inline Array *self() { return this; }

private:
	Value *allocate() const override;

	const int arraySize;
};

void branch(RValue<Bool> cmp, BasicBlock *bodyBB, BasicBlock *endBB);

template<typename T>
inline Value *ValueOf(const T &v)
{
	return ReactorType<T>::cast(v).loadValue();
}

void Return();

template<class T>
void Return(const T &ret)
{
	static_assert(CanBeUsedAsReturn<ReactorTypeT<T>>::value, "Unsupported type for Return()");
	Nucleus::createRet(ValueOf<T>(ret));
	Nucleus::setInsertBlock(Nucleus::createBasicBlock());
}

template<typename FunctionType>
class Function;

template<typename Return, typename... Arguments>
class Function<Return(Arguments...)>
{
	static_assert(sizeof(AssertFunctionSignatureIsValid<Return(Arguments...)>) >= 0, "Invalid function signature");

public:
	Function();

	virtual ~Function();

	template<int index>
	Argument<typename std::tuple_element<index, std::tuple<Arguments...>>::type> Arg() const
	{
		Value *arg = Nucleus::getArgument(index);
		return Argument<typename std::tuple_element<index, std::tuple<Arguments...>>::type>(arg);
	}

	std::shared_ptr<Routine> operator()(const char *name, ...);
	std::shared_ptr<Routine> operator()(const Config::Edit &cfg, const char *name, ...);

protected:
	Nucleus *core;
	std::vector<Type *> arguments;
};

RValue<Long> Ticks();

template<class T>
LValue<T>::LValue()
{
#ifdef ENABLE_RR_DEBUG_INFO
	materialize();
#endif
}

template<class T>
Reference<T>::Reference(Value *pointer, int alignment) : alignment(alignment)
{
	address = pointer;
}

template<class T>
RValue<T> Reference<T>::operator=(RValue<T> rhs) const
{
	Nucleus::createStore(rhs.value(), address, T::type(), false, alignment);

	return rhs;
}

template<class T>
RValue<T> Reference<T>::operator=(const Reference<T> &ref) const
{
	Value *tmp = Nucleus::createLoad(ref.address, T::type(), false, ref.alignment);
	Nucleus::createStore(tmp, address, T::type(), false, alignment);

	return RValue<T>(tmp);
}

template<class T>
RValue<T> Reference<T>::operator+=(RValue<T> rhs) const
{
	return *this = *this + rhs;
}

template<class T>
Value *Reference<T>::loadValue() const
{
	return Nucleus::createLoad(address, T::type(), false, alignment);
}

template<class T>
RValue<T> Reference<T>::load() const
{
	return RValue<T>(loadValue());
}

template<class T>
int Reference<T>::getAlignment() const
{
	return alignment;
}

template<class T>
RValue<T>::RValue(const RValue<T> &rvalue) : val(rvalue.val)
{
	RR_DEBUG_INFO_EMIT_VAR(val);
}
template<class T>
RValue<T>::RValue(Value *value) : val(value)
{
	RR_DEBUG_INFO_EMIT_VAR(val);
}

template<class T>
RValue<T>::RValue(const T &lvalue) : val(lvalue.loadValue())
{
	RR_DEBUG_INFO_EMIT_VAR(val);
}

template<class T>
RValue<T>::RValue(typename IntLiteral<T>::type i) : val(Nucleus::createConstantInt(i))
{
	RR_DEBUG_INFO_EMIT_VAR(val);
}

template<class T>
RValue<T>::RValue(typename FloatLiteral<T>::type f) : val(Nucleus::createConstantFloat(f))
{
	RR_DEBUG_INFO_EMIT_VAR(val);
}

template<class T>
RValue<T>::RValue(const Reference<T> &ref) : val(ref.loadValue())
{
	RR_DEBUG_INFO_EMIT_VAR(val);
}

template<class Vector4, int T>
Swizzle4<Vector4, T>::operator RValue<Vector4>() const
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *vector = parent->loadValue();

	return Swizzle(RValue<Vector4>(vector), T);
}

template<class Vector4, int T>
RValue<Vector4> SwizzleMask4<Vector4, T>::operator=(RValue<Vector4> rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return Mask(*parent, rhs, T);
}

template<class Vector4, int T>
RValue<Vector4> SwizzleMask4<Vector4, T>::operator=(RValue<typename Scalar<Vector4>::Type> rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return Mask(*parent, Vector4(rhs), T);
}

template<class Vector4, int T>
SwizzleMask1<Vector4, T>::operator RValue<typename Scalar<Vector4>::Type>() const
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return Extract(*parent, T & 0x3);
}

template<class Vector4, int T>
SwizzleMask1<Vector4, T>::operator RValue<Vector4>() const
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *vector = parent->loadValue();

	return Swizzle(RValue<Vector4>(vector), T);
}

template<class Vector4, int T>
RValue<Vector4> SwizzleMask1<Vector4, T>::operator=(float x)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return *parent = Insert(*parent, Float(x), T & 0x3);
}

template<class Vector4, int T>
RValue<Vector4> SwizzleMask1<Vector4, T>::operator=(RValue<typename Scalar<Vector4>::Type> rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return *parent = Insert(*parent, rhs, T & 0x3);
}

template<class Vector4, int T>
SwizzleMask2<Vector4, T>::operator RValue<Vector4>() const
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *vector = parent->loadValue();

	return Swizzle(RValue<Float4>(vector), T);
}

template<class Vector4, int T>
RValue<Vector4> SwizzleMask2<Vector4, T>::operator=(RValue<Vector4> rhs)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return Mask(*parent, Float4(rhs), T);
}

template<int T>
Float::Float(const SwizzleMask1<Float4, T> &rhs)
{
	*this = rhs.operator RValue<Float>();
}

template<int T>
RValue<Float> Float::operator=(const SwizzleMask1<Float4, T> &rhs)
{
	return *this = rhs.operator RValue<Float>();
}

template<int T>
Float4::Float4(const SwizzleMask1<Float4, T> &rhs) : XYZW(this)
{
	*this = rhs.operator RValue<Float4>();
}

template<int T>
Float4::Float4(const Swizzle4<Float4, T> &rhs) : XYZW(this)
{
	*this = rhs.operator RValue<Float4>();
}

template<int X, int Y>
Float4::Float4(const Swizzle2<Float4, X> &x, const Swizzle2<Float4, Y> &y) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	*this = ShuffleLowHigh(*x.parent, *y.parent, (uint16_t(X) & 0xFF00u) | (uint16_t(Y >> 8) & 0x00FFu));
}

template<int X, int Y>
Float4::Float4(const SwizzleMask2<Float4, X> &x, const SwizzleMask2<Float4, Y> &y) : XYZW(this)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	*this = ShuffleLowHigh(*x.parent, *y.parent, (uint16_t(X) & 0xFF00u) | (uint16_t(Y >> 8) & 0x00FFu));
}

template<int T>
RValue<Float4> Float4::operator=(const SwizzleMask1<Float4, T> &rhs)
{
	return *this = rhs.operator RValue<Float4>();
}

template<int T>
RValue<Float4> Float4::operator=(const Swizzle4<Float4, T> &rhs)
{
	return *this = rhs.operator RValue<Float4>();
}

// Returns a reactor pointer to the fixed-address ptr.
RValue<Pointer<Byte>> ConstantPointer(void const *ptr);

template<class T>
Pointer<T>::Pointer(Argument<Pointer<T>> argument) : alignment(1)
{
	this->store(argument.rvalue());
}

template<class T>
Pointer<T>::Pointer() : alignment(1)
{
}

template<class T>
Pointer<T>::Pointer(RValue<Pointer<T>> rhs) : alignment(1)
{
	this->store(rhs);
}

template<class T>
Pointer<T>::Pointer(const Pointer<T> &rhs) : alignment(rhs.alignment)
{
	this->store(rhs.load());
}

template<class T>
Pointer<T>::Pointer(const Reference<Pointer<T>> &rhs) : alignment(rhs.getAlignment())
{
	this->store(rhs.load());
}

template<class T>
RValue<Pointer<T>> Pointer<T>::operator=(RValue<Pointer<T>> rhs)
{
	return this->store(rhs);
}

template<class T>
RValue<Pointer<T>> Pointer<T>::operator=(const Pointer<T> &rhs)
{
	return this->store(rhs.load());
}

template<class T>
RValue<Pointer<T>> Pointer<T>::operator=(const Reference<Pointer<T>> &rhs)
{
	return this->store(rhs.load());
}

template<class T>
Reference<T> Pointer<T>::operator*()
{
	return Reference<T>(this->loadValue(), alignment);
}

template<class T>
Reference<T> Pointer<T>::operator[](RValue<UInt> index)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *element = Nucleus::createGEP(this->loadValue(), T::type(), index.value(), true);

	return Reference<T>(element, alignment);
}

template<class T>
Type *Pointer<T>::type()
{
	return Nucleus::getPointerType(T::type());
}

template<class T, int S>
Array<T, S>::Array(int size) : arraySize(size)
{
}

template<class T, int S>
Value *Array<T, S>::allocate() const
{
	return Nucleus::allocateStackVariable(T::type(), arraySize);
}

template<class T, int S>
Reference<T> Array<T, S>::operator[](int index)
{
	Value *element = this->getElementPointer(Nucleus::createConstantInt(index), false);

	return Reference<T>(element);
}

template<class T, int S>
Reference<T> Array<T, S>::operator[](RValue<Int> index)
{
	Value *element = this->getElementPointer(index.value(), false);

	return Reference<T>(element);
}

template<class T>
RValue<T> IfThenElse(RValue<Bool> condition, RValue<T> ifTrue, RValue<T> ifFalse)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return RValue<T>(Nucleus::createSelect(condition.value(), ifTrue.value(), ifFalse.value()));
}

template<class T>
RValue<T> IfThenElse(RValue<Bool> condition, RValue<T> ifTrue, const T &ifFalse)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *falseValue = ifFalse.loadValue();

	return RValue<T>(Nucleus::createSelect(condition.value(), ifTrue.value(), falseValue));
}

template<class T>
RValue<T> IfThenElse(RValue<Bool> condition, const T &ifTrue, const T &ifFalse)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *trueValue = ifTrue.loadValue();
	Value *falseValue = ifFalse.loadValue();

	return RValue<T>(Nucleus::createSelect(condition.value(), trueValue, falseValue));
}

template<typename Return, typename... Arguments>
Function<Return(Arguments...)>::Function()
{
	core = new Nucleus();

	Type *types[] = { Arguments::type()... };
	for(Type *type : types)
	{
		if(type != Void::type())
		{
			arguments.push_back(type);
		}
	}

	Nucleus::createFunction(Return::type(), arguments);
}

template<typename Return, typename... Arguments>
Function<Return(Arguments...)>::~Function()
{
	delete core;
}

template<typename Return, typename... Arguments>
std::shared_ptr<Routine> Function<Return(Arguments...)>::operator()(const char *name, ...)
{
	char fullName[1024 + 1];

	va_list vararg;
	va_start(vararg, name);
	vsnprintf(fullName, 1024, name, vararg);
	va_end(vararg);

	return core->acquireRoutine(fullName, Config::Edit::None);
}

template<class T, class S>
RValue<T> ReinterpretCast(RValue<S> val)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return RValue<T>(Nucleus::createBitCast(val.value(), T::type()));
}

template<class T, class S>
RValue<T> ReinterpretCast(const LValue<S> &var)
{
	RR_DEBUG_INFO_UPDATE_LOC();
	Value *val = var.loadValue();

	return RValue<T>(Nucleus::createBitCast(val, T::type()));
}

template<class T, class S>
RValue<T> ReinterpretCast(const Reference<S> &var)
{
	return ReinterpretCast<T>(RValue<S>(var));
}

template<class T>
RValue<T> As(Value *val)
{
	RR_DEBUG_INFO_UPDATE_LOC();

	return RValue<T>(Nucleus::createBitCast(val, T::type()));
}

template<class T, class S>
RValue<T> As(RValue<S> val)
{
	return ReinterpretCast<T>(val);
}

template<class T, class S>
RValue<T> As(const LValue<S> &var)
{
	return ReinterpretCast<T>(var);
}

template<class T, class S>
RValue<T> As(const Reference<S> &val)
{
	return ReinterpretCast<T>(val);
}

class ForData
{
public:
	ForData(bool init) : loopOnce(init)
	{
	}

	operator bool()
	{
		return loopOnce;
	}

	bool operator=(bool value)
	{
		return loopOnce = value;
	}

	bool setup()
	{
		RR_DEBUG_INFO_FLUSH();

		if(Nucleus::getInsertBlock() != endBB)
		{
			testBB = Nucleus::createBasicBlock();

			Nucleus::createBr(testBB);
			Nucleus::setInsertBlock(testBB);

			return true;
		}

		return false;
	}

	bool test(RValue<Bool> cmp)
	{
		BasicBlock *bodyBB = Nucleus::createBasicBlock();

		endBB = Nucleus::createBasicBlock();

		Nucleus::createCondBr(cmp.value(), bodyBB, endBB);
		Nucleus::setInsertBlock(bodyBB);

		return true;
	}

	void end()
	{
		Nucleus::createBr(testBB);
		Nucleus::setInsertBlock(endBB);
	}

private:
	BasicBlock *testBB = nullptr;
	BasicBlock *endBB = nullptr;
	bool loopOnce = true;
};

class IfElseData
{
public:
	IfElseData(RValue<Bool> cmp) : iteration(0)
	{
		condition = cmp.value();

		beginBB = Nucleus::getInsertBlock();
		trueBB = Nucleus::createBasicBlock();
		falseBB = nullptr;
		endBB = Nucleus::createBasicBlock();

		Nucleus::setInsertBlock(trueBB);
	}

	~IfElseData()
	{
		Nucleus::createBr(endBB);

		Nucleus::setInsertBlock(beginBB);
		Nucleus::createCondBr(condition, trueBB, falseBB ? falseBB : endBB);

		Nucleus::setInsertBlock(endBB);
	}

	operator int()
	{
		return iteration;
	}

	IfElseData &operator++()
	{
		++iteration;

		return *this;
	}

	void elseClause()
	{
		Nucleus::createBr(endBB);

		falseBB = Nucleus::createBasicBlock();
		Nucleus::setInsertBlock(falseBB);
	}

private:
	Value *condition;
	BasicBlock *beginBB;
	BasicBlock *trueBB;
	BasicBlock *falseBB;
	BasicBlock *endBB;
	int iteration;
};

#define For(init, cond, inc) \
	for(ForData for__ = true; for__; for__ = false) \
		for(init; for__.setup() && for__.test(cond); inc, for__.end())

#define While(cond) For((void)0, cond, (void)0)

#define Do \
	{ \
		BasicBlock *body__ = Nucleus::createBasicBlock(); \
		Nucleus::createBr(body__); \
		Nucleus::setInsertBlock(body__);

#define Until(cond) \
		BasicBlock *end__ = Nucleus::createBasicBlock(); \
		Nucleus::createCondBr((cond).value(), end__, body__); \
		Nucleus::setInsertBlock(end__); \
	} \
	do \
	{ \
	} while(false) // Require a semi-colon at the end of the Until()

enum
{
	IF_BLOCK__,
	ELSE_CLAUSE__,
	ELSE_BLOCK__,
	IFELSE_NUM__
};

#define If(cond) \
	for(IfElseData ifElse__(cond); ifElse__ < IFELSE_NUM__; ++ifElse__) \
		if(ifElse__ == IF_BLOCK__)

#define Else \
	else if(ifElse__ == ELSE_CLAUSE__) \
	{ \
		ifElse__.elseClause(); \
	} \
	else // // ELSE_BLOCK__

// The OFFSET macro is a generalization of the offsetof() macro.
// It allows e.g. getting the offset of array elements, even when indexed dynamically.
// We cast the address '32' and subtract it again, because null-dereference is undefined behavior.
#define OFFSET(s,m) ((int)(size_t) & reinterpret_cast<const volatile char &>((((s*)32)->m)) - 32)

}

#endif
