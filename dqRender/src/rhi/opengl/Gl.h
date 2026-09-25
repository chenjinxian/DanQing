// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL function loader
// Ported from: filament backend/src/opengl/Gl.h
//
// On macOS, OpenGL functions are available directly via the framework.
// On other platforms, we'd use GLAD or similar.  This header provides a
// unified include for GL functions.
#pragma once

#if defined(__APPLE__)
    #ifndef GL_SILENCE_DEPRECATION
        #define GL_SILENCE_DEPRECATION
    #endif
    #include <OpenGL/gl3.h>
    #include <OpenGL/gl3ext.h>
#elif defined(_WIN32)
    // Windows SDK 仅带 GL 1.1 头（um/gl/GL.h），现代常量/glext 不随 SDK 发行。
    // 用 Khronos 官方 glcorearb.h（filament 同做法：vendor 第三方 GL 头），
    // 自包含（GLenum/常量/函数指针类型 + WGL 扩展），需先含 windows.h。
    #include <windows.h>
    // Windows 头宏污染清理 —— Ported from: filament
    //   libs/utils/include/utils/unwindows.h（1:1，#ifdef 包裹形式保留）：
    //   max/min（NOMINMAX 已挡，双保险）、near/far（minwindef.h 16 位遗留，
    //   破坏 filament 式 DepthRange{near,far}）、ERROR（wingdi.h，破坏
    //   FenceStatus::ERROR）、OPAQUE/TRANSPARENT/PURE（wingdi.h 枚举名冲突）。
    #ifdef max
    #undef max
    #endif
    #ifdef min
    #undef min
    #endif
    #ifdef far
    #undef far
    #endif
    #ifdef near
    #undef near
    #endif
    #ifdef ERROR
    #undef ERROR
    #endif
    #ifdef OPAQUE
    #undef OPAQUE
    #endif
    #ifdef TRANSPARENT
    #undef TRANSPARENT
    #endif
    #ifdef PURE
    #undef PURE
    #endif
    #include <GL/glcorearb.h>
    // Windows GL 运行时装载（bluegl 机制）：glXxx 映射到函数指针，
    // 由 dqgl::init()（PlatformFactory::createPlatform）解析
    #include "GlLoader.h"
#else
    #include <GL/gl.h>
    #include <GL/glext.h>
#endif

// Ensure we have the types we need
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
