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

#include "Matrix.hpp"

namespace sw {

Matrix Matrix::operator!() const
{
	const Matrix &M = *this;
	Matrix I;

	float M3344 = M(3, 3) * M(4, 4) - M(4, 3) * M(3, 4);
	float M2344 = M(2, 3) * M(4, 4) - M(4, 3) * M(2, 4);
	float M2334 = M(2, 3) * M(3, 4) - M(3, 3) * M(2, 4);
	float M3244 = M(3, 2) * M(4, 4) - M(4, 2) * M(3, 4);
	float M2244 = M(2, 2) * M(4, 4) - M(4, 2) * M(2, 4);
	float M2234 = M(2, 2) * M(3, 4) - M(3, 2) * M(2, 4);
	float M3243 = M(3, 2) * M(4, 3) - M(4, 2) * M(3, 3);
	float M2243 = M(2, 2) * M(4, 3) - M(4, 2) * M(2, 3);
	float M2233 = M(2, 2) * M(3, 3) - M(3, 2) * M(2, 3);
	float M1344 = M(1, 3) * M(4, 4) - M(4, 3) * M(1, 4);
	float M1334 = M(1, 3) * M(3, 4) - M(3, 3) * M(1, 4);
	float M1244 = M(1, 2) * M(4, 4) - M(4, 2) * M(1, 4);
	float M1234 = M(1, 2) * M(3, 4) - M(3, 2) * M(1, 4);
	float M1243 = M(1, 2) * M(4, 3) - M(4, 2) * M(1, 3);
	float M1233 = M(1, 2) * M(3, 3) - M(3, 2) * M(1, 3);
	float M1324 = M(1, 3) * M(2, 4) - M(2, 3) * M(1, 4);
	float M1224 = M(1, 2) * M(2, 4) - M(2, 2) * M(1, 4);
	float M1223 = M(1, 2) * M(2, 3) - M(2, 2) * M(1, 3);

	// Adjoint Matrix
	I(1, 1) =  M(2, 2) * M3344 - M(3, 2) * M2344 + M(4, 2) * M2334;
	I(2, 1) = -M(2, 1) * M3344 + M(3, 1) * M2344 - M(4, 1) * M2334;
	I(3, 1) =  M(2, 1) * M3244 - M(3, 1) * M2244 + M(4, 1) * M2234;
	I(4, 1) = -M(2, 1) * M3243 + M(3, 1) * M2243 - M(4, 1) * M2233;

	I(1, 2) = -M(1, 2) * M3344 + M(3, 2) * M1344 - M(4, 2) * M1334;
	I(2, 2) =  M(1, 1) * M3344 - M(3, 1) * M1344 + M(4, 1) * M1334;
	I(3, 2) = -M(1, 1) * M3244 + M(3, 1) * M1244 - M(4, 1) * M1234;
	I(4, 2) =  M(1, 1) * M3243 - M(3, 1) * M1243 + M(4, 1) * M1233;

	I(1, 3) =  M(1, 2) * M2344 - M(2, 2) * M1344 + M(4, 2) * M1324;
	I(2, 3) = -M(1, 1) * M2344 + M(2, 1) * M1344 - M(4, 1) * M1324;
	I(3, 3) =  M(1, 1) * M2244 - M(2, 1) * M1244 + M(4, 1) * M1224;
	I(4, 3) = -M(1, 1) * M2243 + M(2, 1) * M1243 - M(4, 1) * M1223;

	I(1, 4) = -M(1, 2) * M2334 + M(2, 2) * M1334 - M(3, 2) * M1324;
	I(2, 4) =  M(1, 1) * M2334 - M(2, 1) * M1334 + M(3, 1) * M1324;
	I(3, 4) = -M(1, 1) * M2234 + M(2, 1) * M1234 - M(3, 1) * M1224;
	I(4, 4) =  M(1, 1) * M2233 - M(2, 1) * M1233 + M(3, 1) * M1223;

	// Division by determinant
	I /= M(1, 1) * I(1, 1) +
	     M(2, 1) * I(1, 2) +
	     M(3, 1) * I(1, 3) +
	     M(4, 1) * I(1, 4);

	return I;
}

Matrix Matrix::operator~() const
{
	const Matrix &M = *this;

	return Matrix(M(1, 1), M(2, 1), M(3, 1), M(4, 1), 
	              M(1, 2), M(2, 2), M(3, 2), M(4, 2), 
	              M(1, 3), M(2, 3), M(3, 3), M(4, 3), 
	              M(1, 4), M(2, 4), M(3, 4), M(4, 4));
}

Matrix &Matrix::operator*=(float s)
{
	Matrix &M = *this;

	M(1, 1) *= s; M(1, 2) *= s; M(1, 3) *= s; M(1, 4) *= s;
	M(2, 1) *= s; M(2, 2) *= s; M(2, 3) *= s; M(2, 4) *= s;
	M(3, 1) *= s; M(3, 2) *= s; M(3, 3) *= s; M(3, 4) *= s;
	M(4, 1) *= s; M(4, 2) *= s; M(4, 3) *= s; M(4, 4) *= s;

	return M;
}

Matrix &Matrix::operator/=(float s)
{
	float r = 1.0f / s;

	return *this *= r;
}

Matrix operator*(const Matrix &M, const Matrix &N)
{
	return Matrix(M(1, 1) * N(1, 1) + M(1, 2) * N(2, 1) + M(1, 3) * N(3, 1) + M(1, 4) * N(4, 1), M(1, 1) * N(1, 2) + M(1, 2) * N(2, 2) + M(1, 3) * N(3, 2) + M(1, 4) * N(4, 2), M(1, 1) * N(1, 3) + M(1, 2) * N(2, 3) + M(1, 3) * N(3, 3) + M(1, 4) * N(4, 3), M(1, 1) * N(1, 4) + M(1, 2) * N(2, 4) + M(1, 3) * N(3, 4) + M(1, 4) * N(4, 4), M(2, 1) * N(1, 1) + M(2, 2) * N(2, 1) + M(2, 3) * N(3, 1) + M(2, 4) * N(4, 1), M(2, 1) * N(1, 2) + M(2, 2) * N(2, 2) + M(2, 3) * N(3, 2) + M(2, 4) * N(4, 2), M(2, 1) * N(1, 3) + M(2, 2) * N(2, 3) + M(2, 3) * N(3, 3) + M(2, 4) * N(4, 3), M(2, 1) * N(1, 4) + M(2, 2) * N(2, 4) + M(2, 3) * N(3, 4) + M(2, 4) * N(4, 4), M(3, 1) * N(1, 1) + M(3, 2) * N(2, 1) + M(3, 3) * N(3, 1) + M(3, 4) * N(4, 1), M(3, 1) * N(1, 2) + M(3, 2) * N(2, 2) + M(3, 3) * N(3, 2) + M(3, 4) * N(4, 2), M(3, 1) * N(1, 3) + M(3, 2) * N(2, 3) + M(3, 3) * N(3, 3) + M(3, 4) * N(4, 3), M(3, 1) * N(1, 4) + M(3, 2) * N(2, 4) + M(3, 3) * N(3, 4) + M(3, 4) * N(4, 4), M(4, 1) * N(1, 1) + M(4, 2) * N(2, 1) + M(4, 3) * N(3, 1) + M(4, 4) * N(4, 1), M(4, 1) * N(1, 2) + M(4, 2) * N(2, 2) + M(4, 3) * N(3, 2) + M(4, 4) * N(4, 2), M(4, 1) * N(1, 3) + M(4, 2) * N(2, 3) + M(4, 3) * N(3, 3) + M(4, 4) * N(4, 3), M(4, 1) * N(1, 4) + M(4, 2) * N(2, 4) + M(4, 3) * N(3, 4) + M(4, 4) * N(4, 4));
}

}
