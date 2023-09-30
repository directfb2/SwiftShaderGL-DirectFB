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

#ifndef LIBGLESV2_VERTEXARRAY_H_
#define LIBGLESV2_VERTEXARRAY_H_

#include "Context.h"

namespace es2 {

class VertexArray : public gl::NamedObject
{
public:
	VertexArray(GLuint name);
	~VertexArray();

	const VertexAttribute& getVertexAttribute(size_t attributeIndex) const;
	VertexAttributeArray& getVertexAttributes() { return mVertexAttributes; }

	void detachBuffer(GLuint bufferName);
	void setVertexAttribDivisor(GLuint index, GLuint divisor);
	void enableAttribute(unsigned int attributeIndex, bool enabledState);
	void setAttributeState(unsigned int attributeIndex, Buffer *boundBuffer, GLint size, GLenum type, bool normalized, bool pureInteger, GLsizei stride, const void *pointer);

	Buffer *getElementArrayBuffer() const { return mElementArrayBuffer; }
	void setElementArrayBuffer(Buffer *buffer);

private:
	VertexAttributeArray mVertexAttributes;
	gl::BindingPointer<Buffer> mElementArrayBuffer;
};

}

#endif
