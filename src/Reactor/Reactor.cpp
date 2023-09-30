// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
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

#include "Reactor.hpp"

#include "Debug.hpp"
#ifdef ENABLE_RR_PRINT
#include "Print.hpp"
#endif

#include <algorithm>
#include <cmath>

namespace rr {

const Config::Edit Config::Edit::None = {};

Config Config::Edit::apply(const Config &cfg) const
{
	if(this == &None) { return cfg; }

	auto level = optLevelChanged ? optLevel : cfg.optimization.getLevel();
	auto passes = cfg.optimization.getPasses();
	apply(optPassEdits, passes);

	return Config{ Optimization{ level, passes } };
}

template<typename T>
void rr::Config::Edit::apply(const std::vector<std::pair<ListEdit, T>> &edits, std::vector<T> &list) const
{
	for(auto &edit : edits)
	{
		switch(edit.first)
		{
			case ListEdit::Add:
				list.push_back(edit.second);
				break;
			case ListEdit::Remove:
				list.erase(std::remove_if(list.begin(), list.end(), [&](T item) { return item == edit.second; }), list.end());
				break;
			case ListEdit::Clear:
				list.clear();
				break;
		}
	}
}

// Set of variables that do not have a stack location yet.
thread_local std::unordered_set<const Variable *> *Variable::unmaterializedVariables = nullptr;

Variable::Variable()
{
	unmaterializedVariables->emplace(this);
}

Variable::~Variable()
{
	unmaterializedVariables->erase(this);
}

void Variable::materialize() const
{
	if(!address)
	{
		address = allocate();
		RR_DEBUG_INFO_EMIT_VAR(address);

		if(rvalue)
		{
			storeValue(rvalue);
			rvalue = nullptr;
		}
	}
}

Value *Variable::loadValue() const
{
	if(rvalue)
	{
		return rvalue;
	}

	if(!address)
	{
		materialize();
	}

	return Nucleus::createLoad(address, getType(), false, 0);
}

Value *Variable::storeValue(Value *value) const
{
	if(address)
	{
		return Nucleus::createStore(value, address, getType(), false, 0);
	}

	rvalue = value;

	return value;
}

Value *Variable::getBaseAddress() const
{
	materialize();

	return address;
}

Value *Variable::getElementPointer(Value *index, bool unsignedIndex) const
{
	return Nucleus::createGEP(getBaseAddress(), getType(), index, unsignedIndex);
}

Value *Variable::allocate() const
{
	return Nucleus::allocateStackVariable(getType());
}

void Variable::materializeAll()
{
	for(auto *var : *unmaterializedVariables)
	{
		var->materialize();
	}

	unmaterializedVariables->clear();
}

void Variable::killUnmaterialized()
{
	unmaterializedVariables->clear();
}

// Only 8 bits out of 16 of the select value are used.
// More specifically, the value should look like:
//
//    msb               lsb
//     v                 v
//    [..xx|..yy|..zz|..ww]    where '.' means an ignored bit
//
// This format makes it easy to write calls with hexadecimal select values,
// since each hex digit is a separate swizzle index.
//
// For example:
//      createSwizzle4( [a,b,c,d], 0x0123 ) -> [a,b,c,d]
//      createSwizzle4( [a,b,c,d], 0x0033 ) -> [a,a,d,d]
//
static Value *createSwizzle4(Value *val, uint16_t select)
{
	int swizzle[4] = {
		(select >> 12) & 0x03,
		(select >> 8) & 0x03,
		(select >> 4) & 0x03,
		(select >> 0) & 0x03,
	};

	return Nucleus::createShuffleVector(val, val, swizzle);
}

static Value *createMask4(Value *lhs, Value *rhs, uint16_t select)
{
	bool mask[4] = { false, false, false, false };

	mask[(select >> 12) & 0x03] = true;
	mask[(select >> 8) & 0x03] = true;
	mask[(select >> 4) & 0x03] = true;
	mask[(select >> 0) & 0x03] = true;

	int swizzle[4] = {
		mask[0] ? 4 : 0,
		mask[1] ? 5 : 1,
		mask[2] ? 6 : 2,
		mask[3] ? 7 : 3,
	};

	return Nucleus::createShuffleVector(lhs, rhs, swizzle);
}

Bool::Bool(bool x)
{
	storeValue(Nucleus::createConstantBool(x));
}

Bool::Bool(RValue<Bool> rhs)
{
	store(rhs);
}

RValue<Bool> Bool::operator=(RValue<Bool> rhs)
{
	return store(rhs);
}

RValue<Bool> Bool::operator=(const Bool &rhs)
{
	return store(rhs.load());
}

RValue<Bool> operator!(RValue<Bool> val)
{
	return RValue<Bool>(Nucleus::createNot(val.value()));
}

RValue<Bool> operator&&(RValue<Bool> lhs, RValue<Bool> rhs)
{
	return RValue<Bool>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<Bool> operator||(RValue<Bool> lhs, RValue<Bool> rhs)
{
	return RValue<Bool>(Nucleus::createOr(lhs.value(), rhs.value()));
}

Byte::Byte(RValue<Int> cast)
{
	Value *integer = Nucleus::createTrunc(cast.value(), Byte::type());

	storeValue(integer);
}

Byte::Byte(int x)
{
	storeValue(Nucleus::createConstantByte((unsigned char)x));
}

RValue<Bool> operator!=(RValue<Byte> lhs, RValue<Byte> rhs)
{
	return RValue<Bool>(Nucleus::createICmpNE(lhs.value(), rhs.value()));
}

SByte::SByte(RValue<Int> cast)
{
	Value *integer = Nucleus::createTrunc(cast.value(), SByte::type());

	storeValue(integer);
}

SByte::SByte(signed char x)
{
	storeValue(Nucleus::createConstantByte(x));
}

Short::Short(RValue<Int> cast)
{
	Value *integer = Nucleus::createTrunc(cast.value(), Short::type());

	storeValue(integer);
}

Short::Short(short x)
{
	storeValue(Nucleus::createConstantShort(x));
}

RValue<Bool> operator==(RValue<Short> lhs, RValue<Short> rhs)
{
	return RValue<Bool>(Nucleus::createICmpEQ(lhs.value(), rhs.value()));
}

UShort::UShort(RValue<UInt> cast)
{
	Value *integer = Nucleus::createTrunc(cast.value(), UShort::type());

	storeValue(integer);
}

UShort::UShort(RValue<Int> cast)
{
	Value *integer = Nucleus::createTrunc(cast.value(), UShort::type());

	storeValue(integer);
}

UShort::UShort(unsigned short x)
{
	storeValue(Nucleus::createConstantShort(x));
}

UShort::UShort(const Reference<UShort> &rhs)
{
	store(rhs.load());
}

RValue<UShort> UShort::operator=(RValue<UShort> rhs)
{
	return store(rhs);
}

RValue<UShort> operator&(RValue<UShort> lhs, RValue<UShort> rhs)
{
	return RValue<UShort>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<UShort> operator|(RValue<UShort> lhs, RValue<UShort> rhs)
{
	return RValue<UShort>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<UShort> operator>>(RValue<UShort> lhs, RValue<UShort> rhs)
{
	return RValue<UShort>(Nucleus::createLShr(lhs.value(), rhs.value()));
}

Byte4::Byte4(RValue<Byte8> cast)
{
	storeValue(Nucleus::createBitCast(cast.value(), type()));
}

Byte4::Byte4(const Reference<Byte4> &rhs)
{
	store(rhs.load());
}

Byte8::Byte8(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6, uint8_t x7)
{
	int64_t constantVector[8] = { x0, x1, x2, x3, x4, x5, x6, x7 };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

Byte8::Byte8(const Byte8 &rhs)
{
	store(rhs.load());
}

Byte8::Byte8(const Reference<Byte8> &rhs)
{
	store(rhs.load());
}

RValue<Byte8> Byte8::operator=(RValue<Byte8> rhs)
{
	return store(rhs);
}

RValue<Byte8> Byte8::operator=(const Byte8 &rhs)
{
	return store(rhs.load());
}

RValue<Byte8> Byte8::operator=(const Reference<Byte8> &rhs)
{
	return store(rhs.load());
}

RValue<Byte8> operator+(RValue<Byte8> lhs, RValue<Byte8> rhs)
{
	return RValue<Byte8>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<Byte8> operator-(RValue<Byte8> lhs, RValue<Byte8> rhs)
{
	return RValue<Byte8>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<Byte8> operator&(RValue<Byte8> lhs, RValue<Byte8> rhs)
{
	return RValue<Byte8>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<Byte8> operator|(RValue<Byte8> lhs, RValue<Byte8> rhs)
{
	return RValue<Byte8>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<Byte8> operator^(RValue<Byte8> lhs, RValue<Byte8> rhs)
{
	return RValue<Byte8>(Nucleus::createXor(lhs.value(), rhs.value()));
}

RValue<Byte8> operator+=(Byte8 &lhs, RValue<Byte8> rhs)
{
	return lhs = lhs + rhs;
}

RValue<Byte8> operator&=(Byte8 &lhs, RValue<Byte8> rhs)
{
	return lhs = lhs & rhs;
}

RValue<Byte8> operator|=(Byte8 &lhs, RValue<Byte8> rhs)
{
	return lhs = lhs | rhs;
}

RValue<Byte8> operator^=(Byte8 &lhs, RValue<Byte8> rhs)
{
	return lhs = lhs ^ rhs;
}

RValue<Short4> Unpack(RValue<Byte4> x)
{
	int shuffle[16] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };

	return As<Short4>(Nucleus::createShuffleVector(x.value(), x.value(), shuffle));
}

RValue<Short4> Unpack(RValue<Byte4> x, RValue<Byte4> y)
{
	return UnpackLow(As<Byte8>(x), As<Byte8>(y));
}

RValue<Short4> UnpackLow(RValue<Byte8> x, RValue<Byte8> y)
{
	int shuffle[16] = { 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23 };

	return As<Short4>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));
}

RValue<Short4> UnpackHigh(RValue<Byte8> x, RValue<Byte8> y)
{
	int shuffle[16] = { 0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23 };

	auto lowHigh = RValue<Byte16>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));

	return As<Short4>(Swizzle(As<Int4>(lowHigh), 0x2323));
}

Short2::Short2(RValue<Short4> cast)
{
	storeValue(Nucleus::createBitCast(cast.value(), type()));
}

UShort2::UShort2(RValue<UShort4> cast)
{
	storeValue(Nucleus::createBitCast(cast.value(), type()));
}

Short4::Short4(RValue<Int> cast)
{
	Value *vector = loadValue();
	Value *element = Nucleus::createTrunc(cast.value(), Short::type());
	Value *insert = Nucleus::createInsertElement(vector, element, 0);
	Value *swizzle = Swizzle(RValue<Short4>(insert), 0x0000).value();

	storeValue(swizzle);
}

Short4::Short4(short xyzw)
{
	int64_t constantVector[4] = { xyzw, xyzw, xyzw, xyzw };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

Short4::Short4(short x, short y, short z, short w)
{
	int64_t constantVector[4] = { x, y, z, w };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

Short4::Short4(RValue<Short4> rhs)
{
	store(rhs);
}

Short4::Short4(const Short4 &rhs)
{
	store(rhs.load());
}

Short4::Short4(const Reference<Short4> &rhs)
{
	store(rhs.load());
}

Short4::Short4(RValue<UShort4> rhs)
{
	storeValue(rhs.value());
}

RValue<Short4> Short4::operator=(RValue<Short4> rhs)
{
	return store(rhs);
}

RValue<Short4> Short4::operator=(const Short4 &rhs)
{
	return store(rhs.load());
}

RValue<Short4> Short4::operator=(const Reference<Short4> &rhs)
{
	return store(rhs.load());
}

RValue<Short4> Short4::operator=(RValue<UShort4> rhs)
{
	return RValue<Short4>(storeValue(rhs.value()));
}

RValue<Short4> Short4::operator=(const UShort4 &rhs)
{
	return RValue<Short4>(storeValue(rhs.loadValue()));
}

RValue<Short4> operator+(RValue<Short4> lhs, RValue<Short4> rhs)
{
	return RValue<Short4>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<Short4> operator-(RValue<Short4> lhs, RValue<Short4> rhs)
{
	return RValue<Short4>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<Short4> operator*(RValue<Short4> lhs, RValue<Short4> rhs)
{
	return RValue<Short4>(Nucleus::createMul(lhs.value(), rhs.value()));
}

RValue<Short4> operator&(RValue<Short4> lhs, RValue<Short4> rhs)
{
	return RValue<Short4>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<Short4> operator|(RValue<Short4> lhs, RValue<Short4> rhs)
{
	return RValue<Short4>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<Short4> operator^(RValue<Short4> lhs, RValue<Short4> rhs)
{
	return RValue<Short4>(Nucleus::createXor(lhs.value(), rhs.value()));
}

RValue<Short4> operator+=(Short4 &lhs, RValue<Short4> rhs)
{
	return lhs = lhs + rhs;
}

RValue<Short4> operator&=(Short4 &lhs, RValue<Short4> rhs)
{
	return lhs = lhs & rhs;
}

RValue<Short4> operator|=(Short4 &lhs, RValue<Short4> rhs)
{
	return lhs = lhs | rhs;
}

RValue<Short4> operator<<=(Short4 &lhs, unsigned char rhs)
{
	return lhs = lhs << rhs;
}

RValue<Short4> operator>>=(Short4 &lhs, unsigned char rhs)
{
	return lhs = lhs >> rhs;
}

RValue<Short4> operator-(RValue<Short4> val)
{
	return RValue<Short4>(Nucleus::createNeg(val.value()));
}

RValue<Short4> operator~(RValue<Short4> val)
{
	return RValue<Short4>(Nucleus::createNot(val.value()));
}

RValue<Short4> RoundShort4(RValue<Float4> cast)
{
	RValue<Int4> int4 = RoundInt(cast);
	return As<Short4>(PackSigned(int4, int4));
}

RValue<Int2> UnpackLow(RValue<Short4> x, RValue<Short4> y)
{
	int shuffle[8] = { 0, 8, 1, 9, 2, 10, 3, 11 };

	return As<Int2>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));
}

RValue<Int2> UnpackHigh(RValue<Short4> x, RValue<Short4> y)
{
	int shuffle[8] = { 0, 8, 1, 9, 2, 10, 3, 11 };

	auto lowHigh = RValue<Short8>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));

	return As<Int2>(Swizzle(As<Int4>(lowHigh), 0x2323));
}

RValue<Short4> Swizzle(RValue<Short4> x, uint16_t select)
{
	int shuffle[8] = {
		(select >> 12) & 0x03,
		(select >> 8) & 0x03,
		(select >> 4) & 0x03,
		(select >> 0) & 0x03,
		(select >> 12) & 0x03,
		(select >> 8) & 0x03,
		(select >> 4) & 0x03,
		(select >> 0) & 0x03,
	};

	return As<Short4>(Nucleus::createShuffleVector(x.value(), x.value(), shuffle));
}

RValue<Short4> Insert(RValue<Short4> val, RValue<Short> element, int i)
{
	return RValue<Short4>(Nucleus::createInsertElement(val.value(), element.value(), i));
}

RValue<Short> Extract(RValue<Short4> val, int i)
{
	return RValue<Short>(Nucleus::createExtractElement(val.value(), Short::type(), i));
}

UShort4::UShort4(RValue<Int4> cast)
{
	*this = Short4(cast);
}

UShort4::UShort4(unsigned short xyzw)
{
	int64_t constantVector[4] = { xyzw, xyzw, xyzw, xyzw };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

UShort4::UShort4(RValue<UShort4> rhs)
{
	store(rhs);
}

UShort4::UShort4(const Reference<UShort4> &rhs)
{
	store(rhs.load());
}

UShort4::UShort4(RValue<Short4> rhs)
{
	storeValue(rhs.value());
}

RValue<UShort4> UShort4::operator=(RValue<UShort4> rhs)
{
	return store(rhs);
}

RValue<UShort4> UShort4::operator=(const UShort4 &rhs)
{
	return store(rhs.load());
}

RValue<UShort4> UShort4::operator=(const Reference<UShort4> &rhs)
{
	return store(rhs.load());
}

RValue<UShort4> UShort4::operator=(RValue<Short4> rhs)
{
	return RValue<UShort4>(storeValue(rhs.value()));
}

RValue<UShort4> UShort4::operator=(const Short4 &rhs)
{
	return RValue<UShort4>(storeValue(rhs.loadValue()));
}

RValue<UShort4> operator+(RValue<UShort4> lhs, RValue<UShort4> rhs)
{
	return RValue<UShort4>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<UShort4> operator-(RValue<UShort4> lhs, RValue<UShort4> rhs)
{
	return RValue<UShort4>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<UShort4> operator*(RValue<UShort4> lhs, RValue<UShort4> rhs)
{
	return RValue<UShort4>(Nucleus::createMul(lhs.value(), rhs.value()));
}

RValue<UShort4> operator~(RValue<UShort4> val)
{
	return RValue<UShort4>(Nucleus::createNot(val.value()));
}

Short8::Short8(short c)
{
	int64_t constantVector[8] = { c, c, c, c, c, c, c, c };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

Short8::Short8(short c0, short c1, short c2, short c3, short c4, short c5, short c6, short c7)
{
	int64_t constantVector[8] = { c0, c1, c2, c3, c4, c5, c6, c7 };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

RValue<Short8> operator+(RValue<Short8> lhs, RValue<Short8> rhs)
{
	return RValue<Short8>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<Int4> Abs(RValue<Int4> x)
{
	auto negative = x >> 31;

	return (x ^ negative) - negative;
}

UShort8::UShort8(const Reference<UShort8> &rhs)
{
	store(rhs.load());
}

UShort8::UShort8(RValue<UShort4> lo, RValue<UShort4> hi)
{
	int shuffle[8] = { 0, 1, 2, 3, 8, 9, 10, 11 };

	Value *packed = Nucleus::createShuffleVector(lo.value(), hi.value(), shuffle);

	storeValue(packed);
}

RValue<UShort8> UShort8::operator=(const UShort8 &rhs)
{
	return store(rhs.load());
}

RValue<UShort8> UShort8::operator=(const Reference<UShort8> &rhs)
{
	return store(rhs.load());
}

Int::Int(Argument<Int> argument)
{
	store(argument.rvalue());
}

Int::Int(RValue<Byte> cast)
{
	Value *integer = Nucleus::createZExt(cast.value(), Int::type());

	storeValue(integer);
}

Int::Int(RValue<SByte> cast)
{
	Value *integer = Nucleus::createSExt(cast.value(), Int::type());

	storeValue(integer);
}

Int::Int(RValue<Short> cast)
{
	Value *integer = Nucleus::createSExt(cast.value(), Int::type());

	storeValue(integer);
}

Int::Int(RValue<UShort> cast)
{
	Value *integer = Nucleus::createZExt(cast.value(), Int::type());

	storeValue(integer);
}

Int::Int(RValue<Int2> cast)
{
	*this = Extract(cast, 0);
}

Int::Int(RValue<Float> cast)
{
	Value *integer = Nucleus::createFPToSI(cast.value(), Int::type());

	storeValue(integer);
}

Int::Int(int x)
{
	storeValue(Nucleus::createConstantInt(x));
}

Int::Int(RValue<Int> rhs)
{
	store(rhs);
}

Int::Int(RValue<UInt> rhs)
{
	storeValue(rhs.value());
}

Int::Int(const Int &rhs)
{
	store(rhs.load());
}

Int::Int(const Reference<Int> &rhs)
{
	store(rhs.load());
}

Int::Int(const UInt &rhs)
{
	storeValue(rhs.loadValue());
}

Int::Int(const Reference<UInt> &rhs)
{
	storeValue(rhs.loadValue());
}

RValue<Int> Int::operator=(int rhs)
{
	return RValue<Int>(storeValue(Nucleus::createConstantInt(rhs)));
}

RValue<Int> Int::operator=(RValue<Int> rhs)
{
	return store(rhs);
}

RValue<Int> Int::operator=(const Int &rhs)
{
	return store(rhs.load());
}

RValue<Int> Int::operator=(const Reference<Int> &rhs)
{
	return store(rhs.load());
}

RValue<Int> operator+(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<Int> operator-(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<Int> operator*(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createMul(lhs.value(), rhs.value()));
}

RValue<Int> operator/(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createSDiv(lhs.value(), rhs.value()));
}

RValue<Int> operator%(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createSRem(lhs.value(), rhs.value()));
}

RValue<Int> operator&(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<Int> operator|(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<Int> operator^(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createXor(lhs.value(), rhs.value()));
}

RValue<Int> operator<<(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createShl(lhs.value(), rhs.value()));
}

RValue<Int> operator>>(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Int>(Nucleus::createAShr(lhs.value(), rhs.value()));
}

RValue<Int> operator+=(Int &lhs, RValue<Int> rhs)
{
	return lhs = lhs + rhs;
}

RValue<Int> operator-=(Int &lhs, RValue<Int> rhs)
{
	return lhs = lhs - rhs;
}

RValue<Int> operator*=(Int &lhs, RValue<Int> rhs)
{
	return lhs = lhs * rhs;
}

RValue<Int> operator&=(Int &lhs, RValue<Int> rhs)
{
	return lhs = lhs & rhs;
}

RValue<Int> operator|=(Int &lhs, RValue<Int> rhs)
{
	return lhs = lhs | rhs;
}

RValue<Int> operator<<=(Int &lhs, RValue<Int> rhs)
{
	return lhs = lhs << rhs;
}

RValue<Int> operator-(RValue<Int> val)
{
	return RValue<Int>(Nucleus::createNeg(val.value()));
}

RValue<Int> operator~(RValue<Int> val)
{
	return RValue<Int>(Nucleus::createNot(val.value()));
}

RValue<Bool> operator<(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Bool>(Nucleus::createICmpSLT(lhs.value(), rhs.value()));
}

RValue<Bool> operator>(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Bool>(Nucleus::createICmpSGT(lhs.value(), rhs.value()));
}

RValue<Bool> operator>=(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Bool>(Nucleus::createICmpSGE(lhs.value(), rhs.value()));
}

RValue<Bool> operator!=(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Bool>(Nucleus::createICmpNE(lhs.value(), rhs.value()));
}

RValue<Bool> operator==(RValue<Int> lhs, RValue<Int> rhs)
{
	return RValue<Bool>(Nucleus::createICmpEQ(lhs.value(), rhs.value()));
}

RValue<Int> Max(RValue<Int> x, RValue<Int> y)
{
	return IfThenElse(x > y, x, y);
}

RValue<Int> Min(RValue<Int> x, RValue<Int> y)
{
	return IfThenElse(x < y, x, y);
}

RValue<Int> Clamp(RValue<Int> x, RValue<Int> min, RValue<Int> max)
{
	return Min(Max(x, min), max);
}

Long::Long(RValue<Long> rhs)
{
	store(rhs);
}

RValue<Long> Long::operator=(int64_t rhs)
{
	return RValue<Long>(storeValue(Nucleus::createConstantLong(rhs)));
}

RValue<Long> Long::operator=(RValue<Long> rhs)
{
	return store(rhs);
}

RValue<Long> operator+(RValue<Long> lhs, RValue<Long> rhs)
{
	return RValue<Long>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<Long> operator-(RValue<Long> lhs, RValue<Long> rhs)
{
	return RValue<Long>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<Long> operator+=(Long &lhs, RValue<Long> rhs)
{
	return lhs = lhs + rhs;
}

UInt::UInt(RValue<UShort> cast)
{
	Value *integer = Nucleus::createZExt(cast.value(), UInt::type());

	storeValue(integer);
}

UInt::UInt(int x)
{
	storeValue(Nucleus::createConstantInt(x));
}

UInt::UInt(unsigned int x)
{
	storeValue(Nucleus::createConstantInt(x));
}

UInt::UInt(RValue<UInt> rhs)
{
	store(rhs);
}

UInt::UInt(RValue<Int> rhs)
{
	storeValue(rhs.value());
}

UInt::UInt(const UInt &rhs)
{
	store(rhs.load());
}

UInt::UInt(const Reference<UInt> &rhs)
{
	store(rhs.load());
}

RValue<UInt> UInt::operator=(unsigned int rhs)
{
	return RValue<UInt>(storeValue(Nucleus::createConstantInt(rhs)));
}

RValue<UInt> UInt::operator=(RValue<UInt> rhs)
{
	return store(rhs);
}

RValue<UInt> UInt::operator=(RValue<Int> rhs)
{
	storeValue(rhs.value());

	return RValue<UInt>(rhs);
}

RValue<UInt> operator+(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<UInt>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<UInt> operator*(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<UInt>(Nucleus::createMul(lhs.value(), rhs.value()));
}

RValue<UInt> operator&(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<UInt>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<UInt> operator|(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<UInt>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<UInt> operator<<(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<UInt>(Nucleus::createShl(lhs.value(), rhs.value()));
}

RValue<UInt> operator>>(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<UInt>(Nucleus::createLShr(lhs.value(), rhs.value()));
}

RValue<UInt> operator+=(UInt &lhs, RValue<UInt> rhs)
{
	return lhs = lhs + rhs;
}

RValue<UInt> operator|=(UInt &lhs, RValue<UInt> rhs)
{
	return lhs = lhs | rhs;
}

RValue<Bool> operator<(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<Bool>(Nucleus::createICmpULT(lhs.value(), rhs.value()));
}

RValue<Bool> operator>(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<Bool>(Nucleus::createICmpUGT(lhs.value(), rhs.value()));
}

RValue<Bool> operator!=(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<Bool>(Nucleus::createICmpNE(lhs.value(), rhs.value()));
}

RValue<Bool> operator==(RValue<UInt> lhs, RValue<UInt> rhs)
{
	return RValue<Bool>(Nucleus::createICmpEQ(lhs.value(), rhs.value()));
}

RValue<UInt> Max(RValue<UInt> x, RValue<UInt> y)
{
	return IfThenElse(x > y, x, y);
}

RValue<UInt> Min(RValue<UInt> x, RValue<UInt> y)
{
	return IfThenElse(x < y, x, y);
}

Int2::Int2(RValue<Int4> cast)
{
	storeValue(Nucleus::createBitCast(cast.value(), type()));
}

Int2::Int2(RValue<Int2> rhs)
{
	store(rhs);
}

Int2::Int2(RValue<Int> lo, RValue<Int> hi)
{
	int shuffle[4] = { 0, 4, 1, 5 };

	Value *packed = Nucleus::createShuffleVector(Int4(lo).loadValue(), Int4(hi).loadValue(), shuffle);

	storeValue(Nucleus::createBitCast(packed, Int2::type()));
}

RValue<Int2> Int2::operator=(RValue<Int2> rhs)
{
	return store(rhs);
}

RValue<Short4> UnpackLow(RValue<Int2> x, RValue<Int2> y)
{
	int shuffle[4] = { 0, 4, 1, 5 };

	return As<Short4>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));
}

RValue<Short4> UnpackHigh(RValue<Int2> x, RValue<Int2> y)
{
	int shuffle[4] = { 0, 4, 1, 5 };

	auto lowHigh = RValue<Int4>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));

	return As<Short4>(Swizzle(lowHigh, 0x2323));
}

RValue<Int> Extract(RValue<Int2> val, int i)
{
	return RValue<Int>(Nucleus::createExtractElement(val.value(), Int::type(), i));
}

RValue<Int2> Insert(RValue<Int2> val, RValue<Int> element, int i)
{
	return RValue<Int2>(Nucleus::createInsertElement(val.value(), element.value(), i));
}

UInt2::UInt2(RValue<UInt2> rhs)
{
	store(rhs);
}

UInt2::UInt2(const Reference<UInt2> &rhs)
{
	store(rhs.load());
}

RValue<UInt2> UInt2::operator=(RValue<UInt2> rhs)
{
	return store(rhs);
}

RValue<UInt2> UInt2::operator=(const Reference<UInt2> &rhs)
{
	return store(rhs.load());
}

RValue<UInt2> operator&(RValue<UInt2> lhs, RValue<UInt2> rhs)
{
	return RValue<UInt2>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<UInt2> operator|(RValue<UInt2> lhs, RValue<UInt2> rhs)
{
	return RValue<UInt2>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<UInt2> operator&=(UInt2 &lhs, RValue<UInt2> rhs)
{
	return lhs = lhs & rhs;
}

RValue<UInt2> operator~(RValue<UInt2> val)
{
	return RValue<UInt2>(Nucleus::createNot(val.value()));
}

Int4::Int4() : XYZW(this)
{
}

Int4::Int4(RValue<Float4> cast) : XYZW(this)
{
	Value *xyzw = Nucleus::createFPToSI(cast.value(), Int4::type());

	storeValue(xyzw);
}

Int4::Int4(int xyzw) : XYZW(this)
{
	constant(xyzw, xyzw, xyzw, xyzw);
}

Int4::Int4(int x, int y, int z, int w) : XYZW(this)
{
	constant(x, y, z, w);
}

Int4::Int4(RValue<Int4> rhs) : XYZW(this)
{
	store(rhs);
}

Int4::Int4(const Int4 &rhs) : XYZW(this)
{
	store(rhs.load());
}

Int4::Int4(const Reference<Int4> &rhs) : XYZW(this)
{
	store(rhs.load());
}

Int4::Int4(RValue<UInt4> rhs) : XYZW(this)
{
	storeValue(rhs.value());
}

Int4::Int4(const Int &rhs) : XYZW(this)
{
	*this = RValue<Int>(rhs.loadValue());
}

RValue<Int4> Int4::operator=(RValue<Int4> rhs)
{
	return store(rhs);
}

RValue<Int4> Int4::operator=(const Int4 &rhs)
{
	return store(rhs.load());
}

RValue<Int4> Int4::operator=(const Reference<Int4> &rhs)
{
	return store(rhs.load());
}

void Int4::constant(int x, int y, int z, int w)
{
	int64_t constantVector[4] = { x, y, z, w };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

RValue<Int4> operator+(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<Int4> operator-(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<Int4> operator*(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createMul(lhs.value(), rhs.value()));
}

RValue<Int4> operator/(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createSDiv(lhs.value(), rhs.value()));
}

RValue<Int4> operator%(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createSRem(lhs.value(), rhs.value()));
}

RValue<Int4> operator&(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<Int4> operator|(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<Int4> operator^(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createXor(lhs.value(), rhs.value()));
}

RValue<Int4> operator<<(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createShl(lhs.value(), rhs.value()));
}

RValue<Int4> operator>>(RValue<Int4> lhs, RValue<Int4> rhs)
{
	return RValue<Int4>(Nucleus::createAShr(lhs.value(), rhs.value()));
}

RValue<Int4> operator+=(Int4 &lhs, RValue<Int4> rhs)
{
	return lhs = lhs + rhs;
}

RValue<Int4> operator-=(Int4 &lhs, RValue<Int4> rhs)
{
	return lhs = lhs - rhs;
}

RValue<Int4> operator*=(Int4 &lhs, RValue<Int4> rhs)
{
	return lhs = lhs * rhs;
}

RValue<Int4> operator&=(Int4 &lhs, RValue<Int4> rhs)
{
	return lhs = lhs & rhs;
}

RValue<Int4> operator^=(Int4 &lhs, RValue<Int4> rhs)
{
	return lhs = lhs ^ rhs;
}

RValue<Int4> operator-(RValue<Int4> val)
{
	return RValue<Int4>(Nucleus::createNeg(val.value()));
}

RValue<Int4> operator~(RValue<Int4> val)
{
	return RValue<Int4>(Nucleus::createNot(val.value()));
}

RValue<Int> Extract(RValue<Int4> x, int i)
{
	return RValue<Int>(Nucleus::createExtractElement(x.value(), Int::type(), i));
}

RValue<Int4> Insert(RValue<Int4> x, RValue<Int> element, int i)
{
	return RValue<Int4>(Nucleus::createInsertElement(x.value(), element.value(), i));
}

RValue<Int4> Swizzle(RValue<Int4> x, uint16_t select)
{
	return RValue<Int4>(createSwizzle4(x.value(), select));
}

UInt4::UInt4() : XYZW(this)
{
}

UInt4::UInt4(int xyzw) : XYZW(this)
{
	constant(xyzw, xyzw, xyzw, xyzw);
}

UInt4::UInt4(RValue<UInt4> rhs) : XYZW(this)
{
	store(rhs);
}

UInt4::UInt4(const UInt4 &rhs) : XYZW(this)
{
	store(rhs.load());
}

UInt4::UInt4(const Reference<UInt4> &rhs) : XYZW(this)
{
	store(rhs.load());
}

UInt4::UInt4(RValue<Int4> rhs) : XYZW(this)
{
	storeValue(rhs.value());
}

UInt4::UInt4(RValue<UInt2> lo, RValue<UInt2> hi) : XYZW(this)
{
	int shuffle[4] = { 0, 1, 4, 5 };

	Value *packed = Nucleus::createShuffleVector(lo.value(), hi.value(), shuffle);

	storeValue(packed);
}

RValue<UInt4> UInt4::operator=(RValue<UInt4> rhs)
{
	return store(rhs);
}

RValue<UInt4> UInt4::operator=(const UInt4 &rhs)
{
	return store(rhs.load());
}

RValue<UInt4> UInt4::operator=(const Reference<UInt4> &rhs)
{
	return store(rhs.load());
}

void UInt4::constant(int x, int y, int z, int w)
{
	int64_t constantVector[4] = { x, y, z, w };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

RValue<UInt4> operator+(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createAdd(lhs.value(), rhs.value()));
}

RValue<UInt4> operator-(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createSub(lhs.value(), rhs.value()));
}

RValue<UInt4> operator*(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createMul(lhs.value(), rhs.value()));
}

RValue<UInt4> operator/(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createUDiv(lhs.value(), rhs.value()));
}

RValue<UInt4> operator%(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createURem(lhs.value(), rhs.value()));
}

RValue<UInt4> operator&(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createAnd(lhs.value(), rhs.value()));
}

RValue<UInt4> operator|(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createOr(lhs.value(), rhs.value()));
}

RValue<UInt4> operator^(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createXor(lhs.value(), rhs.value()));
}

RValue<UInt4> operator>>(RValue<UInt4> lhs, RValue<UInt4> rhs)
{
	return RValue<UInt4>(Nucleus::createLShr(lhs.value(), rhs.value()));
}

RValue<UInt4> operator+=(UInt4 &lhs, RValue<UInt4> rhs)
{
	return lhs = lhs + rhs;
}

RValue<UInt4> operator&=(UInt4 &lhs, RValue<UInt4> rhs)
{
	return lhs = lhs & rhs;
}

RValue<UInt4> operator~(RValue<UInt4> val)
{
	return RValue<UInt4>(Nucleus::createNot(val.value()));
}

Float::Float(RValue<Int> cast)
{
	Value *integer = Nucleus::createSIToFP(cast.value(), Float::type());

	storeValue(integer);
}

Float::Float(RValue<UInt> cast)
{
	RValue<Float> result = Float(Int(cast & UInt(0x7FFFFFFF))) + As<Float>((As<Int>(cast) >> 31) & As<Int>(Float(0x80000000u)));

	storeValue(result.value());
}

Float::Float(float x)
{
	// C++ does not have a way to write an infinite or NaN literal,
	// nor does it allow division by zero as a constant expression.
	// Thus we should not accept inf or NaN as a Reactor Float constant,
	// as this would typically indicate a bug, and avoids undefined
	// behavior.
	//
	// This also prevents the issue of the LLVM JIT only taking double
	// values for constructing floating-point constants. During the
	// conversion from single-precision to double, a signaling NaN can
	// become a quiet NaN, thus altering its bit pattern. Hence this
	// assert is also helpful for detecting cases where integers are
	// being reinterpreted as float and then bitcast to integer again,
	// which does not guarantee preserving the integer value.
	//
	// The inifinity() method can be used to obtain positive infinity.
	ASSERT(std::isfinite(x));

	storeValue(Nucleus::createConstantFloat(x));
}

Float::Float(RValue<Float> rhs)
{
	store(rhs);
}

Float::Float(const Float &rhs)
{
	store(rhs.load());
}

Float::Float(const Reference<Float> &rhs)
{
	store(rhs.load());
}

RValue<Float> Float::operator=(RValue<Float> rhs)
{
	return store(rhs);
}

RValue<Float> Float::operator=(const Float &rhs)
{
	return store(rhs.load());
}

RValue<Float> Float::operator=(const Reference<Float> &rhs)
{
	return store(rhs.load());
}

RValue<Float> operator+(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Float>(Nucleus::createFAdd(lhs.value(), rhs.value()));
}

RValue<Float> operator-(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Float>(Nucleus::createFSub(lhs.value(), rhs.value()));
}

RValue<Float> operator*(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Float>(Nucleus::createFMul(lhs.value(), rhs.value()));
}

RValue<Float> operator/(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Float>(Nucleus::createFDiv(lhs.value(), rhs.value()));
}

RValue<Float> operator+=(Float &lhs, RValue<Float> rhs)
{
	return lhs = lhs + rhs;
}

RValue<Float> operator-=(Float &lhs, RValue<Float> rhs)
{
	return lhs = lhs - rhs;
}

RValue<Float> operator*=(Float &lhs, RValue<Float> rhs)
{
	return lhs = lhs * rhs;
}

RValue<Float> operator-(RValue<Float> val)
{
	return RValue<Float>(Nucleus::createFNeg(val.value()));
}

RValue<Bool> operator<(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Bool>(Nucleus::createFCmpOLT(lhs.value(), rhs.value()));
}

RValue<Bool> operator<=(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Bool>(Nucleus::createFCmpOLE(lhs.value(), rhs.value()));
}

RValue<Bool> operator>(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Bool>(Nucleus::createFCmpOGT(lhs.value(), rhs.value()));
}

RValue<Bool> operator>=(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Bool>(Nucleus::createFCmpOGE(lhs.value(), rhs.value()));
}

RValue<Bool> operator!=(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Bool>(Nucleus::createFCmpONE(lhs.value(), rhs.value()));
}

RValue<Bool> operator==(RValue<Float> lhs, RValue<Float> rhs)
{
	return RValue<Bool>(Nucleus::createFCmpOEQ(lhs.value(), rhs.value()));
}

RValue<Float> Abs(RValue<Float> x)
{
	return IfThenElse(x > 0.0f, x, -x);
}

RValue<Float> Max(RValue<Float> x, RValue<Float> y)
{
	return IfThenElse(x > y, x, y);
}

RValue<Float> Min(RValue<Float> x, RValue<Float> y)
{
	return IfThenElse(x < y, x, y);
}

Float2::Float2(RValue<Float4> cast)
{
	storeValue(Nucleus::createBitCast(cast.value(), type()));
}

Float4::Float4(RValue<Byte4> cast) : XYZW(this)
{
	Value *a = Int4(cast).loadValue();
	Value *xyzw = Nucleus::createSIToFP(a, Float4::type());

	storeValue(xyzw);
}

Float4::Float4(RValue<SByte4> cast) : XYZW(this)
{
	Value *a = Int4(cast).loadValue();
	Value *xyzw = Nucleus::createSIToFP(a, Float4::type());

	storeValue(xyzw);
}

Float4::Float4(RValue<Short4> cast) : XYZW(this)
{
	Int4 c(cast);

	storeValue(Nucleus::createSIToFP(RValue<Int4>(c).value(), Float4::type()));
}

Float4::Float4(RValue<UShort4> cast) : XYZW(this)
{
	Int4 c(cast);

	storeValue(Nucleus::createSIToFP(RValue<Int4>(c).value(), Float4::type()));
}

Float4::Float4(RValue<Int4> cast) : XYZW(this)
{
	Value *xyzw = Nucleus::createSIToFP(cast.value(), Float4::type());

	storeValue(xyzw);
}

Float4::Float4(RValue<UInt4> cast) : XYZW(this)
{
	RValue<Float4> result = Float4(Int4(cast & UInt4(0x7FFFFFFF))) + As<Float4>((As<Int4>(cast) >> 31) & As<Int4>(Float4(0x80000000u)));

	storeValue(result.value());
}

Float4::Float4() : XYZW(this)
{
}

Float4::Float4(float xyzw) : XYZW(this)
{
	constant(xyzw, xyzw, xyzw, xyzw);
}

Float4::Float4(float x, float y, float z, float w) : XYZW(this)
{
	constant(x, y, z, w);
}

Float4::Float4(RValue<Float4> rhs) : XYZW(this)
{
	store(rhs);
}

Float4::Float4(const Float4 &rhs) : XYZW(this)
{
	store(rhs.load());
}

Float4::Float4(const Reference<Float4> &rhs) : XYZW(this)
{
	store(rhs.load());
}

Float4::Float4(const Float &rhs) : XYZW(this)
{
	*this = RValue<Float>(rhs.loadValue());
}

Float4::Float4(const Reference<Float> &rhs) : XYZW(this)
{
	*this = RValue<Float>(rhs.loadValue());
}

RValue<Float4> Float4::operator=(float x)
{
	return *this = Float4(x, x, x, x);
}

RValue<Float4> Float4::operator=(RValue<Float4> rhs)
{
	return store(rhs);
}

RValue<Float4> Float4::operator=(const Float4 &rhs)
{
	return store(rhs.load());
}

RValue<Float4> Float4::operator=(const Reference<Float4> &rhs)
{
	return store(rhs.load());
}

RValue<Float4> Float4::operator=(RValue<Float> rhs)
{
	return *this = Float4(rhs);
}

RValue<Float4> Float4::operator=(const Reference<Float> &rhs)
{
	return *this = Float4(rhs);
}

void Float4::constant(float x, float y, float z, float w)
{
	ASSERT(std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w));

	double constantVector[4] = { x, y, z, w };

	storeValue(Nucleus::createConstantVector(constantVector, type()));
}

RValue<Float4> operator+(RValue<Float4> lhs, RValue<Float4> rhs)
{
	return RValue<Float4>(Nucleus::createFAdd(lhs.value(), rhs.value()));
}

RValue<Float4> operator-(RValue<Float4> lhs, RValue<Float4> rhs)
{
	return RValue<Float4>(Nucleus::createFSub(lhs.value(), rhs.value()));
}

RValue<Float4> operator*(RValue<Float4> lhs, RValue<Float4> rhs)
{
	return RValue<Float4>(Nucleus::createFMul(lhs.value(), rhs.value()));
}

RValue<Float4> operator/(RValue<Float4> lhs, RValue<Float4> rhs)
{
	return RValue<Float4>(Nucleus::createFDiv(lhs.value(), rhs.value()));
}

RValue<Float4> operator+=(Float4 &lhs, RValue<Float4> rhs)
{
	return lhs = lhs + rhs;
}

RValue<Float4> operator-=(Float4 &lhs, RValue<Float4> rhs)
{
	return lhs = lhs - rhs;
}

RValue<Float4> operator*=(Float4 &lhs, RValue<Float4> rhs)
{
	return lhs = lhs * rhs;
}

RValue<Float4> operator/=(Float4 &lhs, RValue<Float4> rhs)
{
	return lhs = lhs / rhs;
}

RValue<Float4> operator-(RValue<Float4> val)
{
	return RValue<Float4>(Nucleus::createFNeg(val.value()));
}

RValue<Float4> Abs(RValue<Float4> x)
{
	Value *vector = Nucleus::createBitCast(x.value(), Int4::type());

	int64_t constantVector[4] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };

	Value *result = Nucleus::createAnd(vector, Nucleus::createConstantVector(constantVector, Int4::type()));

	return As<Float4>(result);
}

RValue<Float4> Insert(RValue<Float4> x, RValue<Float> element, int i)
{
	return RValue<Float4>(Nucleus::createInsertElement(x.value(), element.value(), i));
}

RValue<Float> Extract(RValue<Float4> x, int i)
{
	return RValue<Float>(Nucleus::createExtractElement(x.value(), Float::type(), i));
}

RValue<Float4> Swizzle(RValue<Float4> x, uint16_t select)
{
	return RValue<Float4>(createSwizzle4(x.value(), select));
}

RValue<Float4> ShuffleLowHigh(RValue<Float4> x, RValue<Float4> y, uint16_t imm)
{
	int shuffle[4] = {
		((imm >> 12) & 0x03) + 0,
		((imm >> 8) & 0x03) + 0,
		((imm >> 4) & 0x03) + 4,
		((imm >> 0) & 0x03) + 4,
	};

	return RValue<Float4>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));
}

RValue<Float4> UnpackLow(RValue<Float4> x, RValue<Float4> y)
{
	int shuffle[4] = { 0, 4, 1, 5 };

	return RValue<Float4>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));
}

RValue<Float4> UnpackHigh(RValue<Float4> x, RValue<Float4> y)
{
	int shuffle[4] = { 2, 6, 3, 7 };

	return RValue<Float4>(Nucleus::createShuffleVector(x.value(), y.value(), shuffle));
}

RValue<Float4> Mask(Float4 &lhs, RValue<Float4> rhs, uint16_t select)
{
	Value *vector = lhs.loadValue();
	Value *result = createMask4(vector, rhs.value(), select);

	lhs.storeValue(result);

	return RValue<Float4>(result);
}

RValue<Int4> IsInf(RValue<Float4> x)
{
	return CmpEQ(As<Int4>(x) & Int4(0x7FFFFFFF), Int4(0x7F800000));
}

RValue<Int4> IsNan(RValue<Float4> x)
{
	return ~CmpEQ(x, x);
}

RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, int offset)
{
	return lhs + RValue<Int>(Nucleus::createConstantInt(offset));
}

RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, RValue<Int> offset)
{
	return RValue<Pointer<Byte>>(Nucleus::createGEP(lhs.value(), Byte::type(), offset.value(), false));
}

RValue<Pointer<Byte>> operator+(RValue<Pointer<Byte>> lhs, RValue<UInt> offset)
{
	return RValue<Pointer<Byte>>(Nucleus::createGEP(lhs.value(), Byte::type(), offset.value(), true));
}

RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, int offset)
{
	return lhs = lhs + offset;
}

RValue<Pointer<Byte>> operator+=(Pointer<Byte> &lhs, RValue<Int> offset)
{
	return lhs = lhs + offset;
}

RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, int offset)
{
	return lhs + -offset;
}

RValue<Pointer<Byte>> operator-(RValue<Pointer<Byte>> lhs, RValue<Int> offset)
{
	return lhs + -offset;
}

RValue<Pointer<Byte>> operator-=(Pointer<Byte> &lhs, RValue<Int> offset)
{
	return lhs = lhs - offset;
}

void Return()
{
	Nucleus::createRetVoid();

	Nucleus::setInsertBlock(Nucleus::createBasicBlock());
}

void branch(RValue<Bool> cmp, BasicBlock *bodyBB, BasicBlock *endBB)
{
	Nucleus::createCondBr(cmp.value(), bodyBB, endBB);

	Nucleus::setInsertBlock(bodyBB);
}

#ifdef ENABLE_RR_PRINT
static std::string replaceAll(std::string str, const std::string &substr, const std::string &replacement)
{
	size_t pos = 0;
	while((pos = str.find(substr, pos)) != std::string::npos)
	{
		str.replace(pos, substr.length(), replacement);
		pos += replacement.length();
	}
	return str;
}

void Printv(const char *function, const char *file, int line, const char *fmt, std::initializer_list<PrintValue> args)
{
	// Build the printf format message string.
	std::string str;
	if(file != nullptr) { str += (line > 0) ? "%s:%d " : "%s "; }
	if(function != nullptr) { str += "%s "; }
	str += fmt;

	// Perform substitution on all '{n}' bracketed indices in the format message.
	int i = 0;
	for(const PrintValue &arg : args)
	{
		str = replaceAll(str, "{" + std::to_string(i++) + "}", arg.format);
	}

	std::vector<Value *> vals;
	vals.reserve(8);

	// The format message is always the first argument.
	vals.push_back(Nucleus::createConstantString(str));

	// Add optional file, line and function info if provided.
	if(file != nullptr)
	{
		vals.push_back(Nucleus::createConstantString(file));
		if(line > 0)
		{
			vals.push_back(Nucleus::createConstantInt(line));
		}
	}
	if(function != nullptr)
	{
		vals.push_back(Nucleus::createConstantString(function));
	}

	// Add all format arguments.
	for(const PrintValue &arg : args)
	{
		for(auto val : arg.values)
		{
			vals.push_back(val);
		}
	}

	VPrintf(vals);
}

int DebugPrintf(const char *format, ...)
{
	int result;
	va_list args;

	va_start(args, format);
	char buffer[2048];
	result = vsprintf(buffer, format, args);
	va_end(args);

	std::fputs(buffer, stdout);

	return result;
}
#endif

}
