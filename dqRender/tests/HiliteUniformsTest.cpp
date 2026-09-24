// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — HiliteUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              (no direct reference test exists; tests are authored from HiliteUniforms.ts behavior)
#include "render/HiliteUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Default construction exposes the default hilite color.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              TEST(HiliteUniformsTest, DefaultHiliteColor)
TEST(HiliteUniformsTest, DefaultHiliteColor)
{
    HiliteUniforms u;
    FloatRgb const& c = u.getHiliteColor();
    // Default HiliteSettings color = ColorDef::From(0x23, 0xbb, 0xfc).
    EXPECT_FLOAT_EQ(c.red(), 0x23 / 255.0f);
    EXPECT_FLOAT_EQ(c.green(), 0xbb / 255.0f);
    EXPECT_FLOAT_EQ(c.blue(), 0xfc / 255.0f);
}

// update() packs hilite/emphasis colors and ratios into the mat3 uniforms.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              TEST(HiliteUniformsTest, UpdatePacksCompositeAndFeature)
TEST(HiliteUniformsTest, UpdatePacksCompositeAndFeature)
{
    HiliteUniforms u;
    HiliteSettings hilite(ColorDef::from(255, 0, 0),   // red,    visible=0.25, hidden=0.10
                          0.25, 0.10, HiliteSilhouette::Thin);
    HiliteSettings emphasis(ColorDef::from(0, 255, 0), // green
                            0.50, 0.20, HiliteSilhouette::Thick);

    u.update(hilite, emphasis);

    auto const& c = u.getCompositeSettings().data;
    auto const& f = u.getFeatureSettings().data;

    // Row 0: hilite color (red).
    EXPECT_FLOAT_EQ(c[0], 1.0f);
    EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
    // Row 1: emphasis color (green).
    EXPECT_FLOAT_EQ(c[3], 0.0f);
    EXPECT_FLOAT_EQ(c[4], 1.0f);
    EXPECT_FLOAT_EQ(c[5], 0.0f);
    // Row 2 composite: hilite.hiddenRatio, emphasis.hiddenRatio.
    EXPECT_FLOAT_EQ(c[6], 0.10f);
    EXPECT_FLOAT_EQ(c[7], 0.20f);

    // Feature row 0/1 mirror the colors.
    EXPECT_FLOAT_EQ(f[0], 1.0f);
    EXPECT_FLOAT_EQ(f[4], 1.0f);
    // Row 2 feature: hilite.visibleRatio, emphasis.visibleRatio.
    EXPECT_FLOAT_EQ(f[6], 0.25f);
    EXPECT_FLOAT_EQ(f[7], 0.50f);
}

// compositeWidths holds the silhouette enum values as floats.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              TEST(HiliteUniformsTest, CompositeWidths)
TEST(HiliteUniformsTest, CompositeWidths)
{
    HiliteUniforms u;
    HiliteSettings hilite(ColorDef::from(255, 0, 0), 0.25, 0.0, HiliteSilhouette::Thin);    // = 1
    HiliteSettings emphasis(ColorDef::from(0, 255, 0), 0.25, 0.0, HiliteSilhouette::Thick);  // = 2

    u.update(hilite, emphasis);

    UniformHandle w;
    u.bindCompositeWidths(w);
    EXPECT_FLOAT_EQ(w.getData()[0], 1.0f);  // Thin
    EXPECT_FLOAT_EQ(w.getData()[1], 2.0f);  // Thick
}

// bindCompositeSettings uploads a mat3 (9 floats).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              TEST(HiliteUniformsTest, BindCompositeSettings)
TEST(HiliteUniformsTest, BindCompositeSettings)
{
    HiliteUniforms u;
    HiliteSettings hilite(ColorDef::from(255, 0, 0), 0.25, 0.0, HiliteSilhouette::Thin);
    HiliteSettings emphasis(ColorDef::from(0, 0, 255), 0.25, 0.0, HiliteSilhouette::Thin);
    u.update(hilite, emphasis);

    UniformHandle h;
    u.bindCompositeSettings(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::Mat3);
}

// Hilite color getter reflects the last-set (hilite) color.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              TEST(HiliteUniformsTest, HiliteColorIsLastSet)
TEST(HiliteUniformsTest, HiliteColorIsLastSet)
{
    HiliteUniforms u;
    HiliteSettings hilite(ColorDef::from(10, 20, 30), 0.25, 0.0, HiliteSilhouette::Thin);
    HiliteSettings emphasis(ColorDef::from(200, 200, 200), 0.25, 0.0, HiliteSilhouette::Thin);
    u.update(hilite, emphasis);

    // Getter must expose hilite color (set last in reference), not emphasis.
    EXPECT_FLOAT_EQ(u.getHiliteColor().red(), 10.0f / 255.0f);
    EXPECT_FLOAT_EQ(u.getHiliteColor().green(), 20.0f / 255.0f);
    EXPECT_FLOAT_EQ(u.getHiliteColor().blue(), 30.0f / 255.0f);
}

// Re-updating with equal settings is a no-op (state unchanged).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/HiliteUniforms.test.ts
//              TEST(HiliteUniformsTest, UpdateEqualSettingsNoChange)
TEST(HiliteUniformsTest, UpdateEqualSettingsNoChange)
{
    HiliteUniforms u;
    HiliteSettings hilite(ColorDef::from(255, 0, 0), 0.25, 0.0, HiliteSilhouette::Thin);
    HiliteSettings emphasis(ColorDef::from(0, 255, 0), 0.25, 0.0, HiliteSilhouette::Thin);

    u.update(hilite, emphasis);
    float c0 = u.getCompositeSettings().data[0];

    u.update(hilite, emphasis);
    EXPECT_FLOAT_EQ(u.getCompositeSettings().data[0], c0);
}
