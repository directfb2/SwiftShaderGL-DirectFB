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

#ifndef sw_CPUID_hpp
#define sw_CPUID_hpp

namespace sw {

class CPUID
{
public:
	static bool supportsSSE();
	static bool supportsSSE2();
	static int coreCount();
	static int processAffinity();

	static void setEnableSSE(bool enable);
	static void setEnableSSE2(bool enable);

private:
	static bool SSE;
	static bool SSE2;
	static int cores;
	static int affinity;

	static bool enableSSE;
	static bool enableSSE2;

	static bool detectSSE();
	static bool detectSSE2();
	static int detectCoreCount();
	static int detectAffinity();
};

inline bool CPUID::supportsSSE()
{
	return SSE && enableSSE;
}

inline bool CPUID::supportsSSE2()
{
	return SSE2 && enableSSE2;
}

inline int CPUID::coreCount()
{
	return cores;
}

inline int CPUID::processAffinity()
{
	return affinity;
}

}

#endif
