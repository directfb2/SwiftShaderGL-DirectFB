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

#ifndef sw_Matrix_hpp
#define sw_Matrix_hpp

namespace sw {

struct float4;

struct Matrix
{
	Matrix();
	Matrix(const int i);
	Matrix(float m11, float m12, float m13, float m14,
	       float m21, float m22, float m23, float m24,
	       float m31, float m32, float m33, float m34,
	       float m41, float m42, float m43, float m44);

	float m[4][4];

	Matrix operator!() const; // Inverse
	Matrix operator~() const; // Transpose

	Matrix &operator*=(float s);
	Matrix &operator/=(float s);

	// Access element [row][col], starting with [0][0]
	const float *operator[](int i) const;

	// Access element (row, col), starting with (1, 1)
	float &operator()(int i, int j);
	const float &operator()(int i, int j) const;

	friend Matrix operator*(const Matrix &M, const Matrix &N);
};

inline Matrix::Matrix()
{
}

inline Matrix::Matrix(const int i)
{
	const float s = (float)i;

	Matrix &M = *this;

	M(1, 1) = s; M(1, 2) = 0; M(1, 3) = 0; M(1, 4) = 0;
	M(2, 1) = 0; M(2, 2) = s; M(2, 3) = 0; M(2, 4) = 0;
	M(3, 1) = 0; M(3, 2) = 0; M(3, 3) = s; M(3, 4) = 0;
	M(4, 1) = 0; M(4, 2) = 0; M(4, 3) = 0; M(4, 4) = s;
}

inline Matrix::Matrix(float m11, float m12, float m13, float m14, 
                      float m21, float m22, float m23, float m24, 
                      float m31, float m32, float m33, float m34, 
                      float m41, float m42, float m43, float m44)
{
	Matrix &M = *this;

	M(1, 1) = m11; M(1, 2) = m12; M(1, 3) = m13; M(1, 4) = m14;
	M(2, 1) = m21; M(2, 2) = m22; M(2, 3) = m23; M(2, 4) = m24;
	M(3, 1) = m31; M(3, 2) = m32; M(3, 3) = m33; M(3, 4) = m34;
	M(4, 1) = m41; M(4, 2) = m42; M(4, 3) = m43; M(4, 4) = m44;
}

inline const float *Matrix::operator[](int i) const
{
	return m[i];
}

inline float &Matrix::operator()(int i, int j)
{
	return m[i - 1][j - 1];
}

inline const float &Matrix::operator()(int i, int j) const
{
	return m[i - 1][j - 1];
}

}

#endif
