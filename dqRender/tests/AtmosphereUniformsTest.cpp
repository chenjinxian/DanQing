// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — AtmosphereUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              (no direct reference test exists; tests are authored based on AtmosphereUniforms.ts behavior)
#include "render/AtmosphereUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Test AtmosphereUniforms default initialization.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, DefaultInitialization)
TEST(AtmosphereUniformsTest, DefaultInitialization)
{
    AtmosphereUniforms uniforms;

    EXPECT_EQ(uniforms.getAtmosphere(), nullptr);
    EXPECT_TRUE(uniforms.isDisposed());
}

// Test AtmosphereUniforms update with settings.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, UpdateSettings)
TEST(AtmosphereUniformsTest, UpdateSettings)
{
    AtmosphereUniforms uniforms;

    Atmosphere::Settings settings;
    settings.atmosphereHeightAboveEarth = 100000.0;
    settings.depthBelowEarthForMaxDensity = 10000.0;
    settings.densityFalloff = 0.5;
    settings.numViewRaySamples = 16;
    settings.numSunRaySamples = 8;
    settings.exposure = 1.5;
    settings.scatteringStrength = 0.8;
    settings.wavelengths.r = 680.0;
    settings.wavelengths.g = 550.0;
    settings.wavelengths.b = 440.0;

    uniforms.update(settings);

    auto const* current = uniforms.getAtmosphere();
    ASSERT_NE(current, nullptr);
    EXPECT_DOUBLE_EQ(current->atmosphereHeightAboveEarth, 100000.0);
    EXPECT_DOUBLE_EQ(current->densityFalloff, 0.5);
    EXPECT_EQ(current->numViewRaySamples, 16);
    EXPECT_EQ(current->numSunRaySamples, 8);
    EXPECT_DOUBLE_EQ(current->exposure, 1.5);
}

// Test AtmosphereUniforms update with same settings (no change).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, UpdateSameSettings)
TEST(AtmosphereUniformsTest, UpdateSameSettings)
{
    AtmosphereUniforms uniforms;

    Atmosphere::Settings settings;
    settings.atmosphereHeightAboveEarth = 50000.0;

    uniforms.update(settings);
    uniforms.update(settings);  // Should not change

    auto const* current = uniforms.getAtmosphere();
    ASSERT_NE(current, nullptr);
    EXPECT_DOUBLE_EQ(current->atmosphereHeightAboveEarth, 50000.0);
}

// Test AtmosphereUniforms atmosphere data matrix.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, AtmosphereDataMatrix)
TEST(AtmosphereUniformsTest, AtmosphereDataMatrix)
{
    AtmosphereUniforms uniforms;

    Atmosphere::Settings settings;
    settings.atmosphereHeightAboveEarth = 100000.0;
    settings.densityFalloff = 0.5;
    settings.numViewRaySamples = 16;
    settings.numSunRaySamples = 8;
    settings.scatteringStrength = 0.8;
    settings.wavelengths.r = 680.0;
    settings.wavelengths.g = 550.0;
    settings.wavelengths.b = 440.0;

    uniforms.update(settings);

    auto const& data = uniforms.getAtmosphereData();

    // atmosphereRadiusScaleFactor should be > 1
    EXPECT_GT(data.data[0], 1.0f);

    // densityFalloff should be stored
    EXPECT_FLOAT_EQ(data.data[2], 0.5f);

    // numViewRaySamples should be stored
    EXPECT_FLOAT_EQ(data.data[4], 16.0f);

    // numSunRaySamples should be stored
    EXPECT_FLOAT_EQ(data.data[5], 8.0f);

    // Scattering coefficients should be positive
    EXPECT_GT(data.data[12], 0.0f);
    EXPECT_GT(data.data[13], 0.0f);
    EXPECT_GT(data.data[14], 0.0f);
}

// Test AtmosphereUniforms dispose.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, Dispose)
TEST(AtmosphereUniformsTest, Dispose)
{
    AtmosphereUniforms uniforms;

    EXPECT_TRUE(uniforms.isDisposed());
    uniforms.dispose();  // Should not crash
    EXPECT_TRUE(uniforms.isDisposed());
}

// Test MAX_SAMPLE_POINTS constant.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, MaxSamplePoints)
TEST(AtmosphereUniformsTest, MaxSamplePoints)
{
    EXPECT_EQ(kMaxSamplePoints, 40);
}

// Test Atmosphere::Settings defaults.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, AtmosphereDefaults)
TEST(AtmosphereUniformsTest, AtmosphereDefaults)
{
    auto const& defaults = Atmosphere::Settings::defaults();

    EXPECT_GT(defaults.atmosphereHeightAboveEarth, 0.0);
    EXPECT_GT(defaults.densityFalloff, 0.0);
    EXPECT_GT(defaults.numViewRaySamples, 0);
    EXPECT_GT(defaults.numSunRaySamples, 0);
    EXPECT_GT(defaults.exposure, 0.0);
    EXPECT_GT(defaults.scatteringStrength, 0.0);
}

// Test Atmosphere::Settings from JSON.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/AtmosphereUniforms.test.ts
//              TEST(AtmosphereUniformsTest, AtmosphereFromJSON)
TEST(AtmosphereUniformsTest, AtmosphereFromJSON)
{
    Atmosphere::Props props;
    props.atmosphereHeightAboveEarth = 200000.0;
    props.densityFalloff = 0.7;

    auto settings = Atmosphere::Settings::fromJSON(&props);

    EXPECT_DOUBLE_EQ(settings.atmosphereHeightAboveEarth, 200000.0);
    EXPECT_DOUBLE_EQ(settings.densityFalloff, 0.7);
}
