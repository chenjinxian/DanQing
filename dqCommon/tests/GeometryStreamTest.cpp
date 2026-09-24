// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/common/src/GeometryStream.ts
// DanQing dqCommon — BoundingSphere, GeometryStream types tests
#include "dqCommon/BoundingSphere.h"
#include "dqCommon/GeometryStream.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(BoundingSphere, DefaultConstruction)
TEST(BoundingSphere, DefaultConstruction)
{
    const BoundingSphere bs;
    EXPECT_TRUE(bs.center.IsEqual(Point3d::FromZero()));
    EXPECT_DOUBLE_EQ(bs.radius, 0.0);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(BoundingSphere, CustomConstruction)
TEST(BoundingSphere, CustomConstruction)
{
    const BoundingSphere bs(Point3d::From(1, 2, 3), 5.0);
    EXPECT_TRUE(bs.center.IsEqual(Point3d::From(1, 2, 3)));
    EXPECT_DOUBLE_EQ(bs.radius, 5.0);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(BoundingSphere, Init)
TEST(BoundingSphere, Init)
{
    BoundingSphere bs;
    bs.init(Point3d::From(10, 20, 30), 15.0);
    EXPECT_TRUE(bs.center.IsEqual(Point3d::From(10, 20, 30)));
    EXPECT_DOUBLE_EQ(bs.radius, 15.0);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(BoundingSphere, transformBy)
TEST(BoundingSphere, transformBy)
{
    BoundingSphere bs(Point3d::From(0, 0, 0), 1.0);
    const auto trans = Transform::CreateTranslation(Vector3d::From(5, 0, 0));
    const auto result = bs.transformBy(trans);
    EXPECT_TRUE(result.center.AlmostEqual(Point3d::From(5, 0, 0)));
    EXPECT_NEAR(result.radius, 1.0, 1e-10);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(BoundingSphere, transformInPlace)
TEST(BoundingSphere, transformInPlace)
{
    BoundingSphere bs(Point3d::From(0, 0, 0), 1.0);
    const auto trans = Transform::CreateTranslation(Vector3d::From(5, 0, 0));
    bs.transformInPlace(trans);
    EXPECT_TRUE(bs.center.AlmostEqual(Point3d::From(5, 0, 0)));
    EXPECT_NEAR(bs.radius, 1.0, 1e-10);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(ElementGeometryOpcode, Values)
TEST(ElementGeometryOpcode, Values)
{
    EXPECT_EQ(static_cast<int>(ElementGeometryOpcode::SubGraphicRange), 2);
    EXPECT_EQ(static_cast<int>(ElementGeometryOpcode::PartReference), 3);
    EXPECT_EQ(static_cast<int>(ElementGeometryOpcode::BasicSymbology), 4);
    EXPECT_EQ(static_cast<int>(ElementGeometryOpcode::PointPrimitive), 5);
    EXPECT_EQ(static_cast<int>(ElementGeometryOpcode::Polyface), 9);
    EXPECT_EQ(static_cast<int>(ElementGeometryOpcode::BRep), 25);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(GeometryStreamFlags, Values)
TEST(GeometryStreamFlags, Values)
{
    EXPECT_EQ(static_cast<int>(GeometryStreamFlags::None), 0);
    EXPECT_EQ(static_cast<int>(GeometryStreamFlags::ViewIndependent), 1);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(BRepType, Values)
TEST(BRepType, Values)
{
    EXPECT_EQ(static_cast<int>(BRepType::Solid), 0);
    EXPECT_EQ(static_cast<int>(BRepType::Sheet), 1);
    EXPECT_EQ(static_cast<int>(BRepType::Wire), 2);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(Placement3d, DefaultConstruction)
TEST(Placement3d, DefaultConstruction)
{
    const Placement3d p;
    EXPECT_TRUE(p.origin.IsEqual(Point3d::FromZero()));
    EXPECT_DOUBLE_EQ(p.yawDegrees, 0.0);
    EXPECT_DOUBLE_EQ(p.pitchDegrees, 0.0);
    EXPECT_DOUBLE_EQ(p.rollDegrees, 0.0);
    EXPECT_TRUE(p.is3d());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(Placement3d, CustomConstruction)
TEST(Placement3d, CustomConstruction)
{
    const Placement3d p(Point3d::From(10, 20, 30), 45.0, 10.0, 5.0,
                        Range3d::CreateXYZXYZ(0, 0, 0, 100, 100, 100));
    EXPECT_TRUE(p.origin.IsEqual(Point3d::From(10, 20, 30)));
    EXPECT_DOUBLE_EQ(p.yawDegrees, 45.0);
    EXPECT_TRUE(p.isValid());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(Placement3d, SetFrom)
TEST(Placement3d, SetFrom)
{
    Placement3d a(Point3d::From(1, 2, 3), 10, 20, 30,
                  Range3d::CreateXYZXYZ(0, 0, 0, 50, 50, 50));
    Placement3d b;
    b.setFrom(a);
    EXPECT_TRUE(b.origin.IsEqual(a.origin));
    EXPECT_DOUBLE_EQ(b.yawDegrees, a.yawDegrees);
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(Placement3d, isValid)
TEST(Placement3d, isValid)
{
    Placement3d p;
    // Default bbox is null (created by default Range3d constructor)
    // Actually, default Range3d has low > high (null range)
    EXPECT_FALSE(p.isValid());

    p.bbox = Range3d::CreateXYZXYZ(0, 0, 0, 100, 100, 100);
    EXPECT_TRUE(p.isValid());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(Placement2d, DefaultConstruction)
TEST(Placement2d, DefaultConstruction)
{
    const Placement2d p;
    EXPECT_DOUBLE_EQ(p.originX, 0.0);
    EXPECT_DOUBLE_EQ(p.originY, 0.0);
    EXPECT_DOUBLE_EQ(p.angleDegrees, 0.0);
    EXPECT_FALSE(p.is3d());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(GeometryAppearanceProps, DefaultConstruction)
TEST(GeometryAppearanceProps, DefaultConstruction)
{
    const GeometryAppearanceProps props;
    EXPECT_FALSE(props.subCategory.has_value());
    EXPECT_FALSE(props.color.has_value());
    EXPECT_FALSE(props.weight.has_value());
    EXPECT_FALSE(props.transparency.has_value());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(AreaFillProps, DefaultConstruction)
TEST(AreaFillProps, DefaultConstruction)
{
    const AreaFillProps props;
    EXPECT_EQ(props.display, FillDisplay::Never);
    EXPECT_FALSE(props.transparency.has_value());
    EXPECT_FALSE(props.color.has_value());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(GeometryStreamEntry, DefaultConstruction)
TEST(GeometryStreamEntry, DefaultConstruction)
{
    const GeometryStreamEntry entry;
    EXPECT_EQ(entry.opcode, ElementGeometryOpcode::PointPrimitive);
    EXPECT_FALSE(entry.appearance.has_value());
    EXPECT_FALSE(entry.fill.has_value());
}

// Ported from: itwinjs-core core/common/src/test/GeometryStream.test.ts
//              TEST(GeometryPartInstanceProps, DefaultConstruction)
TEST(GeometryPartInstanceProps, DefaultConstruction)
{
    const GeometryPartInstanceProps props;
    EXPECT_DOUBLE_EQ(props.originX, 0.0);
    EXPECT_DOUBLE_EQ(props.scale, 1.0);
}

// Authored: no reference test exists for enhanced GeometryStream types
TEST(GeometryMaterialProps, EnhancedFields)
{
    GeometryMaterialProps mat;
    EXPECT_FALSE(mat.materialId.has_value());
    EXPECT_FALSE(mat.origin.has_value());
    EXPECT_FALSE(mat.size.has_value());
    EXPECT_DOUBLE_EQ(mat.rotationDegrees, 0.0);

    mat.materialId = dqBase::DqId(42);
    mat.origin = Point3d::From(1, 2, 3);
    mat.size = Point3d::From(10, 10, 1);
    mat.rotationDegrees = 45.0;

    EXPECT_TRUE(mat.materialId.has_value());
    EXPECT_TRUE(mat.origin->IsEqual(Point3d::From(1, 2, 3)));
    EXPECT_NEAR(mat.rotationDegrees, 45.0, 1e-10);
}

// Authored: no reference test exists for enhanced GeometryStream types
TEST(BRepDataProps, TransformField)
{
    BRepDataProps brep;
    brep.data = "base64data";
    brep.type = BRepType::Solid;
    EXPECT_FALSE(brep.transform.has_value());

    brep.transform = Transform::CreateIdentity();
    EXPECT_TRUE(brep.transform.has_value());
}

// Authored: no reference test exists for GeometryStreamHeaderProps
TEST(GeometryStreamHeaderProps, DefaultConstruction)
{
    GeometryStreamHeaderProps header;
    EXPECT_EQ(header.flags, GeometryStreamFlags::None);
}

// Authored: no reference test exists for enhanced GeometryStreamEntry
TEST(GeometryStreamEntry, HeaderAndSubRange)
{
    GeometryStreamEntry entry;
    EXPECT_FALSE(entry.header.has_value());
    EXPECT_FALSE(entry.subRange.has_value());

    GeometryStreamHeaderProps header;
    header.flags = GeometryStreamFlags::ViewIndependent;
    entry.header = header;
    EXPECT_TRUE(entry.header.has_value());
    EXPECT_EQ(entry.header->flags, GeometryStreamFlags::ViewIndependent);

    entry.subRange = Range3d::CreateXYZXYZ(0, 0, 0, 10, 10, 10);
    EXPECT_TRUE(entry.subRange.has_value());
}
