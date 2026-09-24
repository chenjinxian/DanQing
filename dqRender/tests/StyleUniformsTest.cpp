// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — StyleUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              (no direct reference test exists; tests are authored from StyleUniforms.ts behavior)
#include "render/StyleUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Default state: white background, intensity = 0.3+0.59+0.11 = 1.0, WoW reversal on.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, DefaultState)
TEST(StyleUniformsTest, DefaultState)
{
    StyleUniforms s;
    EXPECT_FLOAT_EQ(s.getBackgroundIntensity(), 1.0f);  // white luminance
    EXPECT_TRUE(s.getWantWoWReversal());                // white bg -> reversal active
    EXPECT_FLOAT_EQ(s.getBackgroundAlpha(), 1.0f);      // opaque
}

// update(plan) sets the background color and recomputes intensity.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, UpdateBackgroundColor)
TEST(StyleUniformsTest, UpdateBackgroundColor)
{
    StyleUniforms s;
    RenderPlan plan;
    plan.backgroundColor = ColorDef::from(255, 0, 0).getTbgr();  // red -> luminance 0.3
    plan.whiteOnWhiteReversal = false;
    s.update(plan);

    EXPECT_FLOAT_EQ(s.getBackgroundIntensity(), 0.3f);
    EXPECT_FALSE(s.getWantWoWReversal());  // non-white bg + reversal disabled

    UniformHandle rgb;
    s.bindBackgroundRgb(rgb);
    EXPECT_FLOAT_EQ(rgb.getData()[0], 1.0f);
    EXPECT_FLOAT_EQ(rgb.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(rgb.getData()[2], 0.0f);
}

// WoW reversal flag forces reversal on even for non-white background.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, WoWReversalFlag)
TEST(StyleUniformsTest, WoWReversalFlag)
{
    StyleUniforms s;
    RenderPlan plan;
    plan.backgroundColor = ColorDef::from(255, 0, 0).getTbgr();  // non-white
    plan.whiteOnWhiteReversal = true;  // force reversal
    s.update(plan);
    EXPECT_TRUE(s.getWantWoWReversal());
}

// changeBackgroundColor updates intensity without touching monochrome.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, ChangeBackgroundColor)
TEST(StyleUniformsTest, ChangeBackgroundColor)
{
    StyleUniforms s;
    s.changeBackgroundColor(ColorDef::from(0, 0, 255));  // blue -> luminance 0.11
    EXPECT_NEAR(s.getBackgroundIntensity(), 0.11f, 1e-5);
}

// Monochrome color is bound separately from background.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, MonochromeColor)
TEST(StyleUniformsTest, MonochromeColor)
{
    StyleUniforms s;
    RenderPlan plan;
    plan.monochromeColor = ColorDef::from(128, 128, 128).getTbgr();
    plan.whiteOnWhiteReversal = false;
    s.update(plan);

    UniformHandle mono;
    s.bindMonochromeRgb(mono);
    EXPECT_NEAR(mono.getData()[0], 128.0f / 255.0f, 1e-5);
    EXPECT_NEAR(mono.getData()[1], 128.0f / 255.0f, 1e-5);
    EXPECT_NEAR(mono.getData()[2], 128.0f / 255.0f, 1e-5);
}

// Updating with the same plan is a no-op.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, UpdateEqualPlanNoChange)
TEST(StyleUniformsTest, UpdateEqualPlanNoChange)
{
    StyleUniforms s;
    RenderPlan plan;
    plan.backgroundColor = ColorDef::from(10, 20, 30).getTbgr();
    plan.whiteOnWhiteReversal = false;
    s.update(plan);
    float intensity = s.getBackgroundIntensity();
    s.update(plan);
    EXPECT_FLOAT_EQ(s.getBackgroundIntensity(), intensity);
}

// Background tbgr reflects the bg color.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, BackgroundTbgr)
TEST(StyleUniformsTest, BackgroundTbgr)
{
    StyleUniforms s;
    s.changeBackgroundColor(ColorDef::from(255, 0, 0));
    // tbgr encodes 0xTTBBGGRR; red = 0x000000ff.
    EXPECT_EQ(s.getBackgroundTbgr(), 0x000000FFu);
}

// cloneBackgroundRgba copies the RGBA.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/StyleUniforms.test.ts
//              TEST(StyleUniformsTest, CloneBackgroundRgba)
TEST(StyleUniformsTest, CloneBackgroundRgba)
{
    StyleUniforms s;
    s.changeBackgroundColor(ColorDef::from(255, 255, 255));
    FloatRgba out;
    s.cloneBackgroundRgba(out);
    EXPECT_FLOAT_EQ(out.r, 1.0f);
    EXPECT_FLOAT_EQ(out.g, 1.0f);
    EXPECT_FLOAT_EQ(out.b, 1.0f);
    EXPECT_FLOAT_EQ(out.a, 1.0f);
}
