// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — GeometryStreamBuilder unit tests
//
// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
#include <dqCommon/GeometryStreamBuilder.h>

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;
using namespace dqBase;

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, EmptyBuilder)
{
    GeometryStreamBuilder builder;
    EXPECT_TRUE(builder.isEmpty());
    EXPECT_EQ(builder.getSize(), 0u);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, ViewIndependent)
{
    GeometryStreamBuilder builder;
    EXPECT_FALSE(builder.isViewIndependent());
    builder.setViewIndependent(true);
    EXPECT_TRUE(builder.isViewIndependent());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, SubCategoryChange)
{
    GeometryStreamBuilder builder;
    GeometryAppearanceProps app;
    app.subCategory = DqId(42);
    app.color = 0xFF0000u;

    builder.appendSubCategoryChange(app);
    EXPECT_EQ(builder.getSize(), 1u);
    EXPECT_EQ(builder.getEntries()[0].opcode, ElementGeometryOpcode::BasicSymbology);
    ASSERT_TRUE(builder.getEntries()[0].appearance.has_value());
    EXPECT_EQ(builder.getEntries()[0].appearance->color, 0xFF0000u);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, BRepData)
{
    GeometryStreamBuilder builder;
    BRepDataProps brep;
    brep.data = "base64_encoded_solid";
    brep.type = BRepType::Solid;

    builder.appendBRepData(brep);
    EXPECT_EQ(builder.getSize(), 1u);
    EXPECT_EQ(builder.getEntries()[0].opcode, ElementGeometryOpcode::BRep);
    ASSERT_TRUE(builder.getEntries()[0].brep.has_value());
    EXPECT_EQ(builder.getEntries()[0].brep->data, "base64_encoded_solid");
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, GeometryPartReference)
{
    GeometryStreamBuilder builder;
    GeometryPartInstanceProps part;
    part.partId = DqId(100);
    part.originX = 10.0;
    part.originY = 20.0;
    part.originZ = 30.0;
    part.scale = 2.0;

    builder.appendGeometryPart3d(part);
    EXPECT_EQ(builder.getSize(), 1u);
    EXPECT_EQ(builder.getEntries()[0].opcode, ElementGeometryOpcode::PartReference);
    ASSERT_TRUE(builder.getEntries()[0].partRef.has_value());
    EXPECT_EQ(builder.getEntries()[0].partRef->partId, DqId(100));
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, MultipleEntries)
{
    GeometryStreamBuilder builder;

    // add appearance
    GeometryAppearanceProps app;
    app.subCategory = DqId(1);
    builder.appendSubCategoryChange(app);

    // add fill
    AreaFillProps fill;
    fill.display = FillDisplay::Always;
    builder.appendFill(fill);

    // add material
    GeometryMaterialProps mat;
    mat.materialId = DqId(50);
    builder.appendMaterial(mat);

    EXPECT_EQ(builder.getSize(), 3u);
    EXPECT_EQ(builder.getEntries()[0].opcode, ElementGeometryOpcode::BasicSymbology);
    EXPECT_EQ(builder.getEntries()[1].opcode, ElementGeometryOpcode::Fill);
    EXPECT_EQ(builder.getEntries()[2].opcode, ElementGeometryOpcode::Material);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, ClearAndReuse)
{
    GeometryStreamBuilder builder;
    builder.appendSubCategoryChange(GeometryAppearanceProps{});
    EXPECT_EQ(builder.getSize(), 1u);

    builder.clear();
    EXPECT_TRUE(builder.isEmpty());

    builder.appendFill(AreaFillProps{});
    EXPECT_EQ(builder.getSize(), 1u);
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, takeEntries)
{
    GeometryStreamBuilder builder;
    builder.appendSubCategoryChange(GeometryAppearanceProps{});
    builder.appendFill(AreaFillProps{});

    auto entries = builder.takeEntries();
    EXPECT_EQ(entries.size(), 2u);
    EXPECT_TRUE(builder.isEmpty());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, Placement3d)
{
    GeometryStreamBuilder builder;
    Placement3d placement;
    placement.origin = Point3d::From(1, 2, 3);
    placement.bbox = Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10);

    builder.setLocalToWorld3d(placement);
    EXPECT_EQ(builder.getSize(), 1u);
    EXPECT_EQ(builder.getEntries()[0].opcode, ElementGeometryOpcode::SubGraphicRange);
    ASSERT_TRUE(builder.getEntries()[0].subRange.has_value());
}

// Authored: no reference tests exist in itwinjs-core for GeometryStreamBuilder C++ port
TEST(GeometryStreamBuilderTest, GeometryParamsChange)
{
    GeometryStreamBuilder builder;
    GeometryAppearanceProps app;
    app.color = 0x00FF00u;
    AreaFillProps fill;
    fill.display = FillDisplay::ByView;
    GeometryMaterialProps mat;
    mat.materialId = DqId(99);

    builder.appendGeometryParamsChange(app, &fill, &mat);
    EXPECT_EQ(builder.getSize(), 1u);
    auto& entry = builder.getEntries()[0];
    EXPECT_EQ(entry.opcode, ElementGeometryOpcode::BasicSymbology);
    ASSERT_TRUE(entry.appearance.has_value());
    ASSERT_TRUE(entry.fill.has_value());
    ASSERT_TRUE(entry.material.has_value());
    EXPECT_EQ(entry.fill->display, FillDisplay::ByView);
}

// Authored: no reference test exists in itwinjs-core for GeometryStreamBuilder
//   (core/common/src/test has no GeometryStream.test.ts); the following cases exercise the
//   achievable gap-fill methods added against GeometryStream.ts. Behavior verified against the
//   GeometryStream.ts GeometryStreamBuilder contract.

// Authored: exercises appendGeometryRanges (ref GeometryStream.ts:285 appendGeometryRanges)
TEST(GeometryStreamBuilderTest, appendGeometryRanges)
{
    GeometryStreamBuilder builder;
    EXPECT_FALSE(builder.hasLocalToWorld());

    builder.appendGeometryRanges();
    ASSERT_EQ(builder.getSize(), 1u);
    const auto& entry = builder.getEntries()[0];
    EXPECT_EQ(entry.opcode, ElementGeometryOpcode::SubGraphicRange);
    ASSERT_TRUE(entry.subRange.has_value());
    // ref pushes Range3d.createNull() — the marker entry's range is null.
    EXPECT_TRUE(entry.subRange->isNull());
}

// Authored: exercises appendGeometryPart2d (ref GeometryStream.ts:320 appendGeometryPart2d)
TEST(GeometryStreamBuilderTest, appendGeometryPart2d)
{
    GeometryStreamBuilder builder;
    builder.appendGeometryPart2d(DqId(7), 10.0, 20.0, 45.0, 2.0);
    ASSERT_EQ(builder.getSize(), 1u);
    const auto& entry = builder.getEntries()[0];
    EXPECT_EQ(entry.opcode, ElementGeometryOpcode::PartReference);
    ASSERT_TRUE(entry.partRef.has_value());
    EXPECT_EQ(entry.partRef->partId, DqId(7));
    EXPECT_NEAR(entry.partRef->originX, 10.0, 1e-12);
    EXPECT_NEAR(entry.partRef->originY, 20.0, 1e-12);
    EXPECT_NEAR(entry.partRef->originZ, 0.0, 1e-12);  // 2d parts live in z=0 plane
    EXPECT_NEAR(entry.partRef->yawDegrees, 45.0, 1e-12);  // 2d rotation = yaw about +Z
    EXPECT_NEAR(entry.partRef->scale, 2.0, 1e-12);
}

// Authored: exercises appendGeometryPart2d defaults (no origin/rotation/scale supplied)
TEST(GeometryStreamBuilderTest, AppendGeometryPart2dDefaults)
{
    GeometryStreamBuilder builder;
    builder.appendGeometryPart2d(DqId(1));
    ASSERT_EQ(builder.getSize(), 1u);
    const auto& part = *builder.getEntries()[0].partRef;
    EXPECT_EQ(part.partId, DqId(1));
    EXPECT_NEAR(part.originX, 0.0, 1e-12);
    EXPECT_NEAR(part.originY, 0.0, 1e-12);
    EXPECT_NEAR(part.yawDegrees, 0.0, 1e-12);
    EXPECT_NEAR(part.scale, 1.0, 1e-12);
}

// Authored: exercises setLocalToWorld(Transform) (ref GeometryStream.ts:202 setLocalToWorld)
TEST(GeometryStreamBuilderTest, SetLocalToWorldTransform)
{
    GeometryStreamBuilder builder;
    EXPECT_FALSE(builder.hasLocalToWorld());

    // identity transform clears / leaves no world-to-local
    builder.setLocalToWorld(Transform::CreateIdentity());
    EXPECT_FALSE(builder.hasLocalToWorld());

    // Non-identity translation establishes world-to-local
    const auto trans = Transform::CreateTranslation(Vector3d::From(5, 10, 15));
    builder.setLocalToWorld(trans);
    EXPECT_TRUE(builder.hasLocalToWorld());

    // Zero-arg overload clears it again
    builder.setLocalToWorld();
    EXPECT_FALSE(builder.hasLocalToWorld());
}
