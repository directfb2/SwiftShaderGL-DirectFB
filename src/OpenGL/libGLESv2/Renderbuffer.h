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

#ifndef LIBGLESV2_RENDERBUFFER_H_
#define LIBGLESV2_RENDERBUFFER_H_

#include "common/Object.hpp"

namespace egl {

class Image;

}

namespace es2 {

class Renderbuffer;
class Texture2D;
class Texture3D;
class TextureCubeMap;

class RenderbufferInterface
{
public:
	RenderbufferInterface();

	virtual ~RenderbufferInterface() {}

	virtual void addProxyRef(const Renderbuffer *proxy);
	virtual void releaseProxy(const Renderbuffer *proxy);

	virtual egl::Image *getRenderTarget() = 0;
	virtual egl::Image *createSharedImage() = 0;
	virtual bool isShared() const = 0;

	virtual GLsizei getWidth() const = 0;
	virtual GLsizei getHeight() const = 0;
	virtual GLsizei getDepth() const { return 1; }
	virtual GLint getLevel() const { return 0; }
	virtual GLint getFormat() const = 0;
	virtual GLsizei getSamples() const = 0;

	virtual void setLevel(GLint) {}

	GLuint getRedSize() const;
	GLuint getGreenSize() const;
	GLuint getBlueSize() const;
	GLuint getAlphaSize() const;
	GLuint getDepthSize() const;
	GLuint getStencilSize() const;
};

class RenderbufferTexture2D : public RenderbufferInterface
{
public:
	RenderbufferTexture2D(Texture2D *texture, GLint level);

	~RenderbufferTexture2D() override;

	void addProxyRef(const Renderbuffer *proxy) override;
	void releaseProxy(const Renderbuffer *proxy) override;

	egl::Image *getRenderTarget() override;
	egl::Image *createSharedImage() override;
	bool isShared() const override;

	GLsizei getWidth() const override;
	GLsizei getHeight() const override;
	GLint getLevel() const override { return mLevel; }
	GLint getFormat() const override;
	GLsizei getSamples() const override;

	void setLevel(GLint level) override { mLevel = level; }

private:
	gl::BindingPointer<Texture2D> mTexture2D;
	GLint mLevel;
};

class RenderbufferTexture3D : public RenderbufferInterface
{
public:
	RenderbufferTexture3D(Texture3D *texture, GLint level);

	~RenderbufferTexture3D() override;

	void addProxyRef(const Renderbuffer *proxy) override;
	void releaseProxy(const Renderbuffer *proxy) override;

	egl::Image *getRenderTarget() override;
	egl::Image *createSharedImage() override;
	bool isShared() const override;

	GLsizei getWidth() const override;
	GLsizei getHeight() const override;
	GLsizei getDepth() const override;
	GLint getLevel() const override { return mLevel; }
	GLint getFormat() const override;
	GLsizei getSamples() const override;

	void setLevel(GLint level) override { mLevel = level; }

private:
	gl::BindingPointer<Texture3D> mTexture3D;
	GLint mLevel;
};

class RenderbufferTextureCubeMap : public RenderbufferInterface
{
public:
	RenderbufferTextureCubeMap(TextureCubeMap *texture, GLenum target, GLint level);

	~RenderbufferTextureCubeMap() override;

	void addProxyRef(const Renderbuffer *proxy) override;
	void releaseProxy(const Renderbuffer *proxy) override;

	egl::Image *getRenderTarget() override;
	egl::Image *createSharedImage() override;
	bool isShared() const override;

	GLsizei getWidth() const override;
	GLsizei getHeight() const override;
	GLint getLevel() const override { return mLevel; }
	GLint getFormat() const override;
	GLsizei getSamples() const override;

	void setLevel(GLint level) override { mLevel = level; }

private:
	gl::BindingPointer<TextureCubeMap> mTextureCubeMap;
	GLenum mTarget;
	GLint mLevel;
};

class RenderbufferStorage : public RenderbufferInterface
{
public:
	RenderbufferStorage();

	~RenderbufferStorage() override = 0;

	egl::Image *getRenderTarget() override = 0;
	egl::Image *createSharedImage() override = 0;
	bool isShared() const override = 0;

	GLsizei getWidth() const override;
	GLsizei getHeight() const override;
	GLint getFormat() const override;
	GLsizei getSamples() const override;

protected:
	GLsizei mWidth;
	GLsizei mHeight;
	GLenum format;
	GLsizei mSamples;
};

class Renderbuffer : public gl::NamedObject
{
public:
	Renderbuffer(GLuint name, RenderbufferInterface *storage);

	~Renderbuffer() override;

	void addRef() override;
	void release() override;

	egl::Image *getRenderTarget();
	virtual egl::Image *createSharedImage();
	virtual bool isShared() const;

	GLsizei getWidth() const;
	GLsizei getHeight() const;
	GLsizei getDepth() const;
	GLint getLevel() const;
	GLint getFormat() const;
	GLuint getRedSize() const;
	GLuint getGreenSize() const;
	GLuint getBlueSize() const;
	GLuint getAlphaSize() const;
	GLuint getDepthSize() const;
	GLuint getStencilSize() const;
	GLsizei getSamples() const;

	void setLevel(GLint level);
	void setStorage(RenderbufferStorage *newStorage);

private:
	RenderbufferInterface *mInstance;
};

class Colorbuffer : public RenderbufferStorage
{
public:
	explicit Colorbuffer(egl::Image *renderTarget);
	Colorbuffer(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples);

	~Colorbuffer() override;

	egl::Image *getRenderTarget() override;
	egl::Image *createSharedImage() override;
	bool isShared() const override;

private:
	egl::Image *mRenderTarget;
};

class DepthStencilbuffer : public RenderbufferStorage
{
public:
	explicit DepthStencilbuffer(egl::Image *depthStencil);
	DepthStencilbuffer(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples);

	~DepthStencilbuffer();

	egl::Image *getRenderTarget() override;
	egl::Image *createSharedImage() override;
	bool isShared() const override;

protected:
	egl::Image *mDepthStencil;
};

class Depthbuffer : public DepthStencilbuffer
{
public:
	explicit Depthbuffer(egl::Image *depthStencil);
	Depthbuffer(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples);

	~Depthbuffer() override;
};

class Stencilbuffer : public DepthStencilbuffer
{
public:
	explicit Stencilbuffer(egl::Image *depthStencil);
	Stencilbuffer(GLsizei width, GLsizei height, GLsizei samples);

	~Stencilbuffer() override;
};

}

#endif
