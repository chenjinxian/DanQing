// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — EGL platform implementation
// Ported from: filament backend/src/opengl/platforms/PlatformEgl.cpp
#ifdef DQ_RENDER_HAS_EGL

#include "PlatformEgl.h"
#include "rhi/opengl/OpenGLDriver.h"

#include <EGL/egl.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

void PlatformEgl::terminate() noexcept
{
    if (m_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(m_display, m_context);
        m_context = EGL_NO_CONTEXT;
    }
    if (m_surface != EGL_NO_SURFACE) {
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
    }
    if (m_display != EGL_NO_DISPLAY) {
        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;
    }
}

Driver* PlatformEgl::createDriver(void* sharedContext, DriverConfig const& config)
{
    // Get default display
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) return nullptr;

    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(m_display, &major, &minor)) return nullptr;

    // Choose config
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    eglChooseConfig(m_display, configAttribs, &m_config, 1, &numConfigs);
    if (numConfigs == 0) return nullptr;

    // Create pbuffer surface for headless rendering
    EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    m_surface = eglCreatePbufferSurface(m_display, m_config, pbufferAttribs);

    // Bind OpenGL API
    eglBindAPI(EGL_OPENGL_API);

    // Create context
    EGLContext sharedCtx = static_cast<EGLContext>(sharedContext);
    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 1,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    m_context = eglCreateContext(m_display, m_config, sharedCtx, contextAttribs);
    if (m_context == EGL_NO_CONTEXT) return nullptr;

    // Make current
    eglMakeCurrent(m_display, m_surface, m_surface, m_context);

    // Create the OpenGL driver
    auto* driver = new OpenGLDriver(*this, sharedContext, config);
    driver->initializeGL();
    return driver;
}

void* PlatformEgl::createSwapChain(void* /*nativeWindow*/, uint64_t)
{
    // For EGL, we use the pbuffer surface
    return static_cast<void*>(m_surface);
}

void* PlatformEgl::createSwapChain(uint32_t width, uint32_t height, uint64_t)
{
    // Create a new pbuffer surface with the requested size
    EGLint pbufferAttribs[] = {
        EGL_WIDTH, static_cast<EGLint>(width),
        EGL_HEIGHT, static_cast<EGLint>(height),
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(m_display, m_config, pbufferAttribs);
    return static_cast<void*>(surface);
}

void PlatformEgl::destroySwapChain(void* swapChain) noexcept
{
    if (swapChain && m_display) {
        EGLSurface surface = static_cast<EGLSurface>(swapChain);
        if (surface != m_surface) {
            eglDestroySurface(m_display, surface);
        }
    }
}

bool PlatformEgl::makeCurrent(void* draw, void* /*read*/) noexcept
{
    if (m_context == EGL_NO_CONTEXT || m_display == EGL_NO_DISPLAY) return false;

    EGLSurface surface = draw ? static_cast<EGLSurface>(draw) : m_surface;
    return eglMakeCurrent(m_display, surface, surface, m_context) == EGL_TRUE;
}

void PlatformEgl::commit(void* /*swapChain*/) noexcept
{
    // eglSwapBuffers is not needed for pbuffer surfaces
    // For window surfaces, we would call eglSwapBuffers here
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

#endif  // DQ_RENDER_HAS_EGL
