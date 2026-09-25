// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Platform factory implementation
// Authored: platform abstraction layer for Qt/OpenGL, no direct reference equivalent
#include "PlatformFactory.h"

#ifdef _WIN32
#include "PlatformWgl.h"
#include "rhi/opengl/GlLoader.h" // dqgl::init —— Windows GL 运行时符号装载
#elif defined(__linux__)
#include "PlatformGlx.h"
#endif

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

OpenGLPlatform* createPlatform()
{
#if defined(__APPLE__)
    return createPlatformCocoaGl();
#elif defined(_WIN32)
    dqgl::init(); // bluegl 机制：任何 GL 调用前解析全部 glXxx 函数指针
    return new PlatformWgl();
#elif defined(__linux__)
    return new PlatformGlx();
#else
    return nullptr;
#endif
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
