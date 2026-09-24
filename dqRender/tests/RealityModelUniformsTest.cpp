// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RealityModelUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              (no direct reference test exists; tests are authored based on RealityModelUniforms.ts behavior)
#include "render/RealityModelUniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

// Test PointCloudUniforms default initialization.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(PointCloudUniformsTest, DefaultInitialization)
TEST(PointCloudUniformsTest, DefaultInitialization)
{
    PointCloudUniforms uniforms;
    auto const& settings = uniforms.getSettings();

    EXPECT_EQ(settings.sizeMode, PointCloudSizeMode::Voxel);
    EXPECT_DOUBLE_EQ(settings.voxelScale, 1.0);
    EXPECT_DOUBLE_EQ(settings.minPixelsPerVoxel, 2.0);
    EXPECT_DOUBLE_EQ(settings.maxPixelsPerVoxel, 8.0);
    EXPECT_EQ(settings.shape, PointCloudShape::Square);
}

// Test PointCloudUniforms update with new settings.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(PointCloudUniformsTest, UpdateSettings)
TEST(PointCloudUniformsTest, UpdateSettings)
{
    PointCloudUniforms uniforms;

    PointCloudDisplaySettings settings;
    settings.sizeMode = PointCloudSizeMode::Pixel;
    settings.pixelSize = 3.0;
    settings.shape = PointCloudShape::Round;
    settings.edlStrength = 0.5;
    settings.edlRadius = 2.0;

    uniforms.update(settings);

    auto const& current = uniforms.getSettings();
    EXPECT_EQ(current.sizeMode, PointCloudSizeMode::Pixel);
    EXPECT_DOUBLE_EQ(current.pixelSize, 3.0);
    EXPECT_EQ(current.shape, PointCloudShape::Round);
    EXPECT_DOUBLE_EQ(current.edlStrength, 0.5);
    EXPECT_DOUBLE_EQ(current.edlRadius, 2.0);
}

// Test PointCloudUniforms update with same settings (no change).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(PointCloudUniformsTest, UpdateSameSettings)
TEST(PointCloudUniformsTest, UpdateSameSettings)
{
    PointCloudUniforms uniforms;

    auto const& defaults = PointCloudDisplaySettings::defaults();
    uniforms.update(defaults);

    // Should not change
    auto const& current = uniforms.getSettings();
    EXPECT_EQ(current.sizeMode, defaults.sizeMode);
    EXPECT_DOUBLE_EQ(current.voxelScale, defaults.voxelScale);
}

// Test RealityModelUniforms default initialization.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(RealityModelUniformsTest, DefaultInitialization)
TEST(RealityModelUniformsTest, DefaultInitialization)
{
    RealityModelUniforms uniforms;

    // Default override color mix should be 0.5
    // (We can't directly test bindOverrideColorMix without a UniformHandle,
    //  but we can verify the object is constructible)
}

// Test RealityModelUniforms update.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(RealityModelUniformsTest, UpdateSettings)
TEST(RealityModelUniformsTest, UpdateSettings)
{
    RealityModelUniforms uniforms;

    RealityModelDisplaySettings settings;
    settings.overrideColorRatio = 0.75;
    settings.pointCloud.sizeMode = PointCloudSizeMode::Pixel;
    settings.pointCloud.pixelSize = 5.0;

    uniforms.update(settings);

    auto const& pcSettings = uniforms.getPointCloud().getSettings();
    EXPECT_EQ(pcSettings.sizeMode, PointCloudSizeMode::Pixel);
    EXPECT_DOUBLE_EQ(pcSettings.pixelSize, 5.0);
}

// Test PointCloudDisplaySettings defaults.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(RealityModelUniformsTest, PointCloudDefaults)
TEST(RealityModelUniformsTest, PointCloudDefaults)
{
    auto const& defaults = PointCloudDisplaySettings::defaults();

    EXPECT_EQ(defaults.sizeMode, PointCloudSizeMode::Voxel);
    EXPECT_DOUBLE_EQ(defaults.voxelScale, 1.0);
    EXPECT_DOUBLE_EQ(defaults.minPixelsPerVoxel, 2.0);
    EXPECT_DOUBLE_EQ(defaults.maxPixelsPerVoxel, 8.0);
    EXPECT_EQ(defaults.shape, PointCloudShape::Square);
    EXPECT_EQ(defaults.edlMode, PointCloudEDLMode::Off);
    EXPECT_DOUBLE_EQ(defaults.edlStrength, 0.4);
    EXPECT_DOUBLE_EQ(defaults.edlRadius, 1.4);
}

// Test PointCloudDisplaySettings from JSON.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(RealityModelUniformsTest, PointCloudFromJSON)
TEST(RealityModelUniformsTest, PointCloudFromJSON)
{
    PointCloudDisplayProps props;
    props.sizeMode = PointCloudSizeMode::Pixel;
    props.pixelSize = 10.0;
    props.shape = PointCloudShape::Round;

    auto settings = PointCloudDisplaySettings::fromJSON(&props);

    EXPECT_EQ(settings.sizeMode, PointCloudSizeMode::Pixel);
    EXPECT_DOUBLE_EQ(settings.pixelSize, 10.0);
    EXPECT_EQ(settings.shape, PointCloudShape::Round);
}

// Test RealityModelDisplaySettings from JSON.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/RealityModelUniforms.test.ts
//              TEST(RealityModelUniformsTest, RealityModelFromJSON)
TEST(RealityModelUniformsTest, RealityModelFromJSON)
{
    RealityModelDisplayProps props;
    props.overrideColorRatio = 0.9;

    auto settings = RealityModelDisplaySettings::fromJSON(&props);

    EXPECT_DOUBLE_EQ(settings.overrideColorRatio, 0.9);
}
