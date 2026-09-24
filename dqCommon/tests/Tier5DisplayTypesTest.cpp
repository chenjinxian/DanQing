// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Tier 5 display types unit tests
//
// Authored: no reference tests exist in itwinjs-core for these types
#include <dqCommon/ElementMesh.h>
#include <dqCommon/OctEncodedNormal.h>
#include <dqCommon/RealityModelDisplaySettings.h>
#include <dqCommon/SpatialClassification.h>

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;

// ---------------------------------------------------------------------------
// OctEncodedNormal
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(OctEncodedNormalTest, EncodeUpVector)
{
    auto val = OctEncodedNormal::encode(Vector3d::From(0, 0, 1));
    OctEncodedNormal n(val);
    auto decoded = n.decode();
    EXPECT_NEAR(decoded.z, 1.0, 0.01);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(OctEncodedNormalTest, EncodeDownVector)
{
    auto val = OctEncodedNormal::encode(Vector3d::From(0, 0, -1));
    OctEncodedNormal n(val);
    auto decoded = n.decode();
    EXPECT_NEAR(decoded.z, -1.0, 0.01);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(OctEncodedNormalTest, Roundtrip)
{
    Vector3d original = Vector3d::From(0.577, 0.577, 0.577);
    auto val = OctEncodedNormal::encode(original);
    auto decoded = OctEncodedNormal::decodeValue(val);
    // Oct encoding is lossy, but should be close
    EXPECT_NEAR(decoded.x, original.x, 0.01);
    EXPECT_NEAR(decoded.y, original.y, 0.01);
    EXPECT_NEAR(decoded.z, original.z, 0.01);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(OctEncodedNormalTest, Equality)
{
    OctEncodedNormal a(100);
    OctEncodedNormal b(100);
    OctEncodedNormal c(200);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ---------------------------------------------------------------------------
// ElementMesh
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ElementMeshOptionsTest, DefaultConstruction)
{
    ElementMeshOptions opts;
    EXPECT_FALSE(opts.chordTolerance.has_value());
    EXPECT_FALSE(opts.angleTolerance.has_value());
    EXPECT_FALSE(opts.minBRepFeatureSize.has_value());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(ElementMeshRequestPropsTest, WithSource)
{
    ElementMeshRequestProps props;
    props.source = "0x42";
    props.chordTolerance = 0.01;
    EXPECT_EQ(props.source, "0x42");
    EXPECT_NEAR(*props.chordTolerance, 0.01, 1e-10);
}

// ---------------------------------------------------------------------------
// RealityModelDisplaySettings
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(PointCloudDisplaySettingsTest, defaults)
{
    const auto& d = PointCloudDisplaySettings::defaults();
    EXPECT_EQ(d.shape, PointCloudShape::Square);
    EXPECT_EQ(d.sizeMode, PointCloudSizeMode::Voxel);
    EXPECT_NEAR(d.pixelSize, 1.0, 1e-10);
    EXPECT_EQ(d.edlMode, PointCloudEDLMode::Off);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(PointCloudDisplaySettingsTest, fromJSON)
{
    PointCloudDisplayProps props;
    props.shape = PointCloudShape::Round;
    props.sizeMode = PointCloudSizeMode::Pixel;
    props.pixelSize = 2.0;
    props.edlMode = PointCloudEDLMode::On;
    props.edlStrength = 0.8;

    auto s = PointCloudDisplaySettings::fromJSON(&props);
    EXPECT_EQ(s.shape, PointCloudShape::Round);
    EXPECT_EQ(s.sizeMode, PointCloudSizeMode::Pixel);
    EXPECT_NEAR(s.pixelSize, 2.0, 1e-10);
    EXPECT_EQ(s.edlMode, PointCloudEDLMode::On);
    EXPECT_NEAR(s.edlStrength, 0.8, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(PointCloudDisplaySettingsTest, Roundtrip)
{
    auto s = PointCloudDisplaySettings::defaults();
    auto j = s.toJSON();
    auto s2 = PointCloudDisplaySettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(RealityModelDisplaySettingsTest, defaults)
{
    const auto& d = RealityModelDisplaySettings::defaults();
    EXPECT_NEAR(d.overrideColorRatio, 0.0, 1e-10);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(RealityModelDisplaySettingsTest, fromJSON)
{
    RealityModelDisplayProps props;
    props.overrideColorRatio = 0.5;
    PointCloudDisplayProps pc;
    pc.shape = PointCloudShape::Round;
    props.pointCloud = pc;

    auto s = RealityModelDisplaySettings::fromJSON(&props);
    EXPECT_NEAR(s.overrideColorRatio, 0.5, 1e-10);
    EXPECT_EQ(s.pointCloud.shape, PointCloudShape::Round);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(RealityModelDisplaySettingsTest, Roundtrip)
{
    auto s = RealityModelDisplaySettings::defaults();
    auto j = s.toJSON();
    auto s2 = RealityModelDisplaySettings::fromJSON(&j);
    EXPECT_TRUE(s.equals(s2));
}

// ---------------------------------------------------------------------------
// SpatialClassification
// ---------------------------------------------------------------------------

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifierFlagsTest, defaults)
{
    SpatialClassifierFlags f;
    EXPECT_EQ(f.inside, SpatialClassifierInsideDisplay::Off);
    EXPECT_EQ(f.outside, SpatialClassifierOutsideDisplay::Off);
    EXPECT_FALSE(f.isVolumeClassifier);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifierFlagsTest, fromJSON)
{
    SpatialClassifierFlagsProps props;
    props.inside = SpatialClassifierInsideDisplay::Dimmed;
    props.outside = SpatialClassifierOutsideDisplay::On;
    props.isVolumeClassifier = true;

    auto f = SpatialClassifierFlags::fromJSON(props);
    EXPECT_EQ(f.inside, SpatialClassifierInsideDisplay::Dimmed);
    EXPECT_EQ(f.outside, SpatialClassifierOutsideDisplay::On);
    EXPECT_TRUE(f.isVolumeClassifier);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifierTest, fromJSON)
{
    SpatialClassifierProps props;
    props.modelId = "0x42";
    props.expand = 10.0;
    props.name = "TestClass";
    props.flags.inside = SpatialClassifierInsideDisplay::On;

    auto c = SpatialClassifier::fromJSON(props);
    EXPECT_EQ(c.modelId, dqBase::DqId(0x42));
    EXPECT_NEAR(c.expand, 10.0, 1e-10);
    EXPECT_EQ(c.name, "TestClass");
    EXPECT_EQ(c.flags.inside, SpatialClassifierInsideDisplay::On);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifierTest, Roundtrip)
{
    SpatialClassifier c;
    c.modelId = dqBase::DqId(100);
    c.expand = 5.0;
    c.name = "Test";

    auto j = c.toJSON();
    auto c2 = SpatialClassifier::fromJSON(j);
    EXPECT_TRUE(c.equals(c2));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifiersTest, AddAndFind)
{
    SpatialClassifiers classifiers;
    EXPECT_TRUE(classifiers.isEmpty());

    SpatialClassifier c1;
    c1.modelId = dqBase::DqId(1);
    c1.name = "C1";
    classifiers.add(c1);

    SpatialClassifier c2;
    c2.modelId = dqBase::DqId(2);
    c2.name = "C2";
    classifiers.add(c2);

    EXPECT_EQ(classifiers.getSize(), 2u);
    EXPECT_TRUE(classifiers.has(dqBase::DqId(1)));
    EXPECT_TRUE(classifiers.has(dqBase::DqId(2)));
    EXPECT_FALSE(classifiers.has(dqBase::DqId(3)));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifiersTest, ActiveClassifier)
{
    SpatialClassifiers classifiers;

    SpatialClassifier c1;
    c1.modelId = dqBase::DqId(1);
    classifiers.add(c1);

    SpatialClassifier c2;
    c2.modelId = dqBase::DqId(2);
    classifiers.add(c2);

    // Default active is index 0
    const auto* active = classifiers.getActive();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->modelId, dqBase::DqId(1));

    classifiers.setActive(1);
    active = classifiers.getActive();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->modelId, dqBase::DqId(2));
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifiersTest, clear)
{
    SpatialClassifiers classifiers;
    SpatialClassifier c;
    c.modelId = dqBase::DqId(1);
    classifiers.add(c);
    EXPECT_EQ(classifiers.getSize(), 1u);

    classifiers.clear();
    EXPECT_TRUE(classifiers.isEmpty());
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifierInsideDisplayTest, Values)
{
    EXPECT_EQ(static_cast<int>(SpatialClassifierInsideDisplay::Off), 0);
    EXPECT_EQ(static_cast<int>(SpatialClassifierInsideDisplay::On), 1);
    EXPECT_EQ(static_cast<int>(SpatialClassifierInsideDisplay::Dimmed), 2);
    EXPECT_EQ(static_cast<int>(SpatialClassifierInsideDisplay::Hilite), 3);
    EXPECT_EQ(static_cast<int>(SpatialClassifierInsideDisplay::ElementColor), 4);
}

// Authored: no reference tests exist in itwinjs-core for these types
TEST(SpatialClassifierOutsideDisplayTest, Values)
{
    EXPECT_EQ(static_cast<int>(SpatialClassifierOutsideDisplay::Off), 0);
    EXPECT_EQ(static_cast<int>(SpatialClassifierOutsideDisplay::On), 1);
    EXPECT_EQ(static_cast<int>(SpatialClassifierOutsideDisplay::Dimmed), 2);
}
