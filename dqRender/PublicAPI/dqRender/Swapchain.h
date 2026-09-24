// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Swapchain abstract interface
//
// Authored: DanQing swapchain abstraction (native-window GL surface + present lifecycle).
// filament filament/include/backend/SwapChain.h inspired the type name only — filament
// SwapChain is config/callbacks and has NO Acquire/Present/Resize/isValid/getWidth/getHeight
// (audit F2 misattribution). Closest analog is filament backend Driver beginFrame/commit.
// itwinjs RenderTarget.ts (canvas separation) is a conceptual anchor.
//
// A Swapchain represents a renderable surface tied to a native window.
// It manages the GL context binding and buffer presentation.
//
// The rendering loop per frame:
//   1. acquire() — bind the GL context, return the render target (FBO 0)
//   2. Render to the render target (via SceneCompositor or direct RHI calls)
//   3. present() — swap buffers to display on screen
//
// OpenGL: acquire() calls makeCurrent(), present() calls commit().
// Vulkan: acquire() acquires next VkImage, present() queues present.
#pragma once

#include "Export.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Swapchain — abstract renderable surface
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT Swapchain {
public:
    virtual ~Swapchain() = default;

    /// Check if the swapchain is valid and ready for rendering.
    virtual bool isValid() const noexcept = 0;

    /// Resize the swapchain to match the new window dimensions.
    /// On OpenGL, this updates the viewport. On Vulkan, this recreates the swapchain.
    /// @param width New width in pixels.
    /// @param height New height in pixels.
    virtual void resize(uint32_t width, uint32_t height) = 0;

    /// Rebind the swapchain to a (new) native window handle.
    /// Window systems may destroy and recreate a widget's native window on
    /// re-parenting or window-state transitions (e.g. Qt re-creates WA_NativeWindow
    /// children under MDI re-layout/maximize); the old surface handle goes dead and
    /// presents silently stop reaching the screen. The window lifecycle belongs to
    /// the app layer — this call lets it hand us the current handle; the GL side
    /// re-creates its platform surface (pure graphics API, no windowing toolkit
    /// dependency).
    /// @param nativeWindow Current native window handle (HWND / NSView*).
    virtual void rebind(void* nativeWindow) = 0;

    /// Acquire the next image for rendering.
    /// Binds the GL context to the native window surface.
    /// @return The render target handle (FBO 0 on OpenGL, VkImage target on Vulkan).
    virtual rhi::RenderTargetHandle acquire() = 0;

    /// Present the rendered image to the display.
    /// Swaps buffers (OpenGL) or queues a present command (Vulkan).
    virtual void present() = 0;

    /// Get the render target for the swapchain surface.
    /// This is FBO 0 on OpenGL, an MRT target on Vulkan.
    virtual rhi::RenderTargetHandle getRenderTarget() const noexcept = 0;

    /// Get the current width in pixels.
    virtual uint32_t getWidth() const noexcept = 0;

    /// Get the current height in pixels.
    virtual uint32_t getHeight() const noexcept = 0;
};

END_DQ_RENDER_NAMESPACE
