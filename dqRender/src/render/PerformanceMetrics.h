// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Performance metrics
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PerformanceMetrics.ts
//
// Collects per-frame rendering performance metrics (draw call counts,
// triangle counts, etc.).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PerformanceMetrics — per-frame rendering statistics
// (Ported from: itwinjs-core PerformanceMetrics.ts)
// ---------------------------------------------------------------------------
class PerformanceMetrics {
public:
    PerformanceMetrics() = default;

    /// Reset all counters for a new frame.
    void reset() noexcept
    {
        m_drawCalls = 0;
        m_triangles = 0;
        m_lines = 0;
        m_points = 0;
        m_textureBindings = 0;
        m_shaderBindings = 0;
        m_frameBufferBindings = 0;
    }

    /// Record a draw call.
    void recordDrawCall(uint32_t triangleCount = 0) noexcept
    {
        ++m_drawCalls;
        m_triangles += triangleCount;
    }

    /// Record a line draw call.
    void recordLineDraw(uint32_t lineCount) noexcept
    {
        ++m_drawCalls;
        m_lines += lineCount;
    }

    /// Record a point draw call.
    void recordPointDraw(uint32_t pointCount) noexcept
    {
        ++m_drawCalls;
        m_points += pointCount;
    }

    /// Record a texture binding.
    void recordTextureBind() noexcept { ++m_textureBindings; }

    /// Record a shader binding.
    void recordShaderBind() noexcept { ++m_shaderBindings; }

    /// Record a framebuffer binding.
    void recordFrameBufferBind() noexcept { ++m_frameBufferBindings; }

    // --- Accessors ---
    uint32_t getDrawCalls() const noexcept { return m_drawCalls; }
    uint32_t getTriangles() const noexcept { return m_triangles; }
    uint32_t getLines() const noexcept { return m_lines; }
    uint32_t getPoints() const noexcept { return m_points; }
    uint32_t getTextureBindings() const noexcept { return m_textureBindings; }
    uint32_t getShaderBindings() const noexcept { return m_shaderBindings; }
    uint32_t getFrameBufferBindings() const noexcept { return m_frameBufferBindings; }

private:
    uint32_t m_drawCalls = 0;
    uint32_t m_triangles = 0;
    uint32_t m_lines = 0;
    uint32_t m_points = 0;
    uint32_t m_textureBindings = 0;
    uint32_t m_shaderBindings = 0;
    uint32_t m_frameBufferBindings = 0;
};

END_DQ_RENDER_NAMESPACE
