// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Viewport rectangle uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ViewRectUniforms.ts
//
// Maintains uniform state associated with a Target's ViewRect:
//   - orthographic projection matrix mapping window pixels to clip space
//   - viewport matrix mapping NDC to window coordinates
//   - dimensions and inverse dimensions
#pragma once

#include "Matrix.h"
#include "UniformHandle.h"

#include <dqGeom/Matrix4d.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// ViewRectUniforms — Target ViewRect uniform state
// Ported from: itwinjs-core ViewRectUniforms
//
// The projection is an ortho map of the window rectangle (pixels), with the
// Y axis flipped so that row 0 is at the bottom (height .. 0 in reference's
// fromOrtho argument order). The viewport matrix maps NDC [-1,1] to the
// window rectangle [0,width] x [0,height] x [0,1].
// ---------------------------------------------------------------------------
class ViewRectUniforms {
public:
    ViewRectUniforms() noexcept
    {
        // Reference initializes projectionMatrix = Matrix4d.createIdentity().
        m_projection = dqGeom::Matrix4d::CreateIdentity();
    }

    /// Recompute uniforms for a new ViewRect size.
    /// Ported from: itwinjs-core ViewRectUniforms.update()
    void update(float width, float height) noexcept
    {
        if (width == m_dimensions[0] && height == m_dimensions[1])
            return;

        m_dimensions[0] = width;
        m_dimensions[1] = height;

        // Matrix4.fromOrtho(0.0, width, height, 0.0, -1.0, 1.0, projectionMatrix32)
        m_projection32 = Matrix4::fromOrtho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
        m_projection = m_projection32.toMatrix4d();

        m_inverseDimensions[0] = (0.0f != width) ? 1.0f / width : 0.0f;
        m_inverseDimensions[1] = (0.0f != height) ? 1.0f / height : 0.0f;

        // Viewport matrix: NDC -> window.  Near/far depth range is [0, 1].
        buildViewportMatrix(0.0f, 0.0f, width, height, 0.0f, 1.0f, m_viewportMatrix);
    }

    float getWidth() const noexcept { return m_dimensions[0]; }
    float getHeight() const noexcept { return m_dimensions[1]; }

    dqGeom::Matrix4d const& getProjectionMatrix() const noexcept { return m_projection; }
    Matrix4 const& getProjectionMatrix32() const noexcept { return m_projection32; }
    Matrix4 const& getViewportMatrix() const noexcept { return m_viewportMatrix; }

    /// Bind the ortho projection matrix (mat4).
    /// Ported from: itwinjs-core ViewRectUniforms.bindProjectionMatrix()
    void bindProjectionMatrix(UniformHandle& uniform) const
    {
        uniform.setMatrix4(m_projection32.data);
    }

    /// Bind the dimensions (vec2).
    /// Ported from: itwinjs-core ViewRectUniforms.bindDimensions()
    void bindDimensions(UniformHandle& uniform) const
    {
        uniform.setUniform2fv(m_dimensions);
    }

    /// Bind the inverse dimensions (vec2).
    /// Ported from: itwinjs-core ViewRectUniforms.bindInverseDimensions()
    void bindInverseDimensions(UniformHandle& uniform) const
    {
        uniform.setUniform2fv(m_inverseDimensions);
    }

    /// Bind the viewport matrix (mat4).
    /// Ported from: itwinjs-core ViewRectUniforms.bindViewportMatrix()
    void bindViewportMatrix(UniformHandle& uniform) const
    {
        uniform.setMatrix4(m_viewportMatrix.data);
    }

private:
    // Build the NDC->window viewport matrix.
    // Ported from: itwinjs-core ViewRectUniforms.update() viewport block.
    static void buildViewportMatrix(float x, float y, float width, float height,
                                    float nearDepthRange, float farDepthRange,
                                    Matrix4& out) noexcept
    {
        float halfWidth = width * 0.5f;
        float halfHeight = height * 0.5f;
        float halfDepth = (farDepthRange - nearDepthRange) * 0.5f;

        float column0Row0 = halfWidth;
        float column1Row1 = halfHeight;
        float column2Row2 = halfDepth;
        float column3Row0 = x + halfWidth;
        float column3Row1 = y + halfHeight;
        float column3Row2 = nearDepthRange + halfDepth;
        float column3Row3 = 1.0f;

        out = Matrix4::fromValues(
            column0Row0, 0.0f, 0.0f, column3Row0,
            0.0f, column1Row1, 0.0f, column3Row1,
            0.0f, 0.0f, column2Row2, column3Row2,
            0.0f, 0.0f, 0.0f, column3Row3);
    }

    // CPU state
    dqGeom::Matrix4d m_projection;  // double-precision projection
    float m_dimensions[2] = {0.0f, 0.0f};
    float m_inverseDimensions[2] = {0.0f, 0.0f};

    // GPU state
    Matrix4 m_projection32;
    Matrix4 m_viewportMatrix;
};

END_DQ_RENDER_NAMESPACE
