// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Linux GLX platform implementation
// Ported from: filament backend/src/opengl/platforms/PlatformGlx.cpp
#ifdef __linux__

#include "PlatformGlx.h"
#include "rhi/opengl/OpenGLDriver.h"

#include <X11/Xlib.h>
#include <GL/glx.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

void PlatformGlx::terminate() noexcept
{
    if (m_context && !m_externalContext) {
        glXMakeCurrent(m_display, None, nullptr);
        glXDestroyContext(m_display, m_context);
        m_context = nullptr;
    }
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

void PlatformGlx::setExternalContext(void* nativeContext)
{
    if (nativeContext) {
        m_context = static_cast<GLXContext>(nativeContext);
        m_externalContext = true;
    }
}

Driver* PlatformGlx::createDriver(void* sharedContext, DriverConfig const& config)
{
    if (!m_display) {
        m_display = XOpenDisplay(nullptr);
        if (!m_display) return nullptr;
    }

    if (!m_context) {
        // Get an appropriate framebuffer configuration
        int visualAttribs[] = {
            GLX_RGBA,
            GLX_DOUBLEBUFFER,
            GLX_DEPTH_SIZE, 24,
            GLX_STENCIL_SIZE, 8,
            None
        };

        XVisualInfo* vi = glXChooseVisual(m_display, DefaultScreen(m_display), visualAttribs);
        if (!vi) return nullptr;

        // Create GLX context
        GLXContext sharedCtx = static_cast<GLXContext>(sharedContext);
        m_context = glXCreateContext(m_display, vi, sharedCtx, GL_TRUE);
        XFree(vi);

        if (!m_context) return nullptr;
    }

    // Make the context current (need a drawable)
    // We'll create a temporary window for initialization
    Window root = DefaultRootWindow(m_display);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(m_display, root, DefaultVisual(m_display, DefaultScreen(m_display)), AllocNone);
    Window win = XCreateWindow(m_display, root, 0, 0, 1, 1, 0,
                                DefaultDepth(m_display, DefaultScreen(m_display)),
                                InputOutput, DefaultVisual(m_display, DefaultScreen(m_display)),
                                CWColormap, &swa);

    glXMakeCurrent(m_display, win, m_context);

    // Create the OpenGL driver
    auto* driver = new OpenGLDriver(*this, sharedContext, config);
    driver->initializeGL();
    return driver;
}

void* PlatformGlx::createSwapChain(void* nativeWindow, uint64_t)
{
    // For GLX, the swap chain is just the Window
    return nativeWindow;
}

void* PlatformGlx::createSwapChain(uint32_t width, uint32_t height, uint64_t)
{
    if (!m_display) return nullptr;

    // Headless: create a hidden window
    Window root = DefaultRootWindow(m_display);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(m_display, root, DefaultVisual(m_display, DefaultScreen(m_display)), AllocNone);
    Window win = XCreateWindow(m_display, root, 0, 0, width, height, 0,
                                DefaultDepth(m_display, DefaultScreen(m_display)),
                                InputOutput, DefaultVisual(m_display, DefaultScreen(m_display)),
                                CWColormap, &swa);
    XMapWindow(m_display, win);
    return reinterpret_cast<void*>(win);
}

void PlatformGlx::destroySwapChain(void* swapChain) noexcept
{
    if (swapChain && m_display) {
        Window win = reinterpret_cast<Window>(swapChain);
        XDestroyWindow(m_display, win);
    }
}

bool PlatformGlx::makeCurrent(void* draw, void* /*read*/) noexcept
{
    if (!m_context || !m_display) return false;

    if (draw) {
        Window win = reinterpret_cast<Window>(draw);
        glXMakeCurrent(m_display, win, m_context);
    } else {
        glXMakeCurrent(m_display, None, m_context);
    }
    return true;
}

void PlatformGlx::commit(void* swapChain) noexcept
{
    if (swapChain && m_display) {
        Window win = reinterpret_cast<Window>(swapChain);
        glXSwapBuffers(m_display, win);
    }
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

#endif  // __linux__
