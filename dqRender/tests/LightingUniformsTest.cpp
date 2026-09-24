// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — LightingUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              (no direct reference test exists; tests are authored from LightingUniforms.ts behavior)
#include "render/LightingUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Default-constructed uniforms are uninitialized (all-zero data) until update().
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, DefaultUninitialized)
TEST(LightingUniformsTest, DefaultUninitialized)
{
    LightingUniforms u;
    for (int i = 0; i < 16; ++i)
        EXPECT_FLOAT_EQ(u.getData()[i], 0.0f);
}

// update() packs the 16-float lighting array per the reference layout.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, UpdatePacksLayout)
TEST(LightingUniformsTest, UpdatePacksLayout)
{
    LightingUniforms u;
    LightSettings s;
    s.solar.intensity = 2.0;
    s.ambient.color = RgbColor(255, 0, 0);       // red
    s.ambient.intensity = 0.5;
    s.hemisphere.lowerColor = RgbColor(0, 255, 0);  // green
    s.hemisphere.upperColor = RgbColor(0, 0, 255);  // blue
    s.hemisphere.intensity = 0.75;
    s.portraitIntensity = 0.4;
    s.specularIntensity = 1.5;
    s.numCels = 3;
    s.fresnel.intensity = 0.6;
    s.fresnel.invert = false;

    u.update(s);

    auto const& d = u.getData();
    EXPECT_FLOAT_EQ(d[0], 2.0f);                    // solar intensity
    EXPECT_FLOAT_EQ(d[1], 1.0f); EXPECT_FLOAT_EQ(d[2], 0.0f); EXPECT_FLOAT_EQ(d[3], 0.0f);  // ambient red
    EXPECT_FLOAT_EQ(d[4], 0.5f);                    // ambient intensity
    EXPECT_FLOAT_EQ(d[5], 0.0f); EXPECT_FLOAT_EQ(d[6], 1.0f); EXPECT_FLOAT_EQ(d[7], 0.0f);  // lower green
    EXPECT_FLOAT_EQ(d[8], 0.0f); EXPECT_FLOAT_EQ(d[9], 0.0f); EXPECT_FLOAT_EQ(d[10], 1.0f); // upper blue
    EXPECT_FLOAT_EQ(d[11], 0.75f);                  // hemisphere intensity
    EXPECT_FLOAT_EQ(d[12], 0.4f);                   // portrait
    EXPECT_FLOAT_EQ(d[13], 1.5f);                   // specular
    EXPECT_FLOAT_EQ(d[14], 3.0f);                   // num cels
    EXPECT_FLOAT_EQ(d[15], 0.6f);                   // fresnel (positive)
}

// Fresnel invert negates the stored intensity.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, FresnelInvertedNegates)
TEST(LightingUniformsTest, FresnelInvertedNegates)
{
    LightingUniforms u;
    LightSettings s;
    s.fresnel.intensity = 0.6;
    s.fresnel.invert = true;
    u.update(s);
    EXPECT_FLOAT_EQ(u.getData()[15], -0.6f);
}

// bind() uploads 16 floats.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, BindUploadsArray)
TEST(LightingUniformsTest, BindUploadsArray)
{
    LightingUniforms u;
    LightSettings s;
    s.solar.intensity = 1.0;
    u.update(s);

    UniformHandle h;
    u.bind(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::FloatArray);
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);
}

// Updating with equal settings is a no-op.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, UpdateEqualSettingsNoChange)
TEST(LightingUniformsTest, UpdateEqualSettingsNoChange)
{
    LightingUniforms u;
    LightSettings s;
    s.solar.intensity = 5.0;
    u.update(s);
    u.update(s);  // equal -> no recompute
    EXPECT_FLOAT_EQ(u.getData()[0], 5.0f);
}
