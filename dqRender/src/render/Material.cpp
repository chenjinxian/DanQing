// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Material implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Material.ts
#include "Material.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// setInteger — pack two 0-255 values into a single float (lo + hi * 256),
// with the reference's clamp-to-[0,255] floor semantics.
// Ported from: itwinjs-core Material.ts setInteger() (:92-99) + the ctor's
//              scale() closure (:80: Math.floor(value * 255 + 0.5))
// ---------------------------------------------------------------------------
static void setInteger(float loByte, float hiByte, int index, float fragUniforms[4])
{
    auto const clamp = [](float x) { return std::floor(std::min(255.0f, std::max(x, 0.0f))); };
    fragUniforms[index] = clamp(loByte) + clamp(hiByte) * 256.0f;
}

static float scale255(float value)
{
    return std::floor(value * 255.0f + 0.5f);
}

// ---------------------------------------------------------------------------
// fromParams — pack RenderMaterialParams into the GL material.
// Ported from: itwinjs-core Material.ts constructor (:61-90)
// ---------------------------------------------------------------------------
RenderMaterialInternal RenderMaterialInternal::fromParams(RenderMaterialParams const& params)
{
    RenderMaterialInternal mat;

    // rgba: diffuse color override (ColorDef -> FloatRgb 0-1) or -1 sentinel;
    // alpha override or -1 sentinel.
    // Ported from: itwinjs-core Material.ts :68-78
    if (params.hasDiffuseColor) {
        mat.m_rgba[0] = params.diffuseColor[0] / 255.0f;
        mat.m_rgba[1] = params.diffuseColor[1] / 255.0f;
        mat.m_rgba[2] = params.diffuseColor[2] / 255.0f;
    } else {
        mat.m_rgba[0] = mat.m_rgba[1] = mat.m_rgba[2] = -1.0f;
    }
    mat.m_rgba[3] = params.alpha ? *params.alpha : -1.0f;

    // fragUniforms: (diffuse|specular weights), (textureWeight|specularR),
    // (specularG|specularB), specularExponent.
    // Ported from: itwinjs-core Material.ts :80-89
    setInteger(scale255(params.diffuse), scale255(params.specular), 0, mat.m_fragUniforms);

    float const textureWeight = params.textureWeight ? *params.textureWeight : 1.0f;
    setInteger(scale255(textureWeight), params.specularColor[0], 1, mat.m_fragUniforms);
    setInteger(params.specularColor[1], params.specularColor[2], 2, mat.m_fragUniforms);
    mat.m_fragUniforms[3] = params.specularExponent;

    // hasTranslucency bookkeeping (reference Material.ts:52 consumes
    // overridesAlpha && rgba[3] < 1 at query time — DanQing mirrors the
    // overridesAlpha() accessor; no extra state needed).
    mat.m_alpha = mat.m_rgba[3] >= 0.0f ? mat.m_rgba[3] : 1.0f;

    return mat;
}

// ---------------------------------------------------------------------------
// defaultMaterial — Material.default = new Material(RenderMaterialParams.defaults).
// diffuse 0.6 / specular 0.4 / specular white / exponent 13.5 / no overrides
// -> fragUniforms = (26265, 65535, 65535, 13.5), rgba = (-1,-1,-1,-1).
// Ported from: itwinjs-core Material.ts :43 (`public static readonly default`)
//              + core/common/src/internal/RenderMaterialParams.ts :33-44 defaults
// ---------------------------------------------------------------------------
RenderMaterialInternal RenderMaterialInternal::defaultMaterial()
{
    return fromParams(RenderMaterialParams{});
}

END_DQ_RENDER_NAMESPACE
