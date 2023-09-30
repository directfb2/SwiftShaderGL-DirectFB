// Copyright 2020 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef ENABLE_RR_DEBUG_INFO

#include "ReactorDebugInfo.hpp"

#ifdef ENABLE_RR_PRINT
#include "Print.hpp"
#endif

#include <algorithm>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>

namespace rr {

namespace {

std::string to_lower(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });

	return str;
}

bool endswith_lower(const std::string &str, const std::string &suffix)
{
	size_t strLen = str.size();
	size_t suffixLen = suffix.size();

	if(strLen < suffixLen)
	{
		return false;
	}

	return to_lower(str).substr(strLen - suffixLen) == to_lower(suffix);
}

}

Backtrace getCallerBacktrace(size_t limit)
{
	void *stacktrace[128];
	int frames = backtrace(stacktrace, sizeof(stacktrace) / sizeof(stacktrace[0]));

	auto shouldSkipFile = [](const std::string &fileSR)
	{
		return fileSR.empty() ||
		       endswith_lower(fileSR, "ReactorDebugInfo.cpp") ||
		       endswith_lower(fileSR, "Reactor.cpp") ||
		       endswith_lower(fileSR, "Reactor.hpp") ||
		       endswith_lower(fileSR, "Traits.hpp");
	};

	std::vector<Location> locations;

	for(int frame = 0; frame < frames; ++frame)
	{
		Location location;

		Dl_info info;
		if (!dladdr(stacktrace[frame], &info))
			continue;

		int status;
		char *demangled_name = abi::__cxa_demangle(info.dli_sname, 0, 0, &status);
		if (status)
			continue;

		location.function.file = info.dli_fname;
		location.function.name = demangled_name;

		free(demangled_name);

		if(shouldSkipFile(location.function.file))
		{
			continue;
		}

		locations.push_back(location);

		if(limit > 0 && locations.size() >= limit)
		{
			break;
		}
	}

	std::reverse(locations.begin(), locations.end());

	return locations;
}

void emitPrintLocation(const Backtrace &backtrace)
{
#ifdef ENABLE_RR_PRINT
	static Location lastLocation;

	if(backtrace.size() == 0)
	{
		return;
	}

	Location currLocation = backtrace[backtrace.size() - 1];

	if(currLocation != lastLocation)
	{
		rr::Print("rr> {0} [{1}:{2}]\n", currLocation.function.name.c_str(), currLocation.function.file.c_str(), currLocation.line);
	}
#endif
}

}

#endif
