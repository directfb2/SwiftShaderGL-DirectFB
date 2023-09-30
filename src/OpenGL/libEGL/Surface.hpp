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

#ifndef egl_Surface_hpp
#define egl_Surface_hpp

#include "common/Object.hpp"
#include "common/Surface.hpp"
#include "Main/FrameBuffer.hpp"

#include <EGL/egl.h>

namespace egl {

class Display;
class Config;

class Surface : public gl::Surface, public gl::Object
{
public:
	virtual bool initialize();
	virtual void swap() = 0;

	egl::Image *getRenderTarget() override;
	egl::Image *getDepthStencil() override;

	void setMipmapLevel(EGLint mipmapLevel);
	void setMultisampleResolve(EGLenum multisampleResolve);
	void setSwapBehavior(EGLenum swapBehavior);
	void setSwapInterval(EGLint interval);

	virtual EGLint getConfigID() const;
	virtual EGLenum getSurfaceType() const;

	EGLint getWidth() const override;
	EGLint getHeight() const override;
	EGLenum getTextureTarget() const override;
	virtual EGLint getMipmapLevel() const;
	virtual EGLenum getMultisampleResolve() const;
	virtual EGLint getPixelAspectRatio() const;
	virtual EGLenum getRenderBuffer() const;
	virtual EGLenum getSwapBehavior() const;
	virtual EGLenum getTextureFormat() const;
	virtual EGLBoolean getLargestPBuffer() const;
	virtual EGLNativeWindowType getWindowHandle() const = 0;

	void setBoundTexture(egl::Texture *texture) override;
	virtual egl::Texture *getBoundTexture() const;

	virtual bool isWindowSurface() const { return false; }
	virtual bool isPBufferSurface() const { return false; }
	bool hasClientBuffer() const { return clientBuffer != nullptr; }

protected:
	Surface(const Display *display, const Config *config);

	~Surface() override;

	virtual void deleteResources();

	sw::Format getClientBufferFormat() const;

	const Display *const display;
	const Config *const config;

	Image *depthStencil = nullptr;
	Image *backBuffer = nullptr;
	Texture *texture = nullptr;

	bool reset(int backbufferWidth, int backbufferHeight);

	EGLint width = 0;                                             // Width of surface
	EGLint height= 0;                                             // Height of surface
	EGLBoolean largestPBuffer = EGL_FALSE;                        // If true, create largest pbuffer possible
	EGLint mipmapLevel = 0;                                       // Mipmap level to render to
	EGLenum multisampleResolve = EGL_MULTISAMPLE_RESOLVE_DEFAULT; // Multisample resolve behavior
	EGLint pixelAspectRatio = EGL_UNKNOWN;                        // Display aspect ratio
	EGLenum renderBuffer = EGL_BACK_BUFFER;                       // Render buffer
	EGLenum swapBehavior = EGL_BUFFER_PRESERVED;                  // Buffer swap behavior
	EGLenum textureFormat = EGL_NO_TEXTURE;                       // Format of texture: RGB, RGBA, or no texture
	EGLenum textureTarget = EGL_NO_TEXTURE;                       // Type of texture: 2D or no texture

	EGLint swapInterval = 1;

	EGLClientBuffer clientBuffer = nullptr;
	EGLint clientBufferPlane;
	EGLenum clientBufferFormat; // Format of the client buffer
	EGLenum clientBufferType;   // Type of the client buffer
};

class WindowSurface : public Surface
{
public:
	WindowSurface(Display *display, const Config *config, EGLNativeWindowType window);
	~WindowSurface() override;

	bool initialize() override;

	bool isWindowSurface() const override { return true; }
	void swap() override;

	EGLNativeWindowType getWindowHandle() const override;

private:
	void deleteResources() override;
	bool checkForResize();
	bool reset(int backBufferWidth, int backBufferHeight);

	const EGLNativeWindowType window;
	sw::FrameBuffer *frameBuffer = nullptr;
};

class PBufferSurface : public Surface
{
public:
	PBufferSurface(Display *display, const Config *config, EGLint width, EGLint height,
	               EGLenum textureFormat, EGLenum textureTarget, EGLenum internalFormat,
	               EGLenum textureType, EGLBoolean largestPBuffer, EGLClientBuffer clientBuffer,
	               EGLint clientBufferPlane);
	~PBufferSurface() override;

	bool isPBufferSurface() const override { return true; }
	void swap() override;

	EGLNativeWindowType getWindowHandle() const override;

private:
	void deleteResources() override;
};

}

#endif
