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

#include "Buffer.h"

#include "main.h"

#include <cstring>

namespace es2 {

Buffer::Buffer(GLuint name) : NamedObject(name)
{
	mContents = 0;
	mSize = 0;
	mUsage = GL_STATIC_DRAW;
	mIsMapped = false;
	mOffset = 0;
	mLength = 0;
	mAccess = 0;
}

Buffer::~Buffer()
{
	if(mContents)
	{
		mContents->destruct();
	}
}

void Buffer::bufferData(const void *data, GLsizeiptr size, GLenum usage)
{
	if(mContents)
	{
		mContents->destruct();
		mContents = 0;
	}

	mSize = size;
	mUsage = usage;

	if(size > 0)
	{
		const int padding = 1024; // For SIMD processing of vertices
		mContents = new sw::Resource(size + padding);

		if(!mContents)
		{
			return error(GL_OUT_OF_MEMORY);
		}

		if(data)
		{
			char *buffer = (char*)mContents->data();
			memcpy(buffer + mOffset, data, size);
		}
	}
}

void Buffer::bufferSubData(const void *data, GLsizeiptr size, GLintptr offset)
{
	if(mContents && data)
	{
		char *buffer = (char*)mContents->lock(sw::PUBLIC);
		memcpy(buffer + offset, data, size);
		mContents->unlock();
	}
}

void *Buffer::mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access)
{
	if(mContents)
	{
		char *buffer = (char*)mContents->lock(sw::PUBLIC);
		mIsMapped = true;
		mOffset = offset;
		mLength = length;
		mAccess = access;
		return buffer + offset;
	}

	return nullptr;
}

bool Buffer::unmap()
{
	if(mContents)
	{
		mContents->unlock();
	}
	mIsMapped = false;
	mOffset = 0;
	mLength = 0;
	mAccess = 0;
	return true;
}

sw::Resource *Buffer::getResource()
{
	return mContents;
}

}
