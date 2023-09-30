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

#include "SwiftConfig.hpp"

#include "Common/Configurator.hpp"
#include "Common/Debug.hpp"
#include "Config.hpp"

#include <cstring>
#include <sstream>
#include <sys/stat.h>

namespace sw {

extern Profiler profiler;

std::string itoa(int number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

std::string ftoa(double number)
{
	std::stringstream ss;
	ss << number;
	return ss.str();
}

SwiftConfig::SwiftConfig(bool disableServer) : listenSocket(0)
{
	readConfiguration();

	if(!disableServer)
	{
		writeConfiguration();
	}

	receiveBuffer = 0;

	if(!disableServer)
	{
		createServer();
	}
}

SwiftConfig::~SwiftConfig()
{
	destroyServer();
}

void SwiftConfig::createServer()
{
	bufferLength = 16 * 1024;
	receiveBuffer = new char[bufferLength];

	listenSocket = new Socket("localhost", "8080");
	listenSocket->listen();

	terminate = false;
	serverThread = new Thread(serverRoutine, this);
}

void SwiftConfig::destroyServer()
{
	if(receiveBuffer)
	{
		terminate = true;
		serverThread->join();
		delete serverThread;

		delete listenSocket;
		listenSocket = 0;

		delete[] receiveBuffer;
		receiveBuffer = 0;
	}
}

bool SwiftConfig::hasNewConfiguration()
{
	return newConfig;
}

void SwiftConfig::getConfiguration(Configuration &configuration)
{
	criticalSection.lock();
	configuration = config;
	criticalSection.unlock();
}

void SwiftConfig::serverRoutine(void *parameters)
{
	SwiftConfig *swiftConfig = (SwiftConfig*)parameters;

	swiftConfig->serverLoop();
}

void SwiftConfig::serverLoop()
{
	readConfiguration();

	while(!terminate)
	{
		if(listenSocket->select(100000))
		{
			Socket *clientSocket = listenSocket->accept();
			int bytesReceived = 1;

			while(bytesReceived > 0 && !terminate)
			{
				if(clientSocket->select(10))
				{
					bytesReceived = clientSocket->receive(receiveBuffer, bufferLength);

					if(bytesReceived > 0)
					{
						receiveBuffer[bytesReceived] = 0;

						respond(clientSocket, receiveBuffer);
					}
				}
			}

			delete clientSocket;
		}
	}
}

bool match(const char **url, const char *string)
{
	size_t length = strlen(string);

	if(strncmp(*url, string, length) == 0)
	{
		*url += length;

		return true;
	}

	return false;
}

void SwiftConfig::respond(Socket *clientSocket, const char *request)
{
	if(match(&request, "GET /"))
	{
		if(match(&request, "swiftshader") || match(&request, "swiftconfig"))
		{
			if(match(&request, " ") || match(&request, "/ "))
			{
				return send(clientSocket, OK, page());
			}
		}
	}
	else if(match(&request, "POST /"))
	{
		if(match(&request, "swiftshader") || match(&request, "swiftconfig"))
		{
			if(match(&request, " ") || match(&request, "/ "))
			{
				criticalSection.lock();

				const char *postData = strstr(request, "\r\n\r\n");
				postData = postData ? postData + 4 : 0;

				if(postData && strlen(postData) > 0)
				{
					parsePost(postData);
				}
				else // POST data in next packet
				{
					int bytesReceived = clientSocket->receive(receiveBuffer, bufferLength);

					if(bytesReceived > 0)
					{
						receiveBuffer[bytesReceived] = 0;
						parsePost(receiveBuffer);
					}
				}
				writeConfiguration();
				newConfig = true;

				criticalSection.unlock();

				return send(clientSocket, OK, page());
			}
			else if(match(&request, "/profile "))
			{
				return send(clientSocket, OK, profile());
			}
		}
	}

	return send(clientSocket, NotFound);
}

std::string SwiftConfig::page()
{
	std::string html;

	const std::string selected = "selected='selected'";
	const std::string checked = "checked='checked'";
	const std::string empty = "";

	html += "<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01//EN' 'http://www.w3.org/TR/html4/strict.dtd'>\n";
	html += "<html>\n";
	html += "<head>\n";
	html += "<meta http-equiv='content-type' content='text/html; charset=UTF-8'>\n";
	html += "<title>SwiftShader Configuration Panel</title>\n";
	html += "</head>\n";
	html += "<body>\n";
	html += "<script type='text/javascript'>\n";
	html += "request();\n";
	html += "function request()\n";
	html += "{\n";
	html += "var xhr = new XMLHttpRequest();\n";
	html += "xhr.open('POST', '/swiftshader/profile', true);\n";
	html += "xhr.onreadystatechange = function()\n";
	html += "{\n";
	html += "if(xhr.readyState == 4 && xhr.status == 200)\n";
	html += "{\n";
	html += "document.getElementById('profile').innerHTML = xhr.responseText;\n";
	html += "setTimeout('request()', 1000);\n";
	html += "}\n";
	html += "}\n";
	html += "xhr.send();\n";
	html += "}\n";
	html += "</script>\n";
	html += "<form method='POST' action=''>\n";
	html += "<h1>SwiftShader Configuration Panel</h1>\n";
	html += "<div id='profile'>" + profile() + "</div>\n";
	html += "<hr><p>\n";
	html += "<input type='submit' value='Apply changes' title='Click to apply all settings.'>\n";
	html += "</p><hr>\n";

	html += "<h2><em>Quality</em></h2>\n";
	html += "<table>\n";
	html += "<tr><td>Maximum texture sampling quality:</td><td><select name='textureSampleQuality' title='The maximum texture filtering quality. Lower settings can be faster but cause visual artifacts.'>\n";
	html += "<option value='0'" + (config.textureSampleQuality == 0 ? selected : empty) + ">Point</option>\n";
	html += "<option value='1'" + (config.textureSampleQuality == 1 ? selected : empty) + ">Linear</option>\n";
	html += "<option value='2'" + (config.textureSampleQuality == 2 ? selected : empty) + ">Anisotropic (default)</option>\n";
	html += "</select></td>\n";
	html += "</tr>\n";
	html += "<tr><td>Maximum mipmapping quality:</td><td><select name='mipmapQuality' title='The maximum mipmap filtering quality. Higher settings can be more visually appealing but are slower.'>\n";
	html += "<option value='0'" + (config.mipmapQuality == 0 ? selected : empty) + ">Point</option>\n";
	html += "<option value='1'" + (config.mipmapQuality == 1 ? selected : empty) + ">Linear (default)</option>\n";
	html += "</select></td>\n";
	html += "</tr>\n";
	html += "<tr><td>Perspective correction:</td><td><select name='perspectiveCorrection' title='Enables or disables perspective correction. Disabling it is faster but can causes distortion. Recommended for 2D applications only.'>\n";
	html += "<option value='0'" + (config.perspectiveCorrection == 0 ? selected : empty) + ">Off</option>\n";
	html += "<option value='1'" + (config.perspectiveCorrection == 1 ? selected : empty) + ">On (default)</option>\n";
	html += "</select></td>\n";
	html += "</tr>\n";
	html += "<tr><td>Transparency anti-aliasing:</td><td><select name='transparencyAntialiasing' title='The technique used to anti-alias alpha-tested transparent textures.'>\n";
	html += "<option value='0'" + (config.transparencyAntialiasing == 0 ? selected : empty) + ">None (default)</option>\n";
	html += "<option value='1'" + (config.transparencyAntialiasing == 1 ? selected : empty) + ">Alpha-to-Coverage</option>\n";
	html += "</select></td>\n";
	html += "</table>\n";

	html += "<h2><em>Processor settings</em></h2>\n";
	html += "<table>\n";
	html += "<tr><td>Number of threads:</td><td><select name='threadCount' title='The number of rendering threads to be used.'>\n";
	html += "<option value='-1'" + (config.threadCount == -1 ? selected : empty) + ">Core count</option>\n";
	html += "<option value='0'"  + (config.threadCount == 0  ? selected : empty) + ">Process affinity (default)</option>\n";
	html += "<option value='1'"  + (config.threadCount == 1  ? selected : empty) + ">1</option>\n";
	html += "<option value='2'"  + (config.threadCount == 2  ? selected : empty) + ">2</option>\n";
	html += "<option value='3'"  + (config.threadCount == 3  ? selected : empty) + ">3</option>\n";
	html += "<option value='4'"  + (config.threadCount == 4  ? selected : empty) + ">4</option>\n";
	html += "<option value='5'"  + (config.threadCount == 5  ? selected : empty) + ">5</option>\n";
	html += "<option value='6'"  + (config.threadCount == 6  ? selected : empty) + ">6</option>\n";
	html += "<option value='7'"  + (config.threadCount == 7  ? selected : empty) + ">7</option>\n";
	html += "<option value='8'"  + (config.threadCount == 8  ? selected : empty) + ">8</option>\n";
	html += "<option value='9'"  + (config.threadCount == 9  ? selected : empty) + ">9</option>\n";
	html += "<option value='10'" + (config.threadCount == 10 ? selected : empty) + ">10</option>\n";
	html += "<option value='11'" + (config.threadCount == 11 ? selected : empty) + ">11</option>\n";
	html += "<option value='12'" + (config.threadCount == 12 ? selected : empty) + ">12</option>\n";
	html += "<option value='13'" + (config.threadCount == 13 ? selected : empty) + ">13</option>\n";
	html += "<option value='14'" + (config.threadCount == 14 ? selected : empty) + ">14</option>\n";
	html += "<option value='15'" + (config.threadCount == 15 ? selected : empty) + ">15</option>\n";
	html += "<option value='16'" + (config.threadCount == 16 ? selected : empty) + ">16</option>\n";
	html += "</select></td></tr>\n";
	html += "<tr><td>Enable SSE:</td><td><input name = 'enableSSE' type='checkbox'" + (config.enableSSE ? checked : empty) + " title='If checked enables the use of SSE instruction set extentions if supported by the CPU.'></td></tr>";
	html += "<tr><td>Enable SSE2:</td><td><input name = 'enableSSE2' type='checkbox'" + (config.enableSSE2 ? checked : empty) + " title='If checked enables the use of SSE2 instruction set extentions if supported by the CPU.'></td></tr>";
	html += "</table>\n";

	html += "<h2><em>Compiler optimizations</em></h2>\n";
	html += "<table>\n";
	for(size_t pass = 0; pass < config.optimization.size(); pass++)
	{
		html += "<tr><td>Optimization pass " + itoa(pass + 1) + ":</td><td><select name='optimization" + itoa(pass + 1) + "' title='An optimization pass for the shader compiler.'>\n";
		html += "<option value='0'"   + (config.optimization[pass] == Optimization::Pass::Disabled ? selected : empty) + ">Disabled" + (pass > 0 ? " (default)" : "") + "</option>\n";
		html += "<option value='1'"   + (config.optimization[pass] == Optimization::Pass::InstructionCombining ? selected : empty) + ">Instruction Combining" + (pass == 0 ? " (default)" : "") + "</option>\n";
		html += "<option value='2'"   + (config.optimization[pass] == Optimization::Pass::CFGSimplification ? selected : empty) + ">Control Flow Simplification</option>\n";
		html += "<option value='3'"   + (config.optimization[pass] == Optimization::Pass::LICM ? selected : empty) + ">Loop Invariant Code Motion</option>\n";
		html += "<option value='4'"   + (config.optimization[pass] == Optimization::Pass::AggressiveDCE ? selected : empty) + ">Aggressive Dead Code Elimination</option>\n";
		html += "<option value='5'"   + (config.optimization[pass] == Optimization::Pass::GVN ? selected : empty) + ">Global Value Numbering</option>\n";
		html += "<option value='6'"   + (config.optimization[pass] == Optimization::Pass::Reassociate ? selected : empty) + ">Commutative Expressions Reassociation</option>\n";
		html += "<option value='7'"   + (config.optimization[pass] == Optimization::Pass::DeadStoreElimination ? selected : empty) + ">Dead Store Elimination</option>\n";
		html += "<option value='8'"   + (config.optimization[pass] == Optimization::Pass::SCCP ? selected : empty) + ">Sparse Conditional Copy Propagation</option>\n";
		html += "<option value='9'"   + (config.optimization[pass] == Optimization::Pass::ScalarReplAggregates ? selected : empty) + ">Scalar Replacement of Aggregates</option>\n";
		html += "<option value='10'"  + (config.optimization[pass] == Optimization::Pass::EarlyCSEPass ? selected : empty) + ">Eliminate trivially redundant instructions</option>\n";
		html += "</select></td></tr>\n";
	}
	html += "</table>\n";

	html += "<h2><em>Testing</em></h2>\n";
	html += "<table>\n";
	html += "<tr><td>Force windowed mode:</td><td><input name = 'forceWindowed' type='checkbox'" + (config.forceWindowed == true ? checked : empty) + " title='If checked prevents the application from switching to full-screen mode.'></td></tr>";
	html += "<tr><td>Complementary depth buffer:</td><td><input name = 'complementaryDepthBuffer' type='checkbox'" + (config.complementaryDepthBuffer == true ? checked : empty) + " title='If checked causes 1 - z to be stored in the depth buffer.'></td></tr>";
	html += "<tr><td>Post alpha blend sRGB conversion:</td><td><input name = 'postBlendSRGB' type='checkbox'" + (config.postBlendSRGB == true ? checked : empty) + " title='If checked alpha blending is performed in linear color space.'></td></tr>";
	html += "<tr><td>Exact color rounding:</td><td><input name = 'exactColorRounding' type='checkbox'" + (config.exactColorRounding == true ? checked : empty) + " title='If checked color rounding is done at high accuracy.'></td></tr>";
	html += "<tr><td>Force clearing registers that have no default value:</td><td><input name = 'forceClearRegisters' type='checkbox'" + (config.forceClearRegisters == true ? checked : empty) + " title='Initializes shader register values to 0 even if they have no default.'></td></tr>";
	html += "</table>\n";

	#ifndef DISABLE_DEBUG
	html += "<h2><em>Debugging</em></h2>\n";
	html += "<table>\n";
	html += "<tr><td>Minimum primitives:</td><td><input type='text' size='10' maxlength='10' name='minPrimitives' value='" + itoa(config.minPrimitives) + "'></td></tr>\n";
	html += "<tr><td>Maximum primitives:</td><td><input type='text' size='10' maxlength='10' name='maxPrimitives' value='" + itoa(config.maxPrimitives) + "'></td></tr>\n";
	html += "</table>\n";
	#endif

	html += "<hr><p>\n";
	html += "<span style='font-size:10pt'>Removing the SwiftShader.ini file results in resetting the options to their default.</span></p>\n";
	html += "</form>\n";
	html += "</body>\n";
	html += "</html>\n";

	profiler.reset();

	return html;
}

std::string SwiftConfig::profile()
{
	std::string html;

	html += "<p>FPS: " + ftoa(profiler.FPS) + "</p>\n";
	html += "<p>Frame: " + itoa(profiler.framesTotal) + "</p>\n";

	#if PERF_PROFILE
	int texTime = (int)(1000 * profiler.cycles[PERF_TEX] / profiler.cycles[PERF_PIXEL] + 0.5);
	int shaderTime = (int)(1000 * profiler.cycles[PERF_SHADER] / profiler.cycles[PERF_PIXEL] + 0.5);
	int pipeTime = (int)(1000 * profiler.cycles[PERF_PIPE] / profiler.cycles[PERF_PIXEL] + 0.5);
	int ropTime = (int)(1000 * profiler.cycles[PERF_ROP] / profiler.cycles[PERF_PIXEL] + 0.5);
	int interpTime = (int)(1000 * profiler.cycles[PERF_INTERP] / profiler.cycles[PERF_PIXEL] + 0.5);
	int rastTime = 1000 - pipeTime;

	pipeTime -= shaderTime + ropTime + interpTime;
	shaderTime -= texTime;

	double texTimeF = (double)texTime / 10;
	double shaderTimeF = (double)shaderTime / 10;
	double pipeTimeF = (double)pipeTime / 10;
	double ropTimeF = (double)ropTime / 10;
	double interpTimeF = (double)interpTime / 10;
	double rastTimeF = (double)rastTime / 10;

	html += "<div id='profile' style='position:relative; width:1010px; height:50px; background-color:silver;'>";
	html += "<div style='position:relative; width:1000px; height:40px; background-color:white; left:5px; top:5px;'>";
	html += "<div style='position:relative; float:left; width:" + itoa(rastTime)   + "px; height:40px; border-style:none; text-align:center; line-height:40px; background-color:#FFFF7F; overflow:hidden;'>" + ftoa(rastTimeF)   + "% rast</div>\n";
	html += "<div style='position:relative; float:left; width:" + itoa(pipeTime)   + "px; height:40px; border-style:none; text-align:center; line-height:40px; background-color:#FF7F7F; overflow:hidden;'>" + ftoa(pipeTimeF)   + "% pipe</div>\n";
	html += "<div style='position:relative; float:left; width:" + itoa(interpTime) + "px; height:40px; border-style:none; text-align:center; line-height:40px; background-color:#7FFFFF; overflow:hidden;'>" + ftoa(interpTimeF) + "% interp</div>\n";
	html += "<div style='position:relative; float:left; width:" + itoa(shaderTime) + "px; height:40px; border-style:none; text-align:center; line-height:40px; background-color:#7FFF7F; overflow:hidden;'>" + ftoa(shaderTimeF) + "% shader</div>\n";
	html += "<div style='position:relative; float:left; width:" + itoa(texTime)    + "px; height:40px; border-style:none; text-align:center; line-height:40px; background-color:#FF7FFF; overflow:hidden;'>" + ftoa(texTimeF)    + "% tex</div>\n";
	html += "<div style='position:relative; float:left; width:" + itoa(ropTime)    + "px; height:40px; border-style:none; text-align:center; line-height:40px; background-color:#7F7FFF; overflow:hidden;'>" + ftoa(ropTimeF)    + "% rop</div>\n";
	html += "</div></div>\n";

	for(int i = 0; i < PERF_TIMERS; i++)
	{
		profiler.cycles[i] = 0;
	}
	#endif

	return html;
}

void SwiftConfig::send(Socket *clientSocket, Status code, std::string body)
{
	std::string status;
	char header[1024];

	switch(code)
	{
	case OK:       status += "HTTP/1.1 200 OK\r\n";        break;
	case NotFound: status += "HTTP/1.1 404 Not Found\r\n"; break;
	}

	sprintf(header, "Content-Type: text/html; charset=UTF-8\r\n"
					"Content-Length: %zd\r\n"
					"Host: localhost\r\n"
					"\r\n", body.size());

	std::string message = status + header + body;
	clientSocket->send(message.c_str(), (int)message.length());
}

void SwiftConfig::parsePost(const char *post)
{
	// Only enabled checkboxes appear in the POST
	config.enableSSE = false;
	config.enableSSE2 = false;
	config.forceWindowed = false;
	config.complementaryDepthBuffer = false;
	config.postBlendSRGB = false;
	config.exactColorRounding = false;
	config.forceClearRegisters = false;

	while(*post != 0)
	{
		int integer;
		int index;

		if(sscanf(post, "textureSampleQuality=%d", &integer))
		{
			config.textureSampleQuality = integer;
		}
		else if(sscanf(post, "mipmapQuality=%d", &integer))
		{
			config.mipmapQuality = integer;
		}
		else if(sscanf(post, "perspectiveCorrection=%d", &integer))
		{
			config.perspectiveCorrection = integer != 0;
		}
		else if(sscanf(post, "transparencyAntialiasing=%d", &integer))
		{
			config.transparencyAntialiasing = integer;
		}
		else if(sscanf(post, "threadCount=%d", &integer))
		{
			config.threadCount = integer;
		}
		else if(strstr(post, "enableSSE=on"))
		{
			config.enableSSE = true;
		}
		else if(strstr(post, "enableSSE2=on"))
		{
			if(config.enableSSE)
			{
				config.enableSSE2 = true;
			}
		}
		else if(sscanf(post, "optimization%d=%d", &index, &integer))
		{
			config.optimization[index - 1] = (Optimization::Pass)integer;
		}
		else if(strstr(post, "forceWindowed=on"))
		{
			config.forceWindowed = true;
		}
		else if(strstr(post, "complementaryDepthBuffer=on"))
		{
			config.complementaryDepthBuffer = true;
		}
		else if(strstr(post, "postBlendSRGB=on"))
		{
			config.postBlendSRGB = true;
		}
		else if(strstr(post, "exactColorRounding=on"))
		{
			config.exactColorRounding = true;
		}
		else if(strstr(post, "forceClearRegisters=on"))
		{
			config.forceClearRegisters = true;
		}
		#ifndef DISABLE_DEBUG
		else if(sscanf(post, "minPrimitives=%d", &integer))
		{
			config.minPrimitives = integer;
		}
		else if(sscanf(post, "maxPrimitives=%d", &integer))
		{
			config.maxPrimitives = integer;
		}
		#endif
		else
		{
			ASSERT(false);
		}

		do
		{
			post++;
		}
		while(post[-1] != '&' && *post != 0);
	}
}

void SwiftConfig::readConfiguration()
{
	Configurator ini("SwiftShader.ini");

	config.textureSampleQuality = ini.getInteger("Quality", "TextureSampleQuality", 2);
	config.mipmapQuality = ini.getInteger("Quality", "MipmapQuality", 1);
	config.perspectiveCorrection = ini.getBoolean("Quality", "PerspectiveCorrection", true);
	config.transparencyAntialiasing = ini.getInteger("Quality", "TransparencyAntialiasing", 0);
	config.threadCount = ini.getInteger("Processor", "ThreadCount", DEFAULT_THREAD_COUNT);
	config.enableSSE = ini.getBoolean("Processor", "EnableSSE", true);
	config.enableSSE2 = ini.getBoolean("Processor", "EnableSSE2", true);

	for(size_t pass = 0; pass < config.optimization.size(); pass++)
	{
		auto def = pass == 0 ? Optimization::Pass::InstructionCombining : Optimization::Pass::Disabled;
		config.optimization[pass] = (Optimization::Pass)ini.getInteger("Optimization", "OptimizationPass" + itoa(pass + 1), (int)def);
	}

	config.forceWindowed = ini.getBoolean("Testing", "ForceWindowed", false);
	config.complementaryDepthBuffer = ini.getBoolean("Testing", "ComplementaryDepthBuffer", false);
	config.postBlendSRGB = ini.getBoolean("Testing", "PostBlendSRGB", false);
	config.exactColorRounding = ini.getBoolean("Testing", "ExactColorRounding", true);
	config.forceClearRegisters = ini.getBoolean("Testing", "ForceClearRegisters", false);

	#ifndef DISABLE_DEBUG
	config.minPrimitives = 1;
	config.maxPrimitives = 1 << 21;
	#endif

	struct stat status;
	int lastModified = ini.getInteger("LastModified", "Time", 0);

	newConfig = stat("SwiftShader.ini", &status) == 0 && abs((int)status.st_mtime - lastModified) > 1;
}

void SwiftConfig::writeConfiguration()
{
	Configurator ini("SwiftShader.ini");

	ini.addValue("Quality", "TextureSampleQuality", itoa(config.textureSampleQuality));
	ini.addValue("Quality", "MipmapQuality", itoa(config.mipmapQuality));
	ini.addValue("Quality", "PerspectiveCorrection", itoa(config.perspectiveCorrection));
	ini.addValue("Quality", "TransparencyAntialiasing", itoa(config.transparencyAntialiasing));

	ini.addValue("Processor", "ThreadCount", itoa(config.threadCount));
	ini.addValue("Processor", "EnableSSE", itoa(config.enableSSE));
	ini.addValue("Processor", "EnableSSE2", itoa(config.enableSSE2));

	for(size_t pass = 0; pass < config.optimization.size(); pass++)
	{
		ini.addValue("Optimization", "OptimizationPass" + itoa(pass + 1), itoa((int)config.optimization[pass]));
	}

	ini.addValue("Testing", "ForceWindowed", itoa(config.forceWindowed));
	ini.addValue("Testing", "ComplementaryDepthBuffer", itoa(config.complementaryDepthBuffer));
	ini.addValue("Testing", "PostBlendSRGB", itoa(config.postBlendSRGB));
	ini.addValue("Testing", "ExactColorRounding", itoa(config.exactColorRounding));
	ini.addValue("Testing", "ForceClearRegisters", itoa(config.forceClearRegisters));

	ini.addValue("LastModified", "Time", itoa((int)time(0)));

	ini.writeFile("SwiftShader Configuration File\n"
	              ";\n"
				  "; To get an overview of the valid settings and their meaning,\n"
				  "; run the application in windowed mode and open the\n"
				  "; SwiftConfig application or go to http://localhost:8080/swiftconfig.");
}

}
