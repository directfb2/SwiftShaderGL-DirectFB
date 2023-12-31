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

#include "Display.h"

#include "common/debug.h"
#include "common/Image.hpp"
#include "main.h"
#include "Surface.hpp"

#include <directfb.h>

namespace egl {

class DisplayImplementation : public Display
{
public:
	DisplayImplementation(EGLDisplay dpy, void *nativeDisplay) : Display(dpy, nativeDisplay) {}
	~DisplayImplementation() override {}

	Image *getSharedImage(EGLImageKHR name) override
	{
		return Display::getSharedImage(name);
	}
};

Display *Display::get(EGLDisplay dpy)
{
	if(dpy != PRIMARY_DISPLAY && dpy != HEADLESS_DISPLAY)
	{
		return nullptr;
	}

	static void *nativeDisplay = nullptr;

	if(!nativeDisplay && dpy != HEADLESS_DISPLAY)
	{
		IDirectFB *dfb;
		DirectFBCreate(&dfb);
		nativeDisplay = dfb;
	}

	static DisplayImplementation display(dpy, nativeDisplay);

	return &display;
}

Display::Display(EGLDisplay eglDisplay, void *nativeDisplay) : eglDisplay(eglDisplay), nativeDisplay(nativeDisplay)
{
	mMinSwapInterval = 1;
	mMaxSwapInterval = 1;
}

Display::~Display()
{
	terminate();

	if(nativeDisplay)
	{
		IDirectFB *dfb = (IDirectFB*)nativeDisplay;
		dfb->Release(dfb);
	}
}

bool Display::initialize()
{
	if(isInitialized())
	{
		return true;
	}

	mMinSwapInterval = 0;
	mMaxSwapInterval = 4;

	const int samples[] =
	{
		0,
		2,
		4
	};

	const sw::Format renderTargetFormats[] =
	{
		sw::FORMAT_A8R8G8B8,
		sw::FORMAT_A8B8G8R8,
		sw::FORMAT_R5G6B5,
		sw::FORMAT_X8R8G8B8,
		sw::FORMAT_X8B8G8R8
	};

	const sw::Format depthStencilFormats[] =
	{
		sw::FORMAT_NULL,
		sw::FORMAT_D32,
		sw::FORMAT_D24S8,
		sw::FORMAT_D24X8,
		sw::FORMAT_D16
	};

	sw::Format currentDisplayFormat = getDisplayFormat();
	ConfigSet configSet;

	for(unsigned int samplesIndex = 0; samplesIndex < sizeof(samples) / sizeof(int); samplesIndex++)
	{
		for(sw::Format renderTargetFormat : renderTargetFormats)
		{
			for(sw::Format depthStencilFormat : depthStencilFormats)
			{
				configSet.add(currentDisplayFormat, mMinSwapInterval, mMaxSwapInterval, renderTargetFormat, depthStencilFormat, samples[samplesIndex]);
			}
		}
	}

	// Give the sorted configs a unique ID and store them internally
	EGLint index = 1;
	for(ConfigSet::Iterator config = configSet.mSet.begin(); config != configSet.mSet.end(); config++)
	{
		Config configuration = *config;
		configuration.mConfigID = index;
		index++;

		mConfigSet.mSet.insert(configuration);
	}

	if(!isInitialized())
	{
		terminate();

		return false;
	}

	return true;
}

void Display::terminate()
{
	while(!mSurfaceSet.empty())
	{
		destroySurface(*mSurfaceSet.begin());
	}

	while(!mContextSet.empty())
	{
		destroyContext(*mContextSet.begin());
	}

	while(!mSharedImageNameSpace.empty())
	{
		destroySharedImage(reinterpret_cast<EGLImageKHR>((intptr_t)mSharedImageNameSpace.firstName()));
	}
}

bool Display::getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig)
{
	return mConfigSet.getConfigs(configs, attribList, configSize, numConfig);
}

bool Display::getConfigAttrib(EGLConfig config, EGLint attribute, EGLint *value)
{
	const egl::Config *configuration = mConfigSet.get(config);

	switch(attribute)
	{
	case EGL_BUFFER_SIZE:             *value = configuration->mBufferSize;            break;
	case EGL_ALPHA_SIZE:              *value = configuration->mAlphaSize;             break;
	case EGL_BLUE_SIZE:               *value = configuration->mBlueSize;              break;
	case EGL_GREEN_SIZE:              *value = configuration->mGreenSize;             break;
	case EGL_RED_SIZE:                *value = configuration->mRedSize;               break;
	case EGL_DEPTH_SIZE:              *value = configuration->mDepthSize;             break;
	case EGL_STENCIL_SIZE:            *value = configuration->mStencilSize;           break;
	case EGL_CONFIG_CAVEAT:           *value = configuration->mConfigCaveat;          break;
	case EGL_CONFIG_ID:               *value = configuration->mConfigID;              break;
	case EGL_LEVEL:                   *value = configuration->mLevel;                 break;
	case EGL_NATIVE_RENDERABLE:       *value = configuration->mNativeRenderable;      break;
	case EGL_NATIVE_VISUAL_ID:        *value = configuration->mNativeVisualID;        break;
	case EGL_NATIVE_VISUAL_TYPE:      *value = configuration->mNativeVisualType;      break;
	case EGL_SAMPLES:                 *value = configuration->mSamples;               break;
	case EGL_SAMPLE_BUFFERS:          *value = configuration->mSampleBuffers;         break;
	case EGL_SURFACE_TYPE:            *value = configuration->mSurfaceType;           break;
	case EGL_TRANSPARENT_TYPE:        *value = configuration->mTransparentType;       break;
	case EGL_TRANSPARENT_BLUE_VALUE:  *value = configuration->mTransparentBlueValue;  break;
	case EGL_TRANSPARENT_GREEN_VALUE: *value = configuration->mTransparentGreenValue; break;
	case EGL_TRANSPARENT_RED_VALUE:   *value = configuration->mTransparentRedValue;   break;
	case EGL_BIND_TO_TEXTURE_RGB:     *value = configuration->mBindToTextureRGB;      break;
	case EGL_BIND_TO_TEXTURE_RGBA:    *value = configuration->mBindToTextureRGBA;     break;
	case EGL_MIN_SWAP_INTERVAL:       *value = configuration->mMinSwapInterval;       break;
	case EGL_MAX_SWAP_INTERVAL:       *value = configuration->mMaxSwapInterval;       break;
	case EGL_LUMINANCE_SIZE:          *value = configuration->mLuminanceSize;         break;
	case EGL_ALPHA_MASK_SIZE:         *value = configuration->mAlphaMaskSize;         break;
	case EGL_COLOR_BUFFER_TYPE:       *value = configuration->mColorBufferType;       break;
	case EGL_RENDERABLE_TYPE:         *value = configuration->mRenderableType;        break;
	case EGL_MATCH_NATIVE_PIXMAP:     *value = EGL_FALSE; UNIMPLEMENTED();            break;
	case EGL_CONFORMANT:              *value = configuration->mConformant;            break;
	case EGL_MAX_PBUFFER_WIDTH:       *value = configuration->mMaxPBufferWidth;       break;
	case EGL_MAX_PBUFFER_HEIGHT:      *value = configuration->mMaxPBufferHeight;      break;
	case EGL_MAX_PBUFFER_PIXELS:      *value = configuration->mMaxPBufferPixels;      break;
	default:
		return false;
	}

	return true;
}

EGLSurface Display::createWindowSurface(EGLNativeWindowType window, EGLConfig config, const EGLAttrib *attribList)
{
	const Config *configuration = mConfigSet.get(config);

	if(attribList)
	{
		while(*attribList != EGL_NONE)
		{
			switch(attribList[0])
			{
			case EGL_RENDER_BUFFER:
				switch(attribList[1])
				{
				case EGL_BACK_BUFFER:
					break;
				case EGL_SINGLE_BUFFER:
					return error(EGL_BAD_MATCH, EGL_NO_SURFACE); // Rendering directly to front buffer not supported
				default:
					return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
				}
				break;
			case EGL_VG_COLORSPACE:
				return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
			case EGL_VG_ALPHA_FORMAT:
				return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
			default:
				return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
			}

			attribList += 2;
		}
	}

	if(hasExistingWindowSurface(window))
	{
		return error(EGL_BAD_ALLOC, EGL_NO_SURFACE);
	}

	Surface *surface = new WindowSurface(this, configuration, window);

	if(!surface->initialize())
	{
		surface->release();
		return EGL_NO_SURFACE;
	}

	surface->addRef();
	mSurfaceSet.insert(surface);

	return success(surface);
}

EGLSurface Display::createPBufferSurface(EGLConfig config, const EGLint *attribList, EGLClientBuffer clientBuffer)
{
	EGLint width = -1, height = -1;
	EGLenum textureFormat = EGL_NO_TEXTURE;
	EGLenum textureTarget = EGL_NO_TEXTURE;
	EGLenum clientBufferFormat = EGL_NO_TEXTURE;
	EGLenum clientBufferType = EGL_NO_TEXTURE;
	EGLBoolean largestPBuffer = EGL_FALSE;
	const Config *configuration = mConfigSet.get(config);

	if(attribList)
	{
		while(*attribList != EGL_NONE)
		{
			switch(attribList[0])
			{
			case EGL_WIDTH:
				width = attribList[1];
				break;
			case EGL_HEIGHT:
				height = attribList[1];
				break;
			case EGL_LARGEST_PBUFFER:
				largestPBuffer = attribList[1];
				break;
			case EGL_TEXTURE_FORMAT:
				switch(attribList[1])
				{
				case EGL_NO_TEXTURE:
				case EGL_TEXTURE_RGB:
				case EGL_TEXTURE_RGBA:
					textureFormat = attribList[1];
					break;
				default:
					return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
				}
				break;
			case EGL_TEXTURE_TARGET:
				switch(attribList[1])
				{
				case EGL_NO_TEXTURE:
				case EGL_TEXTURE_2D:
					textureTarget = attribList[1];
					break;
				default:
					return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
				}
				break;
			case EGL_MIPMAP_TEXTURE:
				if(attribList[1] != EGL_FALSE)
				{
					UNIMPLEMENTED();
					return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
				}
				break;
			case EGL_VG_COLORSPACE:
				return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
			case EGL_VG_ALPHA_FORMAT:
				return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
			default:
				return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
			}

			attribList += 2;
		}
	}

	if(width < 0 || height < 0)
	{
		return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
	}

	if(width == 0 || height == 0)
	{
		return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
	}

	if((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
	   (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
	{
		return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
	}

	if(!(configuration->mSurfaceType & EGL_PBUFFER_BIT))
	{
		return error(EGL_BAD_MATCH, EGL_NO_SURFACE);
	}

	if(clientBuffer)
	{
		switch(clientBufferType)
		{
		case GL_UNSIGNED_BYTE:
			switch(clientBufferFormat)
			{
			case GL_RED:
			case GL_RG:
			case GL_RGB:
			case GL_BGRA_EXT:
				break;
			case GL_R16UI:
			case GL_RGBA:
				return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
			default:
				return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
			}
			break;
		case GL_UNSIGNED_SHORT:
			switch(clientBufferFormat)
			{
			case GL_R16UI:
				break;
			case GL_RED:
			case GL_RG:
			case GL_BGRA_EXT:
			case GL_RGBA:
				return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
			default:
				return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
			}
			break;
		case GL_HALF_FLOAT_OES:
		case GL_HALF_FLOAT:
			switch(clientBufferFormat)
			{
			case GL_RGBA:
				break;
			case GL_RED:
			case GL_R16UI:
			case GL_RG:
			case GL_BGRA_EXT:
				return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
			default:
				return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
			}
			break;
		default:
			return error(EGL_BAD_PARAMETER, EGL_NO_SURFACE);
		}

		if(textureFormat != EGL_TEXTURE_RGBA)
		{
			return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
		}
	}
	else
	{
		if((textureFormat == EGL_TEXTURE_RGB && configuration->mBindToTextureRGB != EGL_TRUE) ||
		   ((textureFormat == EGL_TEXTURE_RGBA && configuration->mBindToTextureRGBA != EGL_TRUE)))
		{
			return error(EGL_BAD_ATTRIBUTE, EGL_NO_SURFACE);
		}
	}

	Surface *surface = new PBufferSurface(this, configuration, width, height, textureFormat, textureTarget, clientBufferFormat, clientBufferType, largestPBuffer, clientBuffer, -1);

	if(!surface->initialize())
	{
		surface->release();
		return EGL_NO_SURFACE;
	}

	surface->addRef();
	mSurfaceSet.insert(surface);

	return success(surface);
}

EGLContext Display::createContext(EGLConfig configHandle, const egl::Context *shareContext, EGLint clientVersion)
{
	const egl::Config *config = mConfigSet.get(configHandle);
	egl::Context *context = nullptr;

	if((clientVersion == 2 && config->mRenderableType & EGL_OPENGL_ES2_BIT) ||
	   (clientVersion == 3 && config->mRenderableType & EGL_OPENGL_ES3_BIT))
	{
		if(libGLESv2)
		{
			context = libGLESv2->es2CreateContext(this, shareContext, config);
		}
	}
	else
	{
		return error(EGL_BAD_CONFIG, EGL_NO_CONTEXT);
	}

	if(!context)
	{
		return error(EGL_BAD_ALLOC, EGL_NO_CONTEXT);
	}

	context->addRef();
	mContextSet.insert(context);

	return success(context);
}

EGLSyncKHR Display::createSync(Context *context)
{
	FenceSync *fenceSync = new egl::FenceSync(context);
	mSyncSet.insert(fenceSync);
	return fenceSync;
}

void Display::destroySurface(egl::Surface *surface)
{
	surface->release();
	mSurfaceSet.erase(surface);

	if(surface == getCurrentDrawSurface())
	{
		setCurrentDrawSurface(nullptr);
	}

	if(surface == getCurrentReadSurface())
	{
		setCurrentReadSurface(nullptr);
	}
}

void Display::destroyContext(egl::Context *context)
{
	context->release();
	mContextSet.erase(context);

	if(context == getCurrentContext())
	{
		setCurrentContext(nullptr);
		setCurrentDrawSurface(nullptr);
		setCurrentReadSurface(nullptr);
	}
}

void Display::destroySync(FenceSync *sync)
{
	{
		mSyncSet.erase(sync);
	}
	delete sync;
}

bool Display::isInitialized() const
{
	return mConfigSet.size() > 0;
}

bool Display::isValidConfig(EGLConfig config)
{
	return mConfigSet.get(config) != nullptr;
}

bool Display::isValidContext(egl::Context *context)
{
	return mContextSet.find(context) != mContextSet.end();
}

bool Display::isValidSurface(egl::Surface *surface)
{
	return mSurfaceSet.find(surface) != mSurfaceSet.end();
}

bool Display::isValidWindow(EGLNativeWindowType window)
{
		if(nativeDisplay)
		{
			return true;
		}

		return false;
}

bool Display::hasExistingWindowSurface(EGLNativeWindowType window)
{
	for(const auto &surface : mSurfaceSet)
	{
		if(surface->isWindowSurface())
		{
			if(surface->getWindowHandle() == window)
			{
				return true;
			}
		}
	}

	return false;
}

bool Display::isValidSync(FenceSync *sync)
{
	return mSyncSet.find(sync) != mSyncSet.end();
}

EGLint Display::getMinSwapInterval() const
{
	return mMinSwapInterval;
}

EGLint Display::getMaxSwapInterval() const
{
	return mMaxSwapInterval;
}

EGLDisplay Display::getEGLDisplay() const
{
	return eglDisplay;
}

void *Display::getNativeDisplay() const
{
	return nativeDisplay;
}

EGLImageKHR Display::createSharedImage(Image *image)
{
	return reinterpret_cast<EGLImageKHR>((intptr_t)mSharedImageNameSpace.allocate(image));
}

bool Display::destroySharedImage(EGLImageKHR image)
{
	GLuint name = (GLuint)reinterpret_cast<intptr_t>(image);
	Image *eglImage = mSharedImageNameSpace.find(name);

	if(!eglImage)
	{
		return false;
	}

	eglImage->destroyShared();
	mSharedImageNameSpace.remove(name);

	return true;
}

Image *Display::getSharedImage(EGLImageKHR image)
{
	GLuint name = (GLuint)reinterpret_cast<intptr_t>(image);
	return mSharedImageNameSpace.find(name);
}

sw::Format Display::getDisplayFormat() const
{
	if(nativeDisplay)
	{
		unsigned int bpp = 24;

		switch(bpp)
		{
		case 32: return sw::FORMAT_X8R8G8B8;
		case 24: return sw::FORMAT_R8G8B8;
		case 16: return sw::FORMAT_R5G6B5;
		default:
			UNREACHABLE(bpp);
			break;
		}
	}

	return sw::FORMAT_A8B8G8R8;
}

}
