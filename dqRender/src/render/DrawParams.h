// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Draw parameters
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/DrawCommand.ts
//
// DrawParams carries the context needed to execute a single draw call:
// the shader program parameters, the cached geometry, and derived state.
// A recycled singleton pattern avoids per-draw-call allocation.
#pragma once

#include "CachedGeometry.h"

#include <array>
#include <cstring>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class ShaderProgramParams;
class TargetImpl;

// ---------------------------------------------------------------------------
// DrawParams — context for a single draw call
// (Ported from: itwinjs-core DrawCommand.ts DrawParams)
// ---------------------------------------------------------------------------
class DrawParams {
public:
    DrawParams() = default;

    void init(ShaderProgramParams* params, CachedGeometry* geometry)
    {
        m_programParams = params;
        m_geometry = geometry;
    }

    void reset()
    {
        m_programParams = nullptr;
        m_geometry = nullptr;
        m_target = nullptr;
        m_mv = nullptr;
        m_mvp = nullptr;
        m_materialColor = nullptr;
        m_materialParams = nullptr;
        m_normalMatrix = nullptr;
        m_texture = {};
        m_textureUnit = -1;
        m_surfaceFlags.fill(0);
    }

    ShaderProgramParams* getProgramParams() const noexcept { return m_programParams; }
    CachedGeometry* getGeometry() const noexcept { return m_geometry; }
    TargetImpl* getTarget() const noexcept { return m_target; }
    void setTarget(TargetImpl* t) noexcept { m_target = t; }

    // --- Per-draw matrix state (from BranchUniforms) ---
    void setModelViewMatrix(float const* mv) noexcept { m_mv = mv; }
    void setModelViewProjectionMatrix(float const* mvp) noexcept { m_mvp = mvp; }
    float const* getModelViewMatrix() const noexcept { return m_mv; }
    float const* getModelViewProjectionMatrix() const noexcept { return m_mvp; }

    // --- Per-draw material state ---
    void setMaterialColor(float const* rgba) noexcept { m_materialColor = rgba; }
    float const* getMaterialColor() const noexcept { return m_materialColor; }

    // --- Per-draw material params (u_materialParams, vec4) ---
    // Ported from: itwinjs-core Surface.ts addMaterial() (line 214-220)
    void setMaterialParams(float const* mp) noexcept { m_materialParams = mp; }
    float const* getMaterialParams() const noexcept { return m_materialParams; }

    // --- Per-draw surface flags (u_surfaceFlags[12], SurfaceBitIndex) ---
    // Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 519-527)
    void setSurfaceFlags(int const* flags) noexcept
    {
        if (flags) std::memcpy(m_surfaceFlags.data(), flags, m_surfaceFlags.size() * sizeof(int));
        else m_surfaceFlags.fill(0);
    }
    int const* getSurfaceFlags() const noexcept { return m_surfaceFlags.data(); }

    // --- Per-draw normal matrix (u_normalMatrix, mat3 = 9 floats) ---
    // Ported from: itwinjs-core Vertex.ts addNormalMatrix()
    void setNormalMatrix(float const* nm) noexcept { m_normalMatrix = nm; }
    float const* getNormalMatrix() const noexcept { return m_normalMatrix; }

    // --- Per-draw surface texture (s_texture) ---
    // Ported from: itwinjs-core Surface.ts addTexture() (line 596-609)
    void setTexture(rhi::TextureHandle texture) noexcept { m_texture = texture; }
    rhi::TextureHandle getTexture() const noexcept { return m_texture; }
    void setTextureUnit(int unit) noexcept { m_textureUnit = unit; }
    int getTextureUnit() const noexcept { return m_textureUnit; }

    // --- Animation displacement state ---
    void setAnimLutTexture(rhi::TextureHandle tex) noexcept { m_animLutTexture = tex; }
    rhi::TextureHandle getAnimLutTexture() const noexcept { return m_animLutTexture; }
    void setAnimLutParams(float const* p) noexcept { m_animLutParams = p; }
    float const* getAnimLutParams() const noexcept { return m_animLutParams; }
    void setAnimDispParams(float const* p) noexcept { m_animDispParams = p; }
    float const* getAnimDispParams() const noexcept { return m_animDispParams; }
    void setQAnimDispOrigin(float const* p) noexcept { m_qAnimDispOrigin = p; }
    float const* getQAnimDispOrigin() const noexcept { return m_qAnimDispOrigin; }
    void setQAnimDispScale(float const* p) noexcept { m_qAnimDispScale = p; }
    float const* getQAnimDispScale() const noexcept { return m_qAnimDispScale; }

    Pass getRenderPass() const
    {
        return m_geometry ? m_geometry->getPass() : Pass::None;
    }

    TechniqueId getTechniqueId() const
    {
        return m_geometry ? m_geometry->getTechniqueId() : TechniqueId::Surface;
    }

    bool isOverlayPass() const
    {
        return getRenderPass() == Pass::ViewOverlay;
    }

private:
    ShaderProgramParams* m_programParams = nullptr;
    CachedGeometry* m_geometry = nullptr;
    TargetImpl* m_target = nullptr;
    float const* m_mv = nullptr;
    float const* m_mvp = nullptr;
    float const* m_materialColor = nullptr;
    float const* m_materialParams = nullptr;  // vec4, u_materialParams
    float const* m_normalMatrix = nullptr;  // mat3 (9 floats), u_normalMatrix
    rhi::TextureHandle m_texture;            // s_texture
    int m_textureUnit = -1;                  // sampler unit for s_texture
    std::array<int, 12> m_surfaceFlags = {};  // u_surfaceFlags[SurfaceBitIndex::Count]

    // --- Animation displacement state ---
    rhi::TextureHandle m_animLutTexture;     // u_animLUT
    float const* m_animLutParams = nullptr;  // vec3 (width, height, numRgbaPerVert)
    float const* m_animDispParams = nullptr; // vec3 (frameIndex0, frameIndex1, fraction)
    float const* m_qAnimDispOrigin = nullptr; // vec3
    float const* m_qAnimDispScale = nullptr;  // vec3
};

END_DQ_RENDER_NAMESPACE
