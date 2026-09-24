// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ThematicUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              (no direct reference test exists; tests are authored based on ThematicUniforms.ts behavior)
#include "render/ThematicUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Test ThematicUniforms default initialization.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, DefaultInitialization)
TEST(ThematicUniformsTest, DefaultInitialization)
{
    ThematicUniforms uniforms;

    EXPECT_EQ(uniforms.getThematicDisplay(), nullptr);
    EXPECT_FALSE(uniforms.wantIsoLines());
    EXPECT_FALSE(uniforms.wantSlopeMode());
    EXPECT_FALSE(uniforms.wantHillShadeMode());
    EXPECT_TRUE(uniforms.isDisposed());
}

// Test ThematicUniforms update with settings.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, UpdateSettings)
TEST(ThematicUniformsTest, UpdateSettings)
{
    ThematicUniforms uniforms;

    ThematicDisplay display;
    display.displayMode = ThematicDisplayMode::Height;
    display.rangeMin = 0.0;
    display.rangeMax = 100.0;
    display.gradientSettings.colorMix = 0.5;
    display.gradientSettings.mode = ThematicGradientMode::Smooth;
    display.gradientSettings.stepCount = 10;
    display.gradientSettings.transparencyMode = ThematicGradientTransparencyMode::SurfaceOnly;

    uniforms.update(display);

    auto const* current = uniforms.getThematicDisplay();
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->displayMode, ThematicDisplayMode::Height);
    EXPECT_DOUBLE_EQ(current->rangeMin, 0.0);
    EXPECT_DOUBLE_EQ(current->rangeMax, 100.0);
}

// Test ThematicUniforms update with same settings (no change).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, UpdateSameSettings)
TEST(ThematicUniformsTest, UpdateSameSettings)
{
    ThematicUniforms uniforms;

    ThematicDisplay display;
    display.displayMode = ThematicDisplayMode::Height;

    uniforms.update(display);
    uniforms.update(display);  // Should not change

    auto const* current = uniforms.getThematicDisplay();
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->displayMode, ThematicDisplayMode::Height);
}

// Test ThematicUniforms wantIsoLines.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, WantIsoLines)
TEST(ThematicUniformsTest, WantIsoLines)
{
    ThematicUniforms uniforms;

    ThematicDisplay display;
    display.displayMode = ThematicDisplayMode::Height;
    display.gradientSettings.mode = ThematicGradientMode::IsoLines;

    uniforms.update(display);

    EXPECT_TRUE(uniforms.wantIsoLines());
    EXPECT_FALSE(uniforms.wantSlopeMode());
    EXPECT_FALSE(uniforms.wantHillShadeMode());
}

// Test ThematicUniforms wantSlopeMode.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, WantSlopeMode)
TEST(ThematicUniformsTest, WantSlopeMode)
{
    ThematicUniforms uniforms;

    ThematicDisplay display;
    display.displayMode = ThematicDisplayMode::Slope;

    uniforms.update(display);

    EXPECT_FALSE(uniforms.wantIsoLines());
    EXPECT_TRUE(uniforms.wantSlopeMode());
    EXPECT_FALSE(uniforms.wantHillShadeMode());
}

// Test ThematicUniforms wantHillShadeMode.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, WantHillShadeMode)
TEST(ThematicUniformsTest, WantHillShadeMode)
{
    ThematicUniforms uniforms;

    ThematicDisplay display;
    display.displayMode = ThematicDisplayMode::HillShade;

    uniforms.update(display);

    EXPECT_FALSE(uniforms.wantIsoLines());
    EXPECT_FALSE(uniforms.wantSlopeMode());
    EXPECT_TRUE(uniforms.wantHillShadeMode());
}

// Test ThematicUniforms dispose.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, Dispose)
TEST(ThematicUniformsTest, Dispose)
{
    ThematicUniforms uniforms;

    EXPECT_TRUE(uniforms.isDisposed());
    uniforms.dispose();  // Should not crash
    EXPECT_TRUE(uniforms.isDisposed());
}

// Test ThematicDisplay defaults.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, ThematicDisplayDefaults)
TEST(ThematicUniformsTest, ThematicDisplayDefaults)
{
    auto const& defaults = ThematicDisplay::defaults();

    EXPECT_EQ(defaults.displayMode, ThematicDisplayMode::Height);
    EXPECT_DOUBLE_EQ(defaults.rangeMin, 0.0);
    EXPECT_DOUBLE_EQ(defaults.rangeMax, 1.0);
}

// Test ThematicDisplay from JSON.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, ThematicDisplayFromJSON)
TEST(ThematicUniformsTest, ThematicDisplayFromJSON)
{
    ThematicDisplayProps props;
    props.displayMode = ThematicDisplayMode::Slope;
    props.rangeMin = -10.0;
    props.rangeMax = 10.0;

    auto display = ThematicDisplay::fromJSON(&props);

    EXPECT_EQ(display.displayMode, ThematicDisplayMode::Slope);
    EXPECT_DOUBLE_EQ(display.rangeMin, -10.0);
    EXPECT_DOUBLE_EQ(display.rangeMax, 10.0);
}

// Test kDefaultGradientDimension constant.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ThematicUniforms.test.ts
//              TEST(ThematicUniformsTest, DefaultGradientDimension)
TEST(ThematicUniformsTest, DefaultGradientDimension)
{
    EXPECT_EQ(kDefaultGradientDimension, 8192);
}
