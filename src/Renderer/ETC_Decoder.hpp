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

class ETC_Decoder
{
public:
	enum InputType
	{
		ETC_R_SIGNED,
		ETC_R_UNSIGNED,
		ETC_RG_SIGNED,
		ETC_RG_UNSIGNED,
		ETC_RGB,
		ETC_RGB_PUNCHTHROUGH_ALPHA,
		ETC_RGBA
	};

	// Decodes 1 to 4 channel images to 8 bit output, return true if the decoding was performed
	//   src       pointer to ETC2 encoded image
	//   dst       pointer to BGRA, 8 bit output
	//   w         src image width
	//   h         src image height
	//   dstW      dst image width
	//   dstH      dst image height
	//   dstPitch  dst image pitch (bytes per row)
	//   dstBpp    dst image bytes per pixel
	//   inputType src format
	static bool Decode(const unsigned char* src, unsigned char *dst, int w, int h, int dstW, int dstH, int dstPitch, int dstBpp, InputType inputType);
};
