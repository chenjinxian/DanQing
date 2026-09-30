// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GL Material packing tests (M-M(1))
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Material.ts
//              (constructor :61-90 + Material.default :43) and
//              core/common/src/internal/RenderMaterialParams.ts (defaults :33-44).
// Authored: no reference test exists in itwinjs-core for the webgl Material
// class (browser-only); expected values are the reference constants — the
// default pack (26265, 65535, 65535, 13.5) is asserted by the shader-side
// twin in glsl/Surface.ts:134-137 (computeMaterialParams defaults), so the
// two paths cannot drift apart silently.
#include "render/Material.h"

#include <gtest/gtest.h>

using namespace dqRender;

// Material.default == new Material(RenderMaterialParams.defaults):
// diffuse 0.6 / specular 0.4 / specular white / exponent 13.5 / no overrides.
// 26265 = floor(0.6*255+0.5) + floor(0.4*255+0.5)*256 = 153 + 102*256.
TEST(Material, DefaultMaterialMatchesReferenceDefaults)
{
    auto const mat = RenderMaterialInternal::defaultMaterial();

    float const* frag = mat.getFragUniforms();
    EXPECT_FLOAT_EQ(frag[0], 26265.0f);   // diffuse 153 | specular 102
    EXPECT_FLOAT_EQ(frag[1], 65535.0f);   // textureWeight 255 | specularR 255
    EXPECT_FLOAT_EQ(frag[2], 65535.0f);   // specularG 255 | specularB 255
    EXPECT_FLOAT_EQ(frag[3], 13.5f);      // specularExponent

    float const* rgba = mat.getRgba();
    EXPECT_FLOAT_EQ(rgba[0], -1.0f);      // no diffuse color override
    EXPECT_FLOAT_EQ(rgba[1], -1.0f);
    EXPECT_FLOAT_EQ(rgba[2], -1.0f);
    EXPECT_FLOAT_EQ(rgba[3], -1.0f);      // no alpha override
    EXPECT_FALSE(mat.overridesRgb());
    EXPECT_FALSE(mat.overridesAlpha());
}

// Field values mirror the real housemodel-v1 tile `-b-2-0-0-0-1`'s
// renderMaterials["0x40"] probe (2026-09-30): diffuse 0 / specular 0 /
// specularExponent 0.3861 / diffuseColor white / specularColor white /
// transparency 0 -> alpha 1. A zero-weight material suppresses BOTH the
// diffuse and the specular contribution — the DTA-visible difference that
// motivated the imdl material consumption (M-M(1)).
TEST(Material, FromParamsPacksZeroWeightMaterial)
{
    RenderMaterialParams p;
    p.diffuse = 0.0f;
    p.specular = 0.0f;
    p.specularExponent = 0.3861f;
    p.hasDiffuseColor = true;
    p.diffuseColor[0] = p.diffuseColor[1] = p.diffuseColor[2] = 255.0f;
    p.hasSpecularColor = true;
    p.specularColor[0] = p.specularColor[1] = p.specularColor[2] = 255.0f;
    p.alpha = 1.0f;

    auto const mat = RenderMaterialInternal::fromParams(p);

    float const* frag = mat.getFragUniforms();
    EXPECT_FLOAT_EQ(frag[0], 0.0f);       // scale(0) + scale(0)*256
    EXPECT_FLOAT_EQ(frag[1], 65535.0f);
    EXPECT_FLOAT_EQ(frag[2], 65535.0f);
    EXPECT_FLOAT_EQ(frag[3], 0.3861f);

    float const* rgba = mat.getRgba();
    EXPECT_FLOAT_EQ(rgba[0], 1.0f);       // white override
    EXPECT_FLOAT_EQ(rgba[1], 1.0f);
    EXPECT_FLOAT_EQ(rgba[2], 1.0f);
    EXPECT_FLOAT_EQ(rgba[3], 1.0f);       // transparency 0 -> alpha 1
    EXPECT_TRUE(mat.overridesRgb());
    EXPECT_TRUE(mat.overridesAlpha());
}

// Partial params keep the reference defaults for unset fields
// (RenderMaterialParams defaults diffuse 0.6 / specular 0.4 / white / 13.5;
// unset alpha -> -1 sentinel).
TEST(Material, FromParamsKeepsDefaultsForUnsetFields)
{
    RenderMaterialParams p;
    p.diffuse = 1.0f;  // only diffuse overridden

    auto const mat = RenderMaterialInternal::fromParams(p);

    float const* frag = mat.getFragUniforms();
    EXPECT_FLOAT_EQ(frag[0], 255.0f + 102.0f * 256.0f);  // diffuse 255 | specular 102 (default 0.4)
    EXPECT_FLOAT_EQ(frag[1], 65535.0f);                  // textureWeight 1 | specularR white
    EXPECT_FLOAT_EQ(frag[3], 13.5f);
    EXPECT_FLOAT_EQ(mat.getRgba()[3], -1.0f);            // alpha not overridden
}
