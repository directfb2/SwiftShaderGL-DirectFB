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

#ifndef DISABLE_DEBUG

#include "debug.h"

#include <cstdarg>
#include <cstdio>

namespace es {

static void output(const char *format, va_list vararg)
{
	static FILE *file = nullptr;

	if(!file)
	{
		file = fopen(TRACE_OUTPUT_FILE, "w");
	}

	if(file)
	{
		vfprintf(file, format, vararg);
	}
}

void trace(const char *format, ...)
{
	va_list vararg;
	va_start(vararg, format);
	output(format, vararg);
	va_end(vararg);
}

}

#endif
