// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL platform abstraction
// Ported from: filament backend/include/backend/platforms/OpenGLPlatform.h
//
// Extends Platform with GL-specific operations.  Each platform (macOS, Linux,
// Windows) must implement the pure virtual methods for context creation,
// swap chain management, and buffer presentation.
#pragma once

#include "Platform.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// OpenGLPlatform — GL-specific platform operations
// ---------------------------------------------------------------------------
class OpenGLPlatform : public Platform {
public:
    ~OpenGLPlatform() override = default;

    /// Shut down the platform (release contexts, etc.).
    virtual void terminate() noexcept = 0;

    /// Create a swap chain from a native window handle.
    virtual void* createSwapChain(void* nativeWindow, uint64_t flags) = 0;

    /// Create a headless swap chain.
    virtual void* createSwapChain(uint32_t width, uint32_t height,
                                  uint64_t flags) = 0;

    /// Destroy a swap chain.
    virtual void destroySwapChain(void* swapChain) noexcept = 0;

    /// Make the given swap chain current for rendering.
    /// @param draw The swap chain to render to.
    /// @param read The swap chain to read from (may be same as draw).
    /// @return true if the context was successfully made current.
    virtual bool makeCurrent(void* draw, void* read) = 0;

    /// Present the rendered frame (swap buffers).
    virtual void commit(void* swapChain) noexcept = 0;

    /// Get the default framebuffer object (0 for most platforms).
    virtual uint32_t getDefaultFramebufferObject() const noexcept { return 0; }

    /// Set an external GL context to use instead of creating a new one.
    /// This allows Qt's QOpenGLContext to be used as the rendering context.
    /// @param nativeContext Platform-specific context handle (e.g., NSOpenGLContext* on macOS)
    virtual void setExternalContext(void* nativeContext) { (void)nativeContext; }
};

/// Factory: create a macOS OpenGL platform instance.
OpenGLPlatform* createPlatformCocoaGl();

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
