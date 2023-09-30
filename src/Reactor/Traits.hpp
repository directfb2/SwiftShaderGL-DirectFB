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

#ifndef rr_Traits_hpp
#define rr_Traits_hpp

#include <type_traits>

namespace rr {

class Void;
class Int;

template<class T>
class Pointer;

// IsDefined<T>::value is true if T is a valid type, otherwise false.
template<typename T, typename Enable = void>
struct IsDefined
{
	static constexpr bool value = false;
};

template<typename T>
struct IsDefined<T, std::enable_if_t<(sizeof(T) > 0)>>
{
	static constexpr bool value = true;
};

// CToReactorT<T> resolves to the corresponding Reactor type
// for the given C template type T.
template<typename T, typename ENABLE = void>
struct CToReactor;
template<typename T>
using CToReactorT = typename CToReactor<T>::type;

template<>
struct CToReactor<int>
{
	using type = Int;
	static Int cast(int);
};

// ReactorTypeT<T> returns the LValue Reactor type for T.
template<typename T, typename ENABLE = void>
struct ReactorType;
template<typename T>
using ReactorTypeT = typename ReactorType<T>::type;

template<typename T>
struct ReactorType<T, std::enable_if_t<IsDefined<CToReactorT<T>>::value>>
{
	using type = CToReactorT<T>;
	static type cast(T v) { return type(v); }
};

// Reactor types that can be used as a return type for a function.
template<typename T>
struct CanBeUsedAsReturn
{
	static constexpr bool value = false;
};

template<>
struct CanBeUsedAsReturn<Int>
{
	static constexpr bool value = true;
};

template<>
struct CanBeUsedAsReturn<Void>
{
	static constexpr bool value = true;
};

// Reactor types that can be used as a parameter types for a function.
template<typename T>
struct CanBeUsedAsParameter
{
	static constexpr bool value = false;
};

template<>
struct CanBeUsedAsParameter<Int>
{
	static constexpr bool value = true;
};

template<typename T>
struct CanBeUsedAsParameter<Pointer<T>>
{
	static constexpr bool value = true;
};

// AssertParameterTypeIsValid statically asserts that all template parameter
// types can be used as a Reactor function parameter.
template<typename T, typename... other>
struct AssertParameterTypeIsValid : AssertParameterTypeIsValid<other...>
{
	static_assert(CanBeUsedAsParameter<T>::value, "Invalid parameter type");
};

template<typename T>
struct AssertParameterTypeIsValid<T>
{
	static_assert(CanBeUsedAsParameter<T>::value, "Invalid parameter type");
};

// AssertFunctionSignatureIsValid statically asserts that the Reactor
// function signature is valid.
template<typename Return, typename... Arguments>
class AssertFunctionSignatureIsValid;

template<typename Return, typename... Arguments>
class AssertFunctionSignatureIsValid<Return(Arguments...)>
{
	static_assert(CanBeUsedAsReturn<Return>::value, "Invalid return type");
	static_assert(sizeof(AssertParameterTypeIsValid<Arguments...>) >= 0, "");
};

}

#endif
