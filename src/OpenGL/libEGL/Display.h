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

#ifndef egl_Display_h
#define egl_Display_h

#include "common/NameSpace.hpp"
#include "Common/RecursiveLock.hpp"
#include "Config.h"
#include "Sync.hpp"

namespace egl {

class Surface;
class Context;
class Image;

const EGLDisplay PRIMARY_DISPLAY  = reinterpret_cast<EGLDisplay>((intptr_t)1);
const EGLDisplay HEADLESS_DISPLAY = reinterpret_cast<EGLDisplay>((intptr_t)0xFACE1E55);

class [[clang::lto_visibility_public]] Display
{
protected:
	explicit Display(EGLDisplay eglDisplay, void *nativeDisplay);
	virtual ~Display() = 0;

public:
	static Display *get(EGLDisplay dpy);

	bool initialize();
	void terminate();

	bool getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig);
	bool getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value);

	EGLSurface createWindowSurface(EGLNativeWindowType window, EGLConfig config, const EGLAttrib *attribList);
	EGLSurface createPBufferSurface(EGLConfig config, const EGLint *attribList, EGLClientBuffer clientBuffer = nullptr);
	EGLContext createContext(EGLConfig configHandle, const Context *shareContext, EGLint clientVersion);
	EGLSyncKHR createSync(Context *context);

	void destroySurface(Surface *surface);
	void destroyContext(Context *context);
	void destroySync(FenceSync *sync);

	bool isInitialized() const;
	bool isValidConfig(EGLConfig config);
	bool isValidContext(Context *context);
	bool isValidSurface(Surface *surface);
	bool isValidWindow(EGLNativeWindowType window);
	bool hasExistingWindowSurface(EGLNativeWindowType window);
	bool isValidSync(FenceSync *sync);

	EGLint getMinSwapInterval() const;
	EGLint getMaxSwapInterval() const;

	EGLDisplay getEGLDisplay() const;
	void *getNativeDisplay() const;

	EGLImageKHR createSharedImage(Image *image);
	bool destroySharedImage(EGLImageKHR);
	virtual Image *getSharedImage(EGLImageKHR name) = 0;

	sw::RecursiveLock *getLock() { return &mApiMutex; }

private:
	sw::Format getDisplayFormat() const;

	const EGLDisplay eglDisplay;
	void *const nativeDisplay;

	EGLint mMaxSwapInterval;
	EGLint mMinSwapInterval;

	typedef std::set<Surface*> SurfaceSet;
	SurfaceSet mSurfaceSet;

	ConfigSet mConfigSet;

	typedef std::set<Context*> ContextSet;
	ContextSet mContextSet;

	typedef std::set<FenceSync*> SyncSet;
	SyncSet mSyncSet;

	gl::NameSpace<Image> mSharedImageNameSpace;
	sw::RecursiveLock mApiMutex;
};

}

#endif
