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

#ifndef sw_Color_hpp
#define sw_Color_hpp

#include "Common/Types.hpp"

namespace sw {

template<class T>
struct Color
{
	Color();
	Color(unsigned short c);
	Color(int c);
	Color(unsigned int c);
	Color(T r, T g, T b, T a = 1);

	operator unsigned int() const;

	Color<T>& operator=(const Color<T>& c);

	Color<T> &operator+=(const Color<T> &c);

	Color<T> &operator*=(float l);

	template<class S>
	friend Color<S> operator+(const Color<S> &c1, const Color<S> &c2);

	template<class S>
	friend Color<S> operator*(float l, const Color<S> &c);

	T r;
	T g;
	T b;
	T a;
};

template<class T>
inline Color<T>::Color()
{
}

template<>
inline Color<byte>::Color(unsigned short c)
{
	r = (byte)(((c & 0xF800) >> 8) + ((c & 0xE000) >> 13));
	g = (byte)(((c & 0x07E0) >> 3) + ((c & 0x0600) >> 9));
	b = (byte)(((c & 0x001F) << 3) + ((c & 0x001C) >> 2));
	a = 0xFF;
}

template<>
inline Color<float>::Color(int c)
{
	const float d = 1.0f / 255.0f;

	r = (float)((c & 0x00FF0000) >> 16) * d;
	g = (float)((c & 0x0000FF00) >> 8) * d;
	b = (float)((c & 0x000000FF) >> 0) * d;
	a = (float)((c & 0xFF000000) >> 24) * d;
}

template<>
inline Color<float>::Color(unsigned int c)
{
	const float d = 1.0f / 255.0f;

	r = (float)((c & 0x00FF0000) >> 16) * d;
	g = (float)((c & 0x0000FF00) >> 8) * d;
	b = (float)((c & 0x000000FF) >> 0) * d;
	a = (float)((c & 0xFF000000) >> 24) * d;
}

template<class T>
inline Color<T>::Color(T r_, T g_, T b_, T a_)
{
	r = r_;
	g = g_;
	b = b_;
	a = a_;
}

template<>
inline Color<byte>::operator unsigned int() const
{
	return (b << 0) +
	       (g << 8) +
	       (r << 16) +
	       (a << 24);
}

template<class T>
inline Color<T> &Color<T>::operator=(const Color& c)
{
	r = c.r;
	g = c.g;
	b = c.b;
	a = c.a;

	return *this;
}

template<class T>
inline Color<T> &Color<T>::operator+=(const Color &c)
{
	r += c.r;
	g += c.g;
	b += c.b;
	a += c.a;

	return *this;
}

template<class T>
inline Color<T> &Color<T>::operator*=(float l)
{
	*this = l * *this;

	return *this;
}

template<class T>
inline Color<T> operator+(const Color<T> &c1, const Color<T> &c2)
{
	return Color<T>(c1.r + c2.r,
	                c1.g + c2.g,
	                c1.b + c2.b,
	                c1.a + c2.a);	
}

template<class T>
inline Color<T> operator*(float l, const Color<T> &c)
{
	T r = (T)(l * c.r);
	T g = (T)(l * c.g);
	T b = (T)(l * c.b);
	T a = (T)(l * c.a);

	return Color<T>(r, g, b, a);
}

}

#endif
