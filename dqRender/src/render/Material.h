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
#include <optional>

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

// ---------------------------------------------------------------------------
// RenderMaterialParams — material parameter bag fed to
// RenderMaterialInternal::fromParams (the GL Material constructor).
// Ported from: itwinjs-core core/common/src/internal/RenderMaterialParams.ts
// (class RenderMaterialParams — defaults diffuse 0.6 / specular 0.4 /
// specularExponent 13.5 / alpha undefined; specularColor defaults to white
// in the webgl Material constructor, Material.ts:84).
// Color components use the ColorDef 0-255 domain (imdl floats 0..1 are
// converted by the parser: v*255+0.5, ParseImdlDocument.ts:1105-1107).
// ---------------------------------------------------------------------------
struct RenderMaterialParams {
    bool hasDiffuseColor = false;
    float diffuseColor[3] = {0.0f, 0.0f, 0.0f};      // 0-255; overrides surface color when set
    bool hasSpecularColor = false;
    float specularColor[3] = {255.0f, 255.0f, 255.0f};  // 0-255; white when unset (Material.ts:84)
    std::optional<float> alpha;                       // unset = no transparency override (rgba[3] = -1)
    float diffuse = 0.6f;
    float specular = 0.4f;
    float specularExponent = 13.5f;
    std::optional<float> textureWeight;               // textureMapping weight ?? 1.0 (Material.ts:83)
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

    // Vertex-side diffuse-color override present (reference rgba[0] >= 0 —
    // Material.ts:50 overridesRgb). When false the surface's own color shows.
    bool overridesRgb() const noexcept { return m_rgba[0] >= 0.0f; }
    // Vertex-side alpha override present (reference rgba[3] >= 0 — Material.ts:51).
    bool overridesAlpha() const noexcept { return m_rgba[3] >= 0.0f; }

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

    // Build the GL-side material from RenderMaterialParams.
    // Ported from: itwinjs-core webgl/Material.ts constructor (:61-90):
    //   rgba[0..2] = diffuseColor/255 or -1 (no override);
    //   rgba[3]    = alpha or -1 (no override);
    //   fragUniforms[0] = pack(scale(diffuse), scale(specular)) — weights scaled to 0-255;
    //   fragUniforms[1] = pack(scale(textureWeight), specularColor.r);
    //   fragUniforms[2] = pack(specularColor.g, specularColor.b);
    //   fragUniforms[3] = specularExponent.
    static RenderMaterialInternal fromParams(RenderMaterialParams const& params);

    // Default material (matches itwinjs-core Material.default =
    // new Material(RenderMaterialParams.defaults): diffuse 0.6 / specular 0.4 /
    // specular color white / exponent 13.5 / no color or alpha override).
    // NOTE (M-M(1) 归位): the previous version packed (1.0, 0.0, black) —
    // diffuse weight 1.0 and ZERO specular — a deviation from
    // RenderMaterialParams.defaults that suppressed the default specular
    // highlights (two directional lights, glsl/Surface.ts:134-137 defaults
    // (26265, 65535, 65535, 13.5)).
    static RenderMaterialInternal defaultMaterial();

private:
    bool m_hasTexture = false;
    float m_alpha = 1.0f;
    MonochromeMode m_monochromeMode = MonochromeMode::None;
    bool m_isAtlas = false;
    bool m_hasTransform = false;
    bool m_hasVertexColors = false;

    // Fragment material parameters (packed vec4).
    // Default: {26265, 65535, 65535, 13.5} = itwinjs-core Material.default.fragUniforms
    float m_fragUniforms[4] = {26265.0f, 65535.0f, 65535.0f, 13.5f};

    // Vertex material color (RGBA).
    // Default: {-1, -1, -1, -1} = no override
    float m_rgba[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
};

END_DQ_RENDER_NAMESPACE
