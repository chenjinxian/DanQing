// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderSystem implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/System.ts
//
// This replaces itwinjs's System class (WebGL wrapper).  Instead of holding a
// WebGL2RenderingContext, it holds a Filament Driver.  All GL calls that
// System.ts made through `this.context.*` are redirected to `m_driver->*`.
//
// Key integration point: itwinjs rendering logic → Filament RHI.
#pragma once

#include "dqRender/Export.h"
#include "dqRender/RenderTarget.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"
#include "gl/GL.h"
#include "render/GLTimer.h"   // System.ts:822 glTimer = GLTimer.create(this)

#include <array>
#include <cstdint>
#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class TargetImpl;
class Techniques;

// ViewRect is now defined in dqRender/RenderTarget.h (public API)

// ---------------------------------------------------------------------------
// RenderSystemImpl — the central rendering system
//
// Replaces itwinjs System (which wraps WebGL).  This holds the Filament
// Driver and provides factory methods for creating rendering resources.
//
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/System.ts
//              itwinjs-core core/frontend/src/render/RenderSystem.ts
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT RenderSystemImpl {
public:
    explicit RenderSystemImpl(std::unique_ptr<rhi::Driver> driver);
    ~RenderSystemImpl();

    RenderSystemImpl(RenderSystemImpl const&) = delete;
    RenderSystemImpl& operator=(RenderSystemImpl const&) = delete;

    /// Get the underlying RHI driver.
    rhi::Driver& getDriver() noexcept { return *m_driver; }
    rhi::Driver const& getDriver() const noexcept { return *m_driver; }

    /// GPU timer queries (System.ts:822 glTimer + :967 isGLTimerSupported).
    GLTimer& glTimer() noexcept { return m_glTimer; }
    /// Debug controls forwarded by OpenGLRenderSystem::debugControl
    /// (System.ts:967-970: isGLTimerSupported / resultsCallback surface).
    RenderSystemDebugControl* debugControl() noexcept { return &m_glTimer.debugControl(); }

    /// Create a render target for the given view rectangle.
    /// Replaces: System.createTarget(canvas) / System.createOffscreenTarget(rect)
    TargetImpl* createTarget(ViewRect const& rect, Techniques& techniques);

    /// Begin a new frame.
    void beginFrame(int64_t monotonicClockNs, int64_t refreshIntervalNs,
                    uint32_t frameId);

    /// End the current frame.
    void endFrame(uint32_t frameId);

    /// Terminate the rendering system.
    void terminate();

    // --- Texture binding cache ---
    // Ported from: itwinjs-core System.ts bindTexture2d/activateTexture2d/disposeTexture
    //
    // Tracks which texture is bound to which unit to avoid redundant GL calls.
    // The itwinjs-core System._textureBindings[12] array is replicated here.

    /// Bind a 2D texture to a texture unit (may or may not make active).
    /// Ported from: itwinjs-core System.bindTexture2d()
    void bindTexture2d(uint32_t unit, rhi::TextureHandle texture);

    /// Activate and bind a 2D texture to a texture unit.
    /// Ported from: itwinjs-core System.activateTexture2d()
    void activateTexture2d(uint32_t unit, rhi::TextureHandle texture);

    /// Bind a cube map texture to a texture unit.
    /// Ported from: itwinjs-core System.bindTextureCubeMap()
    void bindTextureCubeMap(uint32_t unit, rhi::TextureHandle texture);

    /// Remove a texture from the binding cache (call before destroying).
    /// Ported from: itwinjs-core System.disposeTexture()
    void disposeTexture(rhi::TextureHandle texture);

    // --- Vertex attribute state management ---
    // Ported from: itwinjs-core System.ts enableVertexAttribArray/updateVertexAttribArrays
    //
    // Tracks enabled/disabled/instanced state of vertex attrib arrays to avoid
    // redundant GL calls and prevent errors from leaving attribs enabled.

    /// Mark a vertex attribute as enabled (optionally instanced).
    /// Ported from: itwinjs-core System.enableVertexAttribArray()
    void enableVertexAttribArray(uint32_t id, bool instanced);

    /// Flush pending vertex attribute state changes to GL.
    /// Ported from: itwinjs-core System.updateVertexAttribArrays()
    void updateVertexAttribArrays();

    /// Set vertex attribute divisor (for instanced rendering).
    /// Ported from: itwinjs-core System.vertexAttribDivisor()
    void vertexAttribDivisor(uint32_t index, uint32_t divisor);

    /// Get max texture size supported by the GPU.
    uint32_t getMaxTextureSize() const noexcept { return m_maxTextureSize; }

private:
    std::unique_ptr<rhi::Driver> m_driver;
    GLTimer m_glTimer;   // System.ts:822（System 单例计时器——一上下文一实例）
    rhi::SwapChainHandle m_swapChain;
    rhi::RenderTargetHandle m_defaultRenderTarget;

    // Texture binding cache (Ported from: itwinjs-core System._textureBindings)
    // Each entry tracks which texture handle is bound to that unit.
    static constexpr uint32_t kMaxTextureUnits = 16;
    std::array<rhi::TextureHandle, kMaxTextureUnits> m_textureBindings = {};

    // Vertex attribute state (Ported from: itwinjs-core System._curVertexAttribStates/_nextVertexAttribStates)
    // Uses bit flags: bit 0 = Enabled, bit 1 = Instanced
    static constexpr uint8_t kVAEnabled = 1;
    static constexpr uint8_t kVAInstanced = 2;
    static constexpr uint32_t kMaxVertexAttribs = 16;
    std::array<uint8_t, kMaxVertexAttribs> m_curVertexAttribStates = {};
    std::array<uint8_t, kMaxVertexAttribs> m_nextVertexAttribStates = {};

    // GPU capabilities
    uint32_t m_maxTextureSize = 4096;
};

END_DQ_RENDER_NAMESPACE
