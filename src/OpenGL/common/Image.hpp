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

#ifndef Image_hpp
#define Image_hpp

#include "libEGL/Texture.hpp"
#include "Renderer/Surface.hpp"

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

namespace gl {

struct PixelStorageModes
{
	GLint rowLength = 0;
	GLint skipRows = 0;
	GLint skipPixels = 0;
	GLint alignment = 4;
	GLint imageHeight = 0;
	GLint skipImages = 0;
};

GLint GetSizedInternalFormat(GLint internalFormat, GLenum type);
sw::Format SelectInternalFormat(GLint format);
bool IsUnsizedInternalFormat(GLint internalformat);
GLenum GetBaseInternalFormat(GLint internalformat);
GLsizei ComputePitch(GLsizei width, GLenum format, GLenum type, GLint alignment);
GLsizei ComputeCompressedSize(GLsizei width, GLsizei height, GLenum format);
GLsizei ComputePixelSize(GLenum format, GLenum type);
size_t ComputePackingOffset(GLenum format, GLenum type, GLsizei width, GLsizei height, const PixelStorageModes &storageModes);

}

namespace egl {

class ClientBuffer
{
public:
	ClientBuffer(int width, int height, sw::Format format, void *buffer, size_t plane) :
		width(width), height(height), format(format), buffer(buffer), plane(plane) {}

	int getWidth() const;
	int getHeight() const;
	sw::Format getFormat() const;
	size_t getPlane() const;
	int pitchP() const;
	void retain();
	void release();
	void *lock(int x, int y, int z);
	void unlock();
	bool requiresSync() const;

private:
	int width;
	int height;
	sw::Format format;
	void *buffer;
	size_t plane;
};

class [[clang::lto_visibility_public]] Image : public sw::Surface, public gl::Object
{
protected:
	// 2D texture image
	Image(Texture *parentTexture, GLsizei width, GLsizei height, GLint internalformat) :
		sw::Surface(parentTexture->getResource(), width, height, 1, 0, 1, gl::SelectInternalFormat(internalformat), true, true),
		width(width), height(height), depth(1), internalformat(internalformat), parentTexture(parentTexture)
	{
		shared = false;
		Object::addRef();
		parentTexture->addRef();
	}

	// 3D/Cube texture image
	Image(Texture *parentTexture, GLsizei width, GLsizei height, GLsizei depth, int border, GLint internalformat) :
		sw::Surface(parentTexture->getResource(), width, height, depth, border, 1, gl::SelectInternalFormat(internalformat), true, true),
		width(width), height(height), depth(depth), internalformat(internalformat), parentTexture(parentTexture)
	{
		shared = false;
		Object::addRef();
		parentTexture->addRef();
	}

	// Native EGL image
	Image(GLsizei width, GLsizei height, GLint internalformat, int pitchP) :
		sw::Surface(nullptr, width, height, 1, 0, 1, gl::SelectInternalFormat(internalformat), true, true, pitchP),
		width(width), height(height), depth(1), internalformat(internalformat), parentTexture(nullptr)
	{
		shared = true;
		Object::addRef();
	}

	// Render target
	Image(GLsizei width, GLsizei height, GLint internalformat, int multiSampleDepth, bool lockable) :
		sw::Surface(nullptr, width, height, 1, 0, multiSampleDepth, gl::SelectInternalFormat(internalformat), lockable, true),
		width(width), height(height), depth(1), internalformat(internalformat), parentTexture(nullptr)
	{
		shared = false;
		Object::addRef();
	}

public:
	// 2D texture image
	static Image *create(Texture *parentTexture, GLsizei width, GLsizei height, GLint internalformat);

	// 3D/Cube texture image
	static Image *create(Texture *parentTexture, GLsizei width, GLsizei height, GLsizei depth, int border, GLint internalformat);

	// Native EGL image
	static Image *create(GLsizei width, GLsizei height, GLint internalformat, int pitchP);

	// Render target
	static Image *create(GLsizei width, GLsizei height, GLint internalformat, int multiSampleDepth, bool lockable);

	// Back buffer from client buffer
	static Image *create(const egl::ClientBuffer& clientBuffer);

	static size_t size(int width, int height, int depth, int border, int samples, GLint internalformat);

	GLsizei getWidth() const
	{
		return width;
	}

	GLsizei getHeight() const
	{
		return height;
	}

	int getDepth() const
	{
		return depth;
	}

	GLint getFormat() const
	{
		return internalformat;
	}

	bool isShared() const
	{
		return shared;
	}

	void markShared()
	{
		shared = true;
	}

	virtual void *lock(int x, int y, int z, sw::Lock lock)
	{
		return lockExternal(x, y, z, lock, sw::PUBLIC);
	}

	unsigned int getPitch() const
	{
		return getExternalPitchB();
	}

	unsigned int getSlice() const
	{
		return getExternalSliceB();
	}

	virtual void unlock()
	{
		unlockExternal();
	}

	void *lockInternal(int x, int y, int z, sw::Lock lock, sw::Accessor client) override = 0;
	void unlockInternal() override = 0;

	void loadImageData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const gl::PixelStorageModes &unpackParameters, const void *pixels);
	void loadCompressedData(GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *pixels);

	void release() override = 0;
	void unbind(const Texture *parent);
	bool isChildOf(const Texture *parent) const;

	virtual void destroyShared()
	{
		assert(shared);
		shared = false;
		release();
	}

protected:
	const GLsizei width;
	const GLsizei height;
	const int depth;
	const GLint internalformat;

	bool shared;

	egl::Texture *parentTexture;

	~Image() override = 0;

	void loadImageData(GLsizei width, GLsizei height, GLsizei depth, int inputPitch, int inputHeight, GLenum format, GLenum type, const void *input, void *buffer);
	void loadStencilData(GLsizei width, GLsizei height, GLsizei depth, int inputPitch, int inputHeight, GLenum format, GLenum type, const void *input, void *buffer);
};

}

#endif
