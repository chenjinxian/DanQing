// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextString tests
// Ported from: itwinjs-core core/common/src/geometry/TextString.ts（实现语义源）。
// Authored: no reference test exists in core-common/core-backend for
// TextString.transformInPlace/toJSON in isolation（core-backend
// GeometryStream.test.ts:1254+ 仅经 ElementGeometry.appendTextString 间接消费，
// 随 Y-a4 接线时直移）。断言值按参考实现 TextString.ts:75-126 语义推导。
#include <gtest/gtest.h>

#include <dqCommon/TextString.h>

namespace {

// Ported from: TextString.ts:75 — width() = height * (widthFactor ?? 1.0)
TEST(TextString, WidthDefaultsToHeight)
{
    dqCommon::TextString ts({.text = "Hi", .font = 1, .height = 2.0});
    EXPECT_DOUBLE_EQ(ts.width(), 2.0);
    ts.widthFactor = 0.5;
    EXPECT_DOUBLE_EQ(ts.width(), 1.0);
}

// Ported from: TextString.ts:88-105 — toJSON omits ~zero origin / identity rotation.
TEST(TextString, ToJSONOmitsDefaults)
{
    dqCommon::TextString ts({.text = "A", .font = 3, .height = 1.5, .widthFactor = 0.8,
                             .bold = true});
    auto props = ts.toJSON();
    EXPECT_EQ(props.text, "A");
    EXPECT_EQ(props.font, 3u);
    EXPECT_DOUBLE_EQ(props.height, 1.5);
    ASSERT_TRUE(props.widthFactor.has_value());
    EXPECT_DOUBLE_EQ(*props.widthFactor, 0.8);
    EXPECT_TRUE(props.bold);
    EXPECT_FALSE(props.italic);
    EXPECT_FALSE(props.underline);
    // default origin/rotation omitted
    EXPECT_TRUE(props.origin.AlmostEqual(dqGeom::Point3d::FromZero(), 0.0));
    EXPECT_TRUE(props.rotation.isIdentity());
}

// Ported from: TextString.ts:107-126 — translation moves origin; size fields
// unchanged; uniform size keeps widthFactor unset.
TEST(TextString, TransformByTranslation)
{
    dqCommon::TextString ts({.text = "T", .font = 1, .height = 2.0});
    dqGeom::Transform const t = dqGeom::Transform::CreateTranslation(10.0, 20.0, 30.0);
    ASSERT_TRUE(ts.transformInPlace(t));
    EXPECT_TRUE(ts.origin.AlmostEqual(dqGeom::Point3d::From(10.0, 20.0, 30.0), 1.0e-12));
    EXPECT_DOUBLE_EQ(ts.height, 2.0);
    EXPECT_FALSE(ts.widthFactor.has_value());
}

// Ported from: TextString.ts:107-126 — non-uniform scale folds into
// height/widthFactor (scales.y → height; width/height ratio preserved).
TEST(TextString, TransformByNonUniformScale)
{
    dqCommon::TextString ts({.text = "S", .font = 1, .height = 3.0});
    dqGeom::Transform t;
    t.matrix = dqGeom::Matrix3d::CreateScale(2.0, 3.0, 1.0);
    ASSERT_TRUE(ts.transformInPlace(t));
    EXPECT_DOUBLE_EQ(ts.height, 9.0);           // 3.0 * scales.y(=3)
    ASSERT_TRUE(ts.widthFactor.has_value());    // newWidth(6)/newHeight(9)
    EXPECT_NEAR(*ts.widthFactor, 2.0 / 3.0, 1.0e-12);
}

// Ported from: TextString.ts:107-126 — rotation composes into rotation;
// origin transforms through the full transform.
TEST(TextString, TransformByRotationAndTranslation)
{
    dqCommon::TextString ts({.text = "R", .font = 1, .height = 2.0,
                             .origin = dqGeom::Point3d::From(1.0, 0.0, 0.0)});
    dqGeom::Transform t;
    t.matrix = dqGeom::YawPitchRollAngles::CreateDegrees(90.0, 0.0, 0.0).ToMatrix3d();
    t.origin = dqGeom::Point3d::From(10.0, 20.0, 30.0);
    ASSERT_TRUE(ts.transformInPlace(t));
    // (1,0,0) rotated 90° about Z → (0,1,0); + (10,20,30) → (10,21,30)
    EXPECT_TRUE(ts.origin.AlmostEqual(dqGeom::Point3d::From(10.0, 21.0, 30.0), 1.0e-9));
    EXPECT_NEAR(ts.rotation.yaw.Degrees(), 90.0, 1.0e-9);
    EXPECT_DOUBLE_EQ(ts.height, 2.0);
    EXPECT_FALSE(ts.widthFactor.has_value());
}

// Ported from: TextString.ts:113-114 — degenerate transform (zero column)
// returns false and leaves the TextString unaltered.
TEST(TextString, DegenerateTransformRejected)
{
    dqCommon::TextString ts({.text = "D", .font = 1, .height = 2.0});
    dqGeom::Transform t;
    t.matrix = dqGeom::Matrix3d::CreateScale(1.0, 0.0, 1.0);  // zero Y column
    EXPECT_FALSE(ts.transformInPlace(t));
    EXPECT_DOUBLE_EQ(ts.height, 2.0);
    EXPECT_TRUE(ts.origin.AlmostEqual(dqGeom::Point3d::FromZero(), 0.0));
}

}  // namespace
