// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Shadow display uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ShadowUniforms.ts
//
// Maintains state for uniforms related to shadow rendering.
// Uses Matrix4d (double-precision 4x4 matrix) for shadow projection computation,
// matching the reference implementation exactly.
#pragma once

#include "UniformHandle.h"
#include "FloatRGBA.h"
#include "Matrix.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/RgbColor.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Transform.h>

#include <array>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

using dqGeom::Matrix4d;
using dqGeom::Transform;

// ---------------------------------------------------------------------------
// SolarShadowMap — shadow map state
// Ported from: itwinjs-core SolarShadowMap
//
// Stores the shadow projection matrix and enabled state.
// ---------------------------------------------------------------------------
struct SolarShadowMap {
    bool isEnabled = false;
    Matrix4d projectionMatrix = Matrix4d::CreateIdentity();
    dqCommon::RgbColor color = dqCommon::RgbColor(0, 0, 0);
    float bias = 0.0f;
};

// ---------------------------------------------------------------------------
// ShadowUniforms — shadow display uniform handler
// Ported from: itwinjs-core ShadowUniforms
//
// Handles shadow rendering settings: enabled state, projection matrix,
// color, and bias. The projection matrix is computed as:
//   modelProj = projectionMatrix * modelMatrix
// where modelMatrix comes from the current branch transform.
// ---------------------------------------------------------------------------
class ShadowUniforms {
public:
    ShadowUniforms() = default;

    /// Check if shadows are enabled.
    bool isEnabled() const noexcept { return m_enabled; }

    /// Get the shadow color (RGB).
    dqCommon::RgbColor const& getColor() const noexcept { return m_color; }

    /// Get the shadow bias.
    float getBias() const noexcept { return m_bias; }

    /// Get the projection matrix (float32).
    Matrix4 const& getProjectionMatrix() const noexcept { return m_projection32; }

    /// Update shadow uniforms from shadow map state.
    /// Ported from: itwinjs-core ShadowUniforms.update()
    void update(SolarShadowMap const& shadowMap)
    {
        m_enabled = shadowMap.isEnabled;

        if (m_bias != shadowMap.bias) {
            m_bias = shadowMap.bias;
            m_colorAndBias.a = m_bias;
        }

        if (!m_color.equals(shadowMap.color)) {
            m_color = shadowMap.color;
            m_colorAndBias = FloatRgba(
                m_color.r / 255.0f,
                m_color.g / 255.0f,
                m_color.b / 255.0f,
                m_bias);
        }

        // Store projection matrix (will be combined with model matrix at bind time)
        if (!shadowMap.projectionMatrix.IsExactEqual(m_projectionMatrix)) {
            m_projectionMatrix.SetFrom(shadowMap.projectionMatrix);
        }
    }

    /// Compute and bind projection matrix.
    /// Ported from: itwinjs-core ShadowUniforms.computeProjection()
    ///
    /// Computes: modelProj = projectionMatrix * modelMatrix
    /// Then converts to float32 for GPU upload.
    void computeProjection(Transform const& modelTransform)
    {
        // Convert Transform to Matrix4d
        Matrix4d model = Matrix4d::CreateTransform(modelTransform);

        // Compute: modelProj = projectionMatrix * modelMatrix
        Matrix4d modelProj = m_projectionMatrix.MultiplyMatrixMatrix(model);

        // Convert double-precision to float32 for GPU upload
        // Ported from: itwinjs-core Matrix4.initFromMatrix4d()
        for (int i = 0; i < 16; ++i)
            m_projection32.data[i] = static_cast<float>(modelProj.GetCoeffs()[i]);
    }

    /// Bind color and bias uniform (vec4).
    /// Ported from: itwinjs-core ShadowUniforms.bindColorAndBias()
    void bindColorAndBias(UniformHandle& uniform) const
    {
        float v[4] = {m_colorAndBias.r, m_colorAndBias.g, m_colorAndBias.b, m_colorAndBias.a};
        uniform.setUniform4fv(v);
    }

    /// Bind projection matrix uniform (mat4).
    /// Ported from: itwinjs-core ShadowUniforms.bindProjectionMatrix()
    void bindProjectionMatrix(UniformHandle& uniform) const
    {
        uniform.setMatrix4(m_projection32.data);
    }

private:
    // CPU state
    bool m_enabled = false;
    dqCommon::RgbColor m_color = dqCommon::RgbColor(0, 0, 0);
    float m_bias = 0.0f;
    Matrix4d m_projectionMatrix = Matrix4d::CreateIdentity();

    // GPU state
    FloatRgba m_colorAndBias;
    Matrix4 m_projection32;
};

END_DQ_RENDER_NAMESPACE
