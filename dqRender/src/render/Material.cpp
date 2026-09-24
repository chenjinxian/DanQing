// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Material implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Material.ts
#include "Material.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Pack two 0-255 values into a single float (lo + hi * 256).
// Ported from: itwinjs-core Material.ts pack2Bytes()
// ---------------------------------------------------------------------------
static float pack2Bytes(float lo, float hi)
{
    lo = std::clamp(lo, 0.0f, 255.0f);
    hi = std::clamp(hi, 0.0f, 255.0f);
    return std::floor(lo) + std::floor(hi) * 256.0f;
}

// ---------------------------------------------------------------------------
// create — pack material parameters into fragUniforms format.
// Ported from: itwinjs-core Material.ts constructor
// ---------------------------------------------------------------------------
RenderMaterialInternal RenderMaterialInternal::create(
    float diffuseWeight, float specularWeight,
    float textureWeight,
    float specularR, float specularG, float specularB,
    float specularExponent,
    float alpha)
{
    RenderMaterialInternal mat;

    // Pack weights (0-1 range) into 0-255 byte range, then into fragUniforms.
    // Ported from: itwinjs-core Material.ts constructor lines 50-53
    mat.m_fragUniforms[0] = pack2Bytes(diffuseWeight * 255.0f, specularWeight * 255.0f);
    mat.m_fragUniforms[1] = pack2Bytes(textureWeight * 255.0f, specularR * 255.0f);
    mat.m_fragUniforms[2] = pack2Bytes(specularG * 255.0f, specularB * 255.0f);
    mat.m_fragUniforms[3] = specularExponent;

    // Set RGBA (diffuse color + alpha).
    // Ported from: itwinjs-core Material.ts constructor lines 55-58
    mat.m_rgba[0] = -1.0f;  // no diffuse color override
    mat.m_rgba[1] = -1.0f;
    mat.m_rgba[2] = -1.0f;
    mat.m_rgba[3] = alpha;

    return mat;
}

// ---------------------------------------------------------------------------
// defaultMaterial — returns the default material.
// Ported from: itwinjs-core Material.ts Material.default
// diffuseWeight=1, specularWeight=0, textureWeight=1, specular=0, exponent=13.5
// ---------------------------------------------------------------------------
RenderMaterialInternal RenderMaterialInternal::defaultMaterial()
{
    return create(1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 13.5f);
}

END_DQ_RENDER_NAMESPACE
