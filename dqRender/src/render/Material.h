// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GL-level material properties
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Material.ts
//
// Internal material property bag used during rendering to configure shader
// variants and blending state.  This is the low-level GPU-side representation
// distinct from the higher-level RenderMaterial (PublicAPI) which carries
// application-facing material parameters.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Monochrome rendering mode.
// (Ported from: itwinjs-core webgl/Material.ts MonochromeMode)
enum class MonochromeMode : uint8_t {
    None = 0,
    Saturated = 1,
    Luminance = 2,
};

// Internal material properties used during rendering.
// (Ported from: itwinjs-core webgl/Material.ts)
class RenderMaterialInternal {
public:
    RenderMaterialInternal() = default;

    bool hasTexture() const noexcept { return m_hasTexture; }
    void setHasTexture(bool val) noexcept { m_hasTexture = val; }

    float getAlpha() const noexcept { return m_alpha; }
    void setAlpha(float alpha) noexcept { m_alpha = alpha; }

    MonochromeMode getMonochromeMode() const noexcept { return m_monochromeMode; }
    void setMonochromeMode(MonochromeMode mode) noexcept { m_monochromeMode = mode; }

    bool isAtlas() const noexcept { return m_isAtlas; }
    void setIsAtlas(bool val) noexcept { m_isAtlas = val; }

    bool hasTransform() const noexcept { return m_hasTransform; }
    void setHasTransform(bool val) noexcept { m_hasTransform = val; }

    bool hasVertexColors() const noexcept { return m_hasVertexColors; }
    void setHasVertexColors(bool val) noexcept { m_hasVertexColors = val; }

    bool ignoresMaterial() const noexcept { return m_ignoresMaterial; }
    void setIgnoresMaterial(bool val) noexcept { m_ignoresMaterial = val; }

    // Fragment-side material parameters (packed as vec4).
    // Ported from: itwinjs-core Material.ts fragUniforms
    // [0]: diffuseWeight(lo) + specularWeight(hi) * 256
    // [1]: textureWeight(lo) + specularR(hi) * 256
    // [2]: specularG(lo) + specularB(hi) * 256
    // [3]: specularExponent (float)
    float const* getFragUniforms() const noexcept { return m_fragUniforms; }

    // Vertex-side material color (RGBA).
    // Ported from: itwinjs-core Material.ts rgba
    // [0..2]: diffuse RGB (-1 if not overridden)
    // [3]: alpha (-1 if not overridden)
    float const* getRgba() const noexcept { return m_rgba; }

    // Set fragUniforms from packed values.
    void setFragUniforms(float const* v) { for (int i = 0; i < 4; ++i) m_fragUniforms[i] = v[i]; }

    // Set rgba from values.
    void setRgba(float const* v) { for (int i = 0; i < 4; ++i) m_rgba[i] = v[i]; }

    // Pack material parameters into fragUniforms format.
    // Ported from: itwinjs-core Material.ts constructor
    // Weights: 0-255 (packed as lo/hi bytes in 16-bit float representation)
    // specularExponent: float directly
    static RenderMaterialInternal create(
        float diffuseWeight, float specularWeight,
        float textureWeight,
        float specularR, float specularG, float specularB,
        float specularExponent,
        float alpha = -1.0f);

    // Default material (matches itwinjs-core Material.default).
    // diffuseWeight=1, specularWeight=0, textureWeight=1, specular=0, exponent=13.5
    static RenderMaterialInternal defaultMaterial();

private:
    bool m_hasTexture = false;
    float m_alpha = 1.0f;
    MonochromeMode m_monochromeMode = MonochromeMode::None;
    bool m_isAtlas = false;
    bool m_hasTransform = false;
    bool m_hasVertexColors = false;
    bool m_ignoresMaterial = false;

    // Fragment material parameters (packed vec4).
    // Default: {26265, 65535, 65535, 13.5} = itwinjs-core Material.default.fragUniforms
    float m_fragUniforms[4] = {26265.0f, 65535.0f, 65535.0f, 13.5f};

    // Vertex material color (RGBA).
    // Default: {-1, -1, -1, -1} = no override
    float m_rgba[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
};

END_DQ_RENDER_NAMESPACE
