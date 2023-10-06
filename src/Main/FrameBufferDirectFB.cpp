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

#include "FrameBufferDirectFB.hpp"

namespace sw {

FrameBufferDirectFB::FrameBufferDirectFB(IDirectFB *dfb, IDirectFBSurface *window, int width, int height) : FrameBuffer(width, height, false, false)
{
	DFBSurfacePixelFormat pixelformat;
	window->GetPixelFormat(window, &pixelformat);
	switch(pixelformat)
	{
	case DSPF_RGB16:
		format = FORMAT_R5G6B5;
		break;
	case DSPF_RGB24:
		format = FORMAT_R8G8B8;
		break;
	default: // FORMAT_X8R8G8B8
		break;
	}

	window->GetCapabilities(window, &caps);
	if (caps & DSCAPS_GL)
	{
		window->AddRef(window);
		surface = window;
	}
	else
		window->GetSubSurface(window, NULL, &surface);
}

FrameBufferDirectFB::~FrameBufferDirectFB()
{
	surface->Release(surface);
}

void *FrameBufferDirectFB::lock()
{
	surface->Lock(surface, DSLF_WRITE, &framebuffer, &stride);

	return framebuffer;
}

void FrameBufferDirectFB::unlock()
{
	surface->Unlock(surface);

	framebuffer = nullptr;
}

void FrameBufferDirectFB::blit(sw::Surface *source, const Rect *sourceRect, const Rect *destRect)
{
	copy(source);

	if (!(caps & DSCAPS_GL))
		surface->Flip(surface, NULL, DSFLIP_WAITFORSYNC);
}

}

sw::FrameBuffer *createFrameBuffer(void *display, void *window, int width, int height)
{
	return new sw::FrameBufferDirectFB((IDirectFB*)display, (IDirectFBSurface*)window, width, height);
}
