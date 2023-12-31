# Copyright 2023 The SwiftShader Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

egl_sources = [
  'Common/SharedLibrary.cpp',
  'OpenGL/common/debug.cpp',
  'OpenGL/common/Object.cpp',
  'OpenGL/libEGL/Config.cpp',
  'OpenGL/libEGL/Display.cpp',
  'OpenGL/libEGL/libEGL.cpp',
  'OpenGL/libEGL/main.cpp',
  'OpenGL/libEGL/Surface.cpp'
]

glesv2_sources = [
  'Common/Configurator.cpp',
  'Common/CPUID.cpp',
  'Common/Debug.cpp',
  'Common/Half.cpp',
  'Common/Memory.cpp',
  'Common/Resource.cpp',
  'Common/SharedLibrary.cpp',
  'Common/Socket.cpp',
  'Common/Thread.cpp',
  'Common/Timer.cpp',
  'Main/Config.cpp',
  'Main/FrameBuffer.cpp',
  'Main/FrameBufferDirectFB.cpp',
  'Main/SwiftConfig.cpp',
  'OpenGL/common/debug.cpp',
  'OpenGL/common/Image.cpp',
  'OpenGL/common/Object.cpp',
  'OpenGL/compiler/AnalyzeCallDepth.cpp',
  'OpenGL/compiler/Compiler.cpp',
  'OpenGL/compiler/debug.cpp',
  'OpenGL/compiler/Diagnostics.cpp',
  'OpenGL/compiler/DirectiveHandler.cpp',
  'OpenGL/compiler/glslang_lex.cpp',
  'OpenGL/compiler/glslang_tab.cpp',
  'OpenGL/compiler/InfoSink.cpp',
  'OpenGL/compiler/Initialize.cpp',
  'OpenGL/compiler/InitializeParseContext.cpp',
  'OpenGL/compiler/Intermediate.cpp',
  'OpenGL/compiler/intermOut.cpp',
  'OpenGL/compiler/IntermTraverse.cpp',
  'OpenGL/compiler/ossource_posix.cpp',
  'OpenGL/compiler/OutputASM.cpp',
  'OpenGL/compiler/parseConst.cpp',
  'OpenGL/compiler/ParseHelper.cpp',
  'OpenGL/compiler/PoolAlloc.cpp',
  'OpenGL/compiler/preprocessor/DiagnosticsBase.cpp',
  'OpenGL/compiler/preprocessor/DirectiveHandlerBase.cpp',
  'OpenGL/compiler/preprocessor/DirectiveParser.cpp',
  'OpenGL/compiler/preprocessor/ExpressionParser.cpp',
  'OpenGL/compiler/preprocessor/Input.cpp',
  'OpenGL/compiler/preprocessor/Lexer.cpp',
  'OpenGL/compiler/preprocessor/Macro.cpp',
  'OpenGL/compiler/preprocessor/MacroExpander.cpp',
  'OpenGL/compiler/preprocessor/Preprocessor.cpp',
  'OpenGL/compiler/preprocessor/Token.cpp',
  'OpenGL/compiler/preprocessor/Tokenizer.cpp',
  'OpenGL/compiler/SymbolTable.cpp',
  'OpenGL/compiler/TranslatorASM.cpp',
  'OpenGL/compiler/util.cpp',
  'OpenGL/compiler/ValidateLimitations.cpp',
  'OpenGL/compiler/ValidateSwitch.cpp',
  'OpenGL/libGLESv2/Buffer.cpp',
  'OpenGL/libGLESv2/Context.cpp',
  'OpenGL/libGLESv2/Device.cpp',
  'OpenGL/libGLESv2/entry_points.cpp',
  'OpenGL/libGLESv2/Fence.cpp',
  'OpenGL/libGLESv2/Framebuffer.cpp',
  'OpenGL/libGLESv2/IndexDataManager.cpp',
  'OpenGL/libGLESv2/libGLESv2.cpp',
  'OpenGL/libGLESv2/libGLESv3.cpp',
  'OpenGL/libGLESv2/main.cpp',
  'OpenGL/libGLESv2/Program.cpp',
  'OpenGL/libGLESv2/Query.cpp',
  'OpenGL/libGLESv2/Renderbuffer.cpp',
  'OpenGL/libGLESv2/ResourceManager.cpp',
  'OpenGL/libGLESv2/Shader.cpp',
  'OpenGL/libGLESv2/Texture.cpp',
  'OpenGL/libGLESv2/TransformFeedback.cpp',
  'OpenGL/libGLESv2/utilities.cpp',
  'OpenGL/libGLESv2/VertexArray.cpp',
  'OpenGL/libGLESv2/VertexDataManager.cpp',
  'Reactor/Debug.cpp',
  'Reactor/ExecutableMemory.cpp',
  'Reactor/Reactor.cpp',
  'Reactor/ReactorDebugInfo.cpp',
  'Reactor/LLVMJIT.cpp',
  'Reactor/LLVMReactor.cpp',
  'Reactor/LLVMReactorDebugInfo.cpp',
  'Renderer/Blitter.cpp',
  'Renderer/Clipper.cpp',
  'Renderer/Context.cpp',
  'Renderer/ETC_Decoder.cpp',
  'Renderer/Matrix.cpp',
  'Renderer/PixelProcessor.cpp',
  'Renderer/Plane.cpp',
  'Renderer/Point.cpp',
  'Renderer/QuadRasterizer.cpp',
  'Renderer/Renderer.cpp',
  'Renderer/Sampler.cpp',
  'Renderer/SetupProcessor.cpp',
  'Renderer/Surface.cpp',
  'Renderer/TextureStage.cpp',
  'Renderer/VertexProcessor.cpp',
  'Shader/Constants.cpp',
  'Shader/PixelPipeline.cpp',
  'Shader/PixelProgram.cpp',
  'Shader/PixelRoutine.cpp',
  'Shader/PixelShader.cpp',
  'Shader/SamplerCore.cpp',
  'Shader/SetupRoutine.cpp',
  'Shader/Shader.cpp',
  'Shader/ShaderCore.cpp',
  'Shader/VertexPipeline.cpp',
  'Shader/VertexProgram.cpp',
  'Shader/VertexRoutine.cpp',
  'Shader/VertexShader.cpp'
]

libEGL = library('EGL', egl_sources,
                 include_directories: incdir,
                 dependencies: [directfb_dep, dl_dep, threads_dep],
                 version: '1',
                 install: true)

libGLESv2 = library('GLESv2', glesv2_sources,
                    include_directories: incdir,
                    dependencies: [directfb_dep, dl_dep, llvm_dep],
                    version: '2',
                    install: true)

pkgconfig.generate(name: 'egl',
                   description: 'SwiftShader EGL library',
                   extra_cflags: '-DEGL_PLATFORM_DIRECTFB_EXT=0x31DB',
                   libraries: '-L${libdir} -lEGL')

pkgconfig.generate(name: 'glesv2',
                   description: 'SwiftShader OpenGL ES 2.0 library',
                   libraries: '-L${libdir} -lGLESv2')
