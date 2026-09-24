// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Framebuffer management
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/FrameBuffer.ts
//
// Manages framebuffers (FBOs) with color texture attachments and optional
// multisampling.
//
// Integration point: itwinjs held WebGLFramebuffer and called
// gl.bindFramebuffer().  In the RHI approach, FBOs are RenderTargets
// managed by the Driver.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FrameBuffer — wraps a RenderTarget
// (Ported from: itwinjs-core FrameBuffer.ts)
// ---------------------------------------------------------------------------
class FrameBuffer {
public:
    FrameBuffer() = default;
    ~FrameBuffer() = default;

    /// Create a framebuffer from a render target handle.
    static FrameBuffer create(rhi::RenderTargetHandle renderTarget);

    /// Get the render target handle.
    rhi::RenderTargetHandle getRenderTarget() const noexcept { return m_renderTarget; }

private:
    rhi::RenderTargetHandle m_renderTarget;
};

// ---------------------------------------------------------------------------
// FrameBufferStack — stack of framebuffers for nested rendering
// (Ported from: itwinjs-core FrameBuffer.ts FrameBufferStack)
// ---------------------------------------------------------------------------
class FrameBufferStack {
public:
    FrameBufferStack() = default;

    /// Push a framebuffer and begin rendering to it.
    void push(rhi::Driver& driver, FrameBuffer const& fb,
              rhi::RenderPassParams const& params);

    /// Pop the current framebuffer.
    void pop(rhi::Driver& driver);

    /// Get the current framebuffer.
    FrameBuffer const* getCurrent() const;

private:
    std::vector<FrameBuffer> m_stack;
};

END_DQ_RENDER_NAMESPACE
