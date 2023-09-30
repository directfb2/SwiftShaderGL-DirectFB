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

#include "CPUID.hpp"

#include <unistd.h>

namespace sw {

bool CPUID::SSE = detectSSE();
bool CPUID::SSE2 = detectSSE2();
int CPUID::cores = detectCoreCount();
int CPUID::affinity = detectAffinity();

bool CPUID::enableSSE = true;
bool CPUID::enableSSE2 = true;

void CPUID::setEnableSSE(bool enable)
{
	enableSSE = enable;

	if(!enableSSE)
	{
		enableSSE2 = false;
	}
}

void CPUID::setEnableSSE2(bool enable)
{
	enableSSE2 = enable;

	if(enableSSE2)
	{
		enableSSE = true;
	}
}

static void cpuid(int registers[4], int info)
{
	#if defined(__i386__) || defined(__x86_64__)
	__asm volatile("cpuid": "=a" (registers[0]), "=b" (registers[1]), "=c" (registers[2]), "=d" (registers[3]): "a" (info));
	#else
	registers[0] = 0;
	registers[1] = 0;
	registers[2] = 0;
	registers[3] = 0;
	#endif
}

bool CPUID::detectSSE()
{
	int registers[4];
	cpuid(registers, 1);
	return SSE = (registers[3] & 0x02000000) != 0;
}

bool CPUID::detectSSE2()
{
	int registers[4];
	cpuid(registers, 1);
	return SSE2 = (registers[3] & 0x04000000) != 0;
}

int CPUID::detectCoreCount()
{
	int cores = 0;

	cores = sysconf(_SC_NPROCESSORS_ONLN);

	if(cores < 1)  cores = 1;
	if(cores > 16) cores = 16;

	return cores;
}

int CPUID::detectAffinity()
{
	return detectCoreCount();
}

}
