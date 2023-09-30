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

project('SwiftShaderGL-DirectFB', 'cpp',
        version: '4.1',
        meson_version: '>= 0.62',
        default_options: 'buildtype=release')

add_global_arguments('-DEGL_EGLEXT_PROTOTYPES',                                           language: 'cpp')
add_global_arguments('-DEGL_NO_PLATFORM_SPECIFIC_TYPES',                                  language: 'cpp')
add_global_arguments('-DEGL_PLATFORM_DIRECTFB_EXT=0x31DB',                                language: 'cpp')

add_global_arguments('-DMAJOR_VERSION=@0@'.format(meson.project_version().split('.')[0]), language: 'cpp')
add_global_arguments('-DMINOR_VERSION=@0@'.format(meson.project_version().split('.')[1]), language: 'cpp')

if not get_option('pool-alloc')
  add_global_arguments('-DDISABLE_POOL_ALLOC',                                            language: 'cpp')
endif

if get_option('named-mmap')
  add_global_arguments('-DENABLE_NAMED_MMAP',                                             language: 'cpp')
endif

if get_option('debug')
  add_global_arguments('-DENABLE_RR_DEBUG_INFO',                                          language: 'cpp')
  add_global_arguments('-DENABLE_RR_LLVM_IR_VERIFICATION',                                language: 'cpp')
  add_global_arguments('-DENABLE_RR_PRINT',                                               language: 'cpp')
else
  add_global_arguments('-DDISABLE_DEBUG',                                                 language: 'cpp')
  add_global_arguments('-Wno-strict-aliasing',                                            language: 'cpp')
endif

add_global_arguments('-Wno-attributes',                                                   language: 'cpp')
add_global_arguments('-Wno-ignored-attributes',                                           language: 'cpp')

pkgconfig = import('pkgconfig')

directfb_dep = dependency('directfb')
dl_dep       = dependency('dl')
llvm_dep     = dependency('llvm')
threads_dep  = dependency('threads')

egl_inc = []
if get_option('egl-registry-api') != ''
  egl_inc = include_directories(get_option('egl-registry-api'))
endif
gl_inc = []
if get_option('opengl-registry-api') != ''
  gl_inc = include_directories(get_option('gl-registry-api'))
endif
incdir = include_directories(join_paths(get_option('prefix'), get_option('includedir')))

if not meson.get_compiler('cpp').has_header('EGL/egl.h', include_directories: [egl_inc, incdir])
  error('EGL API headers not found')
endif

if not meson.get_compiler('cpp').has_header('GLES2/gl2.h', include_directories: [gl_inc, incdir])
  error('OpenGL API headers not found')
endif

incdir = [include_directories('src/OpenGL'), egl_inc, gl_inc, incdir]

subdir('src')
