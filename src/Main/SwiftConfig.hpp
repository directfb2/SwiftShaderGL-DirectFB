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

#ifndef sw_SwiftConfig_hpp
#define sw_SwiftConfig_hpp

#include "Common/MutexLock.hpp"
#include "Common/Socket.hpp"
#include "Common/Thread.hpp"
#include "Reactor/Nucleus.hpp"

namespace sw {

using namespace rr;

class SwiftConfig
{
public:
	struct Configuration
	{
		int textureSampleQuality;
		int mipmapQuality;
		bool perspectiveCorrection;
		int threadCount;
		bool enableSSE;
		bool enableSSE2;
		std::array<Optimization::Pass, 10> optimization;
		bool forceWindowed;
		bool complementaryDepthBuffer;
		bool postBlendSRGB;
		bool exactColorRounding;
		int transparencyAntialiasing;
		int shadowMapping;
		bool forceClearRegisters;
		#ifndef DISABLE_DEBUG
		unsigned int minPrimitives;
		unsigned int maxPrimitives;
		#endif
	};

	SwiftConfig(bool disableServerOverride);

	~SwiftConfig();

	bool hasNewConfiguration();
	void getConfiguration(Configuration &configuration);

private:
	enum Status
	{
		OK = 200,
		NotFound = 404
	};

	void createServer();
	void destroyServer();

	static void serverRoutine(void *parameters);

	void serverLoop();
	void respond(Socket *clientSocket, const char *request);
	std::string page();
	std::string profile();
	void send(Socket *clientSocket, Status code, std::string body = "");
	void parsePost(const char *post);

	void readConfiguration();
	void writeConfiguration();

	Configuration config;

	Thread *serverThread;
	volatile bool terminate;
	MutexLock criticalSection; // Protects reading and writing the configuration settings

	bool newConfig;

	Socket *listenSocket;

	int bufferLength;
	char *receiveBuffer;
};

}

#endif
