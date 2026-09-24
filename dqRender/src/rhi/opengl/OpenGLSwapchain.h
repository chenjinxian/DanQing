// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL Swapchain implementation
// Ported from: filament filament/include/backend/SwapChain.h
//              PlatformCocoaGl.mm swap chain management
//
// Wraps the RHI Driver's SwapChain methods into a clean acquire/present cycle.
// On macOS, this binds an NSView to the NSOpenGLContext and manages FBO 0.
#pragma once

#include "dqRender/Swapchain.h"
#include "dqRender/rhi/Handle.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace rhi {
class Driver;
}

// ---------------------------------------------------------------------------
// OpenGLSwapchain — OpenGL Swapchain implementation
//
// Manages the GL context binding to a native window surface.
// acquire() makes the context current, present() swaps buffers.
// The render target is always FBO 0 (the default framebuffer).
// ---------------------------------------------------------------------------
class OpenGLSwapchain : public Swapchain {
public:
    /// Create an OpenGL Swapchain for the given native window.
    /// @param driver The RHI driver (must outlive this Swapchain).
    /// @param nativeWindow Platform-specific window handle (NSView* on macOS, HWND on Windows).
    /// @param width Initial width in pixels.
    /// @param height Initial height in pixels.
    OpenGLSwapchain(rhi::Driver& driver, void* nativeWindow, uint32_t width, uint32_t height);
    ~OpenGLSwapchain() override;

    OpenGLSwapchain(OpenGLSwapchain const&) = delete;
    OpenGLSwapchain& operator=(OpenGLSwapchain const&) = delete;

    // Swapchain interface
    bool isValid() const noexcept override;
    void resize(uint32_t width, uint32_t height) override;
    void rebind(void* nativeWindow) override;
    rhi::RenderTargetHandle acquire() override;
    void present() override;
    rhi::RenderTargetHandle getRenderTarget() const noexcept override;
    uint32_t getWidth() const noexcept override { return m_width; }
    uint32_t getHeight() const noexcept override { return m_height; }

private:
    rhi::Driver& m_driver;
    rhi::SwapChainHandle m_swapChain;
    rhi::RenderTargetHandle m_defaultTarget;
    void* m_nativeWindow = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

END_DQ_RENDER_NAMESPACE
