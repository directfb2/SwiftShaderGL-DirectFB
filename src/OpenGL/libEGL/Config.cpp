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

#include "Config.h"

#include "common/debug.h"

#include <cassert>
#include <cstring>
#include <map>
#include <vector>

namespace egl {

Config::Config(sw::Format displayFormat, EGLint minInterval, EGLint maxInterval, sw::Format renderTargetFormat, sw::Format depthStencilFormat, EGLint multiSample) :
	mRenderTargetFormat(renderTargetFormat), mDepthStencilFormat(depthStencilFormat), mMultiSample(multiSample)
{
	mBindToTextureRGB = EGL_FALSE;
	mBindToTextureRGBA = EGL_FALSE;

	// Initialize to a high value to lower the preference of formats for which there's no native support
	mNativeVisualID = 0x7FFFFFFF;

	switch(renderTargetFormat)
	{
	case sw::FORMAT_A1R5G5B5:
		mRedSize = 5;
		mGreenSize = 5;
		mBlueSize = 5;
		mAlphaSize = 1;
		break;
	case sw::FORMAT_A2R10G10B10:
		mRedSize = 10;
		mGreenSize = 10;
		mBlueSize = 10;
		mAlphaSize = 2;
		break;
	case sw::FORMAT_A8R8G8B8:
		mRedSize = 8;
		mGreenSize = 8;
		mBlueSize = 8;
		mAlphaSize = 8;
		mBindToTextureRGBA = EGL_TRUE;
		mNativeVisualID = 2; // Arbitrary; prefer over ABGR
		break;
	case sw::FORMAT_A8B8G8R8:
		mRedSize = 8;
		mGreenSize = 8;
		mBlueSize = 8;
		mAlphaSize = 8;
		mBindToTextureRGBA = EGL_TRUE;
		break;
	case sw::FORMAT_R5G6B5:
		mRedSize = 5;
		mGreenSize = 6;
		mBlueSize = 5;
		mAlphaSize = 0;
		break;
	case sw::FORMAT_X8R8G8B8:
		mRedSize = 8;
		mGreenSize = 8;
		mBlueSize = 8;
		mAlphaSize = 0;
		mBindToTextureRGB = EGL_TRUE;
		mNativeVisualID = 1; // Arbitrary; prefer over XBGR
		break;
	case sw::FORMAT_X8B8G8R8:
		mRedSize = 8;
		mGreenSize = 8;
		mBlueSize = 8;
		mAlphaSize = 0;
		mBindToTextureRGB = EGL_TRUE;
		break;
	default:
		UNREACHABLE(renderTargetFormat);
		break;
	}

	mLuminanceSize = 0;
	mBufferSize = mRedSize + mGreenSize + mBlueSize + mLuminanceSize + mAlphaSize;
	mAlphaMaskSize = 0;
	mColorBufferType = EGL_RGB_BUFFER;
	mConfigCaveat = EGL_NONE;
	mConfigID = 0;
	mConformant = EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;

	switch(depthStencilFormat)
	{
	case sw::FORMAT_NULL:
		mDepthSize = 0;
		mStencilSize = 0;
		break;
	case sw::FORMAT_D32:
		mDepthSize = 32;
		mStencilSize = 0;
		break;
	case sw::FORMAT_D24S8:
		mDepthSize = 24;
		mStencilSize = 8;
		break;
	case sw::FORMAT_D24X8:
		mDepthSize = 24;
		mStencilSize = 0;
		break;
	case sw::FORMAT_D16:
		mDepthSize = 16;
		mStencilSize = 0;
		break;
	default:
		UNREACHABLE(depthStencilFormat);
		break;
	}

	mLevel = 0;
	mMatchNativePixmap = EGL_NONE;
	mMaxPBufferWidth = 4096;
	mMaxPBufferHeight = 4096;
	mMaxPBufferPixels = mMaxPBufferWidth * mMaxPBufferHeight;
	mMaxSwapInterval = maxInterval;
	mMinSwapInterval = minInterval;
	mNativeRenderable = EGL_FALSE;
	mNativeVisualType = 0;
	mRenderableType = EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
	mSampleBuffers = (multiSample > 0) ? 1 : 0;
	mSamples = multiSample;
	mSurfaceType = EGL_PBUFFER_BIT | EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT | EGL_MULTISAMPLE_RESOLVE_BOX_BIT;
	mTransparentType = EGL_NONE;
	mTransparentRedValue = 0;
	mTransparentGreenValue = 0;
	mTransparentBlueValue = 0;
}

EGLConfig Config::getHandle() const
{
	return (EGLConfig)(size_t)mConfigID;
}

// This ordering determines the config ID
bool CompareConfig::operator()(const Config &x, const Config &y) const
{
	#define SORT_SMALLER(attribute)                \
		if(x.attribute != y.attribute)             \
		{                                          \
			return x.attribute < y.attribute;      \
		}

	static_assert(EGL_NONE < EGL_SLOW_CONFIG && EGL_SLOW_CONFIG < EGL_NON_CONFORMANT_CONFIG, "");
	SORT_SMALLER(mConfigCaveat);

	static_assert(EGL_RGB_BUFFER < EGL_LUMINANCE_BUFFER, "");
	SORT_SMALLER(mColorBufferType);

	SORT_SMALLER(mRedSize);
	SORT_SMALLER(mGreenSize);
	SORT_SMALLER(mBlueSize);
	SORT_SMALLER(mAlphaSize);

	SORT_SMALLER(mBufferSize);
	SORT_SMALLER(mSampleBuffers);
	SORT_SMALLER(mSamples);
	SORT_SMALLER(mDepthSize);
	SORT_SMALLER(mStencilSize);
	SORT_SMALLER(mAlphaMaskSize);
	SORT_SMALLER(mNativeVisualType);
	SORT_SMALLER(mNativeVisualID);

	#undef SORT_SMALLER

	assert(memcmp(&x, &y, sizeof(Config)) == 0);

	return false;
}

class SortConfig
{
public:
	explicit SortConfig(const EGLint *attribList);

	bool operator()(const Config *x, const Config *y) const;

private:
	EGLint wantedComponentsSize(const Config *config) const;

	bool mWantRed;
	bool mWantGreen;
	bool mWantBlue;
	bool mWantAlpha;
	bool mWantLuminance;
};

SortConfig::SortConfig(const EGLint *attribList) :
	mWantRed(false), mWantGreen(false), mWantBlue(false), mWantAlpha(false), mWantLuminance(false)
{
	for(const EGLint *attr = attribList; attr[0] != EGL_NONE; attr += 2)
	{
		// When multiple instances of the same attribute are present, last wins.
		bool isSpecified = attr[1] && attr[1] != EGL_DONT_CARE;
		switch(attr[0])
		{
		case EGL_RED_SIZE:       mWantRed = isSpecified;       break;
		case EGL_GREEN_SIZE:     mWantGreen = isSpecified;     break;
		case EGL_BLUE_SIZE:      mWantBlue = isSpecified;      break;
		case EGL_ALPHA_SIZE:     mWantAlpha = isSpecified;     break;
		case EGL_LUMINANCE_SIZE: mWantLuminance = isSpecified; break;
		}
	}
}

EGLint SortConfig::wantedComponentsSize(const Config *config) const
{
	EGLint total = 0;

	if(mWantRed)       total += config->mRedSize;
	if(mWantGreen)     total += config->mGreenSize;
	if(mWantBlue)      total += config->mBlueSize;
	if(mWantAlpha)     total += config->mAlphaSize;
	if(mWantLuminance) total += config->mLuminanceSize;

	return total;
}

bool SortConfig::operator()(const Config *x, const Config *y) const
{
	#define SORT_SMALLER(attribute)                \
		if(x->attribute != y->attribute)           \
		{                                          \
			return x->attribute < y->attribute;    \
		}

	static_assert(EGL_NONE < EGL_SLOW_CONFIG && EGL_SLOW_CONFIG < EGL_NON_CONFORMANT_CONFIG, "");
	SORT_SMALLER(mConfigCaveat);

	static_assert(EGL_RGB_BUFFER < EGL_LUMINANCE_BUFFER, "");
	SORT_SMALLER(mColorBufferType);

	EGLint xComponentsSize = wantedComponentsSize(x);
	EGLint yComponentsSize = wantedComponentsSize(y);
	if(xComponentsSize != yComponentsSize)
	{
		return xComponentsSize > yComponentsSize;
	}

	SORT_SMALLER(mBufferSize);
	SORT_SMALLER(mSampleBuffers);
	SORT_SMALLER(mSamples);
	SORT_SMALLER(mDepthSize);
	SORT_SMALLER(mStencilSize);
	SORT_SMALLER(mAlphaMaskSize);
	SORT_SMALLER(mNativeVisualType);
	SORT_SMALLER(mConfigID);

	#undef SORT_SMALLER

	return false;
}

ConfigSet::ConfigSet()
{
}

void ConfigSet::add(sw::Format displayFormat, EGLint minSwapInterval, EGLint maxSwapInterval, sw::Format renderTargetFormat, sw::Format depthStencilFormat, EGLint multiSample)
{
	Config conformantConfig(displayFormat, minSwapInterval, maxSwapInterval, renderTargetFormat, depthStencilFormat, multiSample);
	mSet.insert(conformantConfig);
}

size_t ConfigSet::size() const
{
	return mSet.size();
}

bool ConfigSet::getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig)
{
	std::vector<const Config*> passed;
	passed.reserve(mSet.size());

	std::map<EGLint, EGLint> attribs;
	const EGLint *attribute = attribList;
	while(attribute[0] != EGL_NONE)
	{
		attribs[attribute[0]] = attribute[1];
		attribute += 2;
	}

	for(Iterator config = mSet.begin(); config != mSet.end(); config++)
	{
		bool match = true;
		bool caveatMatch = (config->mConfigCaveat == EGL_NONE);
		for(std::map<EGLint, EGLint>::iterator attribIt = attribs.begin(); attribIt != attribs.end(); attribIt++)
		{
			if(attribIt->second != EGL_DONT_CARE)
			{
				switch(attribIt->first)
				{
				case EGL_BUFFER_SIZE:             match = config->mBufferSize >= attribIt->second;                          break;
				case EGL_ALPHA_SIZE:              match = config->mAlphaSize >= attribIt->second;                           break;
				case EGL_BLUE_SIZE:               match = config->mBlueSize >= attribIt->second;                            break;
				case EGL_GREEN_SIZE:              match = config->mGreenSize >= attribIt->second;                           break;
				case EGL_RED_SIZE:                match = config->mRedSize >= attribIt->second;                             break;
				case EGL_DEPTH_SIZE:              match = config->mDepthSize >= attribIt->second;                           break;
				case EGL_STENCIL_SIZE:            match = config->mStencilSize >= attribIt->second;                         break;
				case EGL_CONFIG_CAVEAT:           match = config->mConfigCaveat == (EGLenum)attribIt->second;               break;
				case EGL_CONFIG_ID:               match = config->mConfigID == attribIt->second;                            break;
				case EGL_LEVEL:                   match = config->mLevel >= attribIt->second;                               break;
				case EGL_NATIVE_RENDERABLE:       match = config->mNativeRenderable == (EGLBoolean)attribIt->second;        break;
				case EGL_NATIVE_VISUAL_TYPE:      match = config->mNativeVisualType == attribIt->second;                    break;
				case EGL_SAMPLES:                 match = config->mSamples >= attribIt->second;                             break;
				case EGL_SAMPLE_BUFFERS:          match = config->mSampleBuffers >= attribIt->second;                       break;
				case EGL_SURFACE_TYPE:            match = (config->mSurfaceType & attribIt->second) == attribIt->second;    break;
				case EGL_TRANSPARENT_TYPE:        match = config->mTransparentType == (EGLenum)attribIt->second;            break;
				case EGL_TRANSPARENT_BLUE_VALUE:  match = config->mTransparentBlueValue == attribIt->second;                break;
				case EGL_TRANSPARENT_GREEN_VALUE: match = config->mTransparentGreenValue == attribIt->second;               break;
				case EGL_TRANSPARENT_RED_VALUE:   match = config->mTransparentRedValue == attribIt->second;                 break;
				case EGL_BIND_TO_TEXTURE_RGB:     match = config->mBindToTextureRGB == (EGLBoolean)attribIt->second;        break;
				case EGL_BIND_TO_TEXTURE_RGBA:    match = config->mBindToTextureRGBA == (EGLBoolean)attribIt->second;       break;
				case EGL_MIN_SWAP_INTERVAL:       match = config->mMinSwapInterval == attribIt->second;                     break;
				case EGL_MAX_SWAP_INTERVAL:       match = config->mMaxSwapInterval == attribIt->second;                     break;
				case EGL_LUMINANCE_SIZE:          match = config->mLuminanceSize >= attribIt->second;                       break;
				case EGL_ALPHA_MASK_SIZE:         match = config->mAlphaMaskSize >= attribIt->second;                       break;
				case EGL_COLOR_BUFFER_TYPE:       match = config->mColorBufferType == (EGLenum)attribIt->second;            break;
				case EGL_RENDERABLE_TYPE:         match = (config->mRenderableType & attribIt->second) == attribIt->second; break;
				case EGL_MATCH_NATIVE_PIXMAP:     match = false; UNIMPLEMENTED();                                           break;
				case EGL_CONFORMANT:              match = (config->mConformant & attribIt->second) == attribIt->second;     break;

				// Ignored attributes
				case EGL_MAX_PBUFFER_WIDTH:
				case EGL_MAX_PBUFFER_HEIGHT:
				case EGL_MAX_PBUFFER_PIXELS:
				case EGL_NATIVE_VISUAL_ID:
					break;

				default:
					*numConfig = 0;
					return false;
				}

				if(!match)
				{
					break;
				}
			}

			if(attribIt->first == EGL_CONFIG_CAVEAT)
			{
				caveatMatch = match;
			}
		}

		if(match && caveatMatch)
		{
			passed.push_back(&*config);
		}
	}

	if(configs)
	{
		sort(passed.begin(), passed.end(), SortConfig(attribList));

		EGLint index;
		for(index = 0; index < configSize && index < static_cast<EGLint>(passed.size()); index++)
		{
			configs[index] = passed[index]->getHandle();
		}

		*numConfig = index;
	}
	else
	{
		*numConfig = static_cast<EGLint>(passed.size());
	}

	return true;
}

const egl::Config *ConfigSet::get(EGLConfig configHandle)
{
	for(Iterator config = mSet.begin(); config != mSet.end(); config++)
	{
		if(config->getHandle() == configHandle)
		{
			return &(*config);
		}
	}

	return nullptr;
}

}
