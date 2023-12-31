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

#include "main.h"

#include "common/debug.h"
#include "Context.h"

static void glAttachThread()
{
	TRACE("()");
}

static void glDetachThread()
{
	TRACE("()");
}

__attribute__((constructor)) static void glAttachProcess()
{
	TRACE("()");

	glAttachThread();
}

__attribute__((destructor)) static void glDetachProcess()
{
	TRACE("()");

	glDetachThread();
}

namespace es2 {

Context *getContextLocked()
{
	egl::Context *context = libEGL->clientGetCurrentContext();

	if(context && (context->getClientVersion() == 2 ||
	               context->getClientVersion() == 3))
	{
		return static_cast<es2::Context*>(context);
	}

	return nullptr;
}

ContextPtr getContext()
{
	return ContextPtr{getContextLocked()};
}

Device *getDevice()
{
	Context *context = getContextLocked();

	return context ? context->getDevice() : nullptr;
}

void error(GLenum errorCode)
{
	es2::Context *context = es2::getContextLocked();

	if(context)
	{
		switch(errorCode)
		{
		case GL_INVALID_ENUM:
			context->recordInvalidEnum();
			TRACE("\t! Error generated: invalid enum\n");
			break;
		case GL_INVALID_VALUE:
			context->recordInvalidValue();
			TRACE("\t! Error generated: invalid value\n");
			break;
		case GL_INVALID_OPERATION:
			context->recordInvalidOperation();
			TRACE("\t! Error generated: invalid operation\n");
			break;
		case GL_OUT_OF_MEMORY:
			context->recordOutOfMemory();
			TRACE("\t! Error generated: out of memory\n");
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			context->recordInvalidFramebufferOperation();
			TRACE("\t! Error generated: invalid framebuffer operation\n");
			break;
		default:
			UNREACHABLE(errorCode);
			break;
		}
	}
}

}

namespace egl {

GLint getClientVersion()
{
	Context *context = libEGL->clientGetCurrentContext();

	return context ? context->getClientVersion() : 0;
}

}
