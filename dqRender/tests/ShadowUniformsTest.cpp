// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ShadowUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              (no direct reference test exists; tests are authored based on ShadowUniforms.ts behavior)
#include "render/ShadowUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Test ShadowUniforms default initialization.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, DefaultInitialization)
TEST(ShadowUniformsTest, DefaultInitialization)
{
    ShadowUniforms uniforms;

    EXPECT_FALSE(uniforms.isEnabled());
    EXPECT_EQ(uniforms.getColor().r, 0);
    EXPECT_EQ(uniforms.getColor().g, 0);
    EXPECT_EQ(uniforms.getColor().b, 0);
    EXPECT_FLOAT_EQ(uniforms.getBias(), 0.0f);
}

// Test ShadowUniforms update with settings.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, UpdateSettings)
TEST(ShadowUniformsTest, UpdateSettings)
{
    ShadowUniforms uniforms;

    SolarShadowMap shadowMap;
    shadowMap.isEnabled = true;
    shadowMap.color = RgbColor(128, 64, 32);
    shadowMap.bias = 0.001f;

    uniforms.update(shadowMap);

    EXPECT_TRUE(uniforms.isEnabled());
    EXPECT_EQ(uniforms.getColor().r, 128);
    EXPECT_EQ(uniforms.getColor().g, 64);
    EXPECT_EQ(uniforms.getColor().b, 32);
    EXPECT_FLOAT_EQ(uniforms.getBias(), 0.001f);
}

// Test ShadowUniforms update with same settings (no change).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, UpdateSameSettings)
TEST(ShadowUniformsTest, UpdateSameSettings)
{
    ShadowUniforms uniforms;

    SolarShadowMap shadowMap;
    shadowMap.isEnabled = true;
    shadowMap.color = RgbColor(255, 255, 255);
    shadowMap.bias = 0.01f;

    uniforms.update(shadowMap);
    uniforms.update(shadowMap);  // Should not change

    EXPECT_TRUE(uniforms.isEnabled());
    EXPECT_EQ(uniforms.getColor().r, 255);
    EXPECT_EQ(uniforms.getColor().g, 255);
    EXPECT_EQ(uniforms.getColor().b, 255);
    EXPECT_FLOAT_EQ(uniforms.getBias(), 0.01f);
}

// Test ShadowUniforms update with different settings.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, UpdateDifferentSettings)
TEST(ShadowUniformsTest, UpdateDifferentSettings)
{
    ShadowUniforms uniforms;

    SolarShadowMap shadowMap1;
    shadowMap1.isEnabled = true;
    shadowMap1.color = RgbColor(255, 0, 0);
    shadowMap1.bias = 0.001f;

    SolarShadowMap shadowMap2;
    shadowMap2.isEnabled = false;
    shadowMap2.color = RgbColor(0, 255, 0);
    shadowMap2.bias = 0.002f;

    uniforms.update(shadowMap1);
    uniforms.update(shadowMap2);

    EXPECT_FALSE(uniforms.isEnabled());
    EXPECT_EQ(uniforms.getColor().r, 0);
    EXPECT_EQ(uniforms.getColor().g, 255);
    EXPECT_EQ(uniforms.getColor().b, 0);
    EXPECT_FLOAT_EQ(uniforms.getBias(), 0.002f);
}

// Test ShadowUniforms projection matrix.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, ProjectionMatrix)
TEST(ShadowUniformsTest, ProjectionMatrix)
{
    ShadowUniforms uniforms;

    auto const& matrix = uniforms.getProjectionMatrix();

    // Default should be identity matrix
    EXPECT_FLOAT_EQ(matrix.data[0], 1.0f);
    EXPECT_FLOAT_EQ(matrix.data[5], 1.0f);
    EXPECT_FLOAT_EQ(matrix.data[10], 1.0f);
    EXPECT_FLOAT_EQ(matrix.data[15], 1.0f);
}

// Test RgbColor constructor.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, RgbColorConstructor)
TEST(ShadowUniformsTest, RgbColorConstructor)
{
    RgbColor color(100, 150, 200);

    EXPECT_EQ(color.r, 100);
    EXPECT_EQ(color.g, 150);
    EXPECT_EQ(color.b, 200);
}

// Test RgbColor clamping.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ShadowUniforms.test.ts
//              TEST(ShadowUniformsTest, RgbColorClamping)
TEST(ShadowUniformsTest, RgbColorClamping)
{
    RgbColor color(-10, 300, 128);

    EXPECT_EQ(color.r, 0);
    EXPECT_EQ(color.g, 255);
    EXPECT_EQ(color.b, 128);
}
