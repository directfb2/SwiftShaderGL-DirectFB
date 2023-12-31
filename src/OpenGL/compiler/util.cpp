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

#include "util.h"

#include "preprocessor/numeric_lex.h"

#include <limits>

bool atof_clamp(const char *str, float *value)
{
	bool success = pp::numeric_lex_float(str, value);
	if(!success)
		*value = std::numeric_limits<float>::max();

	return success;
}

bool atoi_clamp(const char *str, int *value)
{
	bool success = pp::numeric_lex_int(str, value);
	if(!success)
		*value = std::numeric_limits<int>::max();

	return success;
}

bool atou_clamp(const char *str, unsigned int *value)
{
	bool success = pp::numeric_lex_int(str, value);
	if(!success)
		*value = std::numeric_limits<unsigned int>::max();

	return success;
}
