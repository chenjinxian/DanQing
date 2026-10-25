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
        m_materialColor.fill(0.0f);
        m_materialParams.fill(0.0f);
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
    // 值语义承载（2026-10-20 Release 31 例像素测试 saga 根因修复）：此前存
    // `mat.getRgba()` 裸指针，而 mat 在绘制循环里绑定 `defaultMaterial()`
    // **按值临时**（const& 延寿仅至填充块尾）——shader->draw(drawParams) 在块外
    // 解引用死临时的栈内存：Debug 栈残留恰为哨兵值=侥幸绿，Release /O2 栈复用
    // =垃圾色（全部 Surface 瓦盒染错色）。拷贝进 DrawParams 自持数组。
    void setMaterialColor(float const* rgba) noexcept
    {
        if (rgba) std::memcpy(m_materialColor.data(), rgba, m_materialColor.size() * sizeof(float));
        else m_materialColor.fill(0.0f);
    }
    float const* getMaterialColor() const noexcept { return m_materialColor.data(); }

    // --- Per-draw material params (u_materialParams, vec4) ---
    // Ported from: itwinjs-core Surface.ts addMaterial() (line 214-220)
    //（值语义同上——同型悬垂隐患一并消除。）
    void setMaterialParams(float const* mp) noexcept
    {
        if (mp) std::memcpy(m_materialParams.data(), mp, m_materialParams.size() * sizeof(float));
        else m_materialParams.fill(0.0f);
    }
    float const* getMaterialParams() const noexcept { return m_materialParams.data(); }

    // --- Per-draw surface flags (u_surfaceFlags[12], SurfaceBitIndex) ---
    // Ported from: itwinjs-core Surface.ts addSurfaceFlags() (line 519-527)
    void setSurfaceFlags(int const* flags) noexcept
    {
        if (flags) std::memcpy(m_surfaceFlags.data(), flags, m_surfaceFlags.size() * sizeof(int));
        else m_surfaceFlags.fill(0);
    }
    int const* getSurfaceFlags() const noexcept { return m_surfaceFlags.data(); }

    // --- Per-draw material atlas color-table base row (u_numColors) ---
    // Ported from: itwinjs-core Surface.ts addMaterial u_numColors
    //   (uniform.setUniform1f(info.vertexTableOffset)——readMaterialAtlas 在
    //   顶点 LUT 内的材质色条目基行；非 atlas 几何 = 0 → atlas 分支不产出)。
    void setNumColors(float n) noexcept { m_numColors = n; }
    float getNumColors() const noexcept { return m_numColors; }

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
        // M-S S-d：getPass(target) 签名随参考归位（CachedGeometry.ts:84）——
        // 本面零调用方（DrawCommand.cpp:81 走 executor 面）；target 缺席时
        // 无法判 thematic/透明路由 → None（惰性面原样）。
        return (m_geometry && m_target) ? m_geometry->getPass(*m_target) : Pass::None;
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
    std::array<float, 4> m_materialColor = {{-1.0f, -1.0f, -1.0f, -1.0f}};  // rgba，-1=无覆盖哨兵
    std::array<float, 4> m_materialParams = {};  // vec4, u_materialParams（fragUniforms）
    float const* m_normalMatrix = nullptr;  // mat3 (9 floats), u_normalMatrix
    rhi::TextureHandle m_texture;            // s_texture
    int m_textureUnit = -1;                  // sampler unit for s_texture
    std::array<int, 12> m_surfaceFlags = {};  // u_surfaceFlags[SurfaceBitIndex::Count]
    float m_numColors = 0.0f;                 // u_numColors（材质色图集基行，非 atlas=0）

    // --- Animation displacement state ---
    rhi::TextureHandle m_animLutTexture;     // u_animLUT
    float const* m_animLutParams = nullptr;  // vec3 (width, height, numRgbaPerVert)
    float const* m_animDispParams = nullptr; // vec3 (frameIndex0, frameIndex1, fraction)
    float const* m_qAnimDispOrigin = nullptr; // vec3
    float const* m_qAnimDispScale = nullptr;  // vec3
};

END_DQ_RENDER_NAMESPACE
