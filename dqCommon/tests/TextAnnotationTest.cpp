// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — TextAnnotation tests
// Ported from: itwinjs-core core/common/src/test/annotations/TextAnnotation.test.ts
//              describe("TextAnnotation") — 全 describe 直移。
#include <gtest/gtest.h>

#include <dqCommon/TextAnnotation.h>

#include <utility>

namespace {

using namespace dqCommon;

// describe("computeAnchorPoint") — expectAnchor 助手（:7-16）。
static void expectAnchor(double x, double y, char const* horizontal, char const* vertical)
{
    TextAnnotationProps props;
    props.anchor = TextAnnotationProps::Anchor{vertical, horizontal};
    auto annotation = TextAnnotation::fromJSON(&props);
    dqGeom::Range2d const extents(0.0, 0.0, 20.0, 10.0);
    auto const actual = annotation.computeAnchorPoint(extents);
    EXPECT_DOUBLE_EQ(actual.x, x);
    EXPECT_DOUBLE_EQ(actual.y, y);
}

// it("should compute anchor point based on anchor settings and text block dimensions")
TEST(TextAnnotation_ComputeAnchorPoint, ComputesFromAnchorAndDimensions)
{
    expectAnchor(0.0, 10.0, "left", "top");
    expectAnchor(0.0, 0.0, "left", "bottom");
    expectAnchor(0.0, 5.0, "left", "middle");
    expectAnchor(20.0, 10.0, "right", "top");
    expectAnchor(20.0, 0.0, "right", "bottom");
    expectAnchor(20.0, 5.0, "right", "middle");
    expectAnchor(10.0, 10.0, "center", "top");
    expectAnchor(10.0, 0.0, "center", "bottom");
    expectAnchor(10.0, 5.0, "center", "middle");
}

// describe("computeTransform") — expectAnchorAtOrigin 内嵌助手（:42-48）。
TEST(TextAnnotation_ComputeTransform, AlignsAnchorWithOriginPriorToOffsetTranslation)
{
    char const* verticals[] = {"top", "middle", "bottom"};
    char const* horizontals[] = {"left", "center", "right"};
    dqGeom::Range2d const extents(0.0, -10.0, 20.0, 0.0);

    for (auto const* horizontal : horizontals) {
        for (auto const* vertical : verticals) {
            TextAnnotationProps props;
            props.anchor = TextAnnotationProps::Anchor{vertical, horizontal};
            auto annotation = TextAnnotation::fromJSON(&props);

            auto const expectAnchorAtOrigin = [&](double scale = 1.0) {
                auto const transform = annotation.computeTransform(extents, scale);
                auto const anchor = annotation.computeAnchorPoint(extents);
                auto const transformed = transform.MultiplyPoint3d(anchor);
                EXPECT_TRUE(transformed.AlmostEqual(annotation.offset, 1.0e-6));
            };

            expectAnchorAtOrigin();       // No offset nor rotation
            expectAnchorAtOrigin(2.0);    // Scale only

            annotation.orientation =
                dqGeom::YawPitchRollAngles::CreateDegrees(45.0, 0.0, 0.0);  // Rotation only
            expectAnchorAtOrigin();

            annotation.orientation = dqGeom::YawPitchRollAngles{};
            annotation.offset = dqGeom::Point3d::From(4.0, -6.0, 0.0);          // Offset only
            expectAnchorAtOrigin();

            annotation.orientation =
                dqGeom::YawPitchRollAngles::CreateDegrees(45.0, 0.0, 0.0);  // Offset and rotation
            expectAnchorAtOrigin();

            expectAnchorAtOrigin(2.0);    // Offset and scale and rotation
        }
    }
}

// expectTransformedRange（:75-103）——anchor 平移修正 + 乘 range 后全等。
struct TransformRangeOptions {
    bool hasAnchor = false;
    TextAnnotationProps::Anchor anchor;
    bool hasOrigin = false;
    double originX = 0.0, originY = 0.0;
    double rotationDegrees = 0.0;
    bool hasRotation = false;
};

static void expectTransformedRange(std::array<double, 4> const& expectedRange,
                                   TransformRangeOptions const& options)
{
    TextAnnotationProps props;
    if (options.hasAnchor)
        props.anchor = options.anchor;
    if (options.hasOrigin)
        props.offset = dqGeom::Point3d::From(options.originX, options.originY, 0.0);
    if (options.hasRotation)
        props.orientation =
            dqGeom::YawPitchRollAngles::CreateDegrees(options.rotationDegrees, 0.0, 0.0);
    auto annotation = TextAnnotation::fromJSON(&props);

    // NB: In TextBlock coordinates, the origin is at the top-left.
    double const dimX = 20.0, dimY = 10.0;
    dqGeom::Range3d const extents(0.0, -dimY, 0.0, dimX, 0.0, 0.0);
    auto const transform = annotation.computeTransform(
        dqGeom::Range2d(0.0, -dimY, dimX, 0.0));

    // Tests predate anchor-alignment; subtract the anchor from the expected range.
    auto const anchor = annotation.computeAnchorPoint(dqGeom::Range2d(0.0, -dimY, dimX, 0.0));
    dqGeom::Range3d expected(expectedRange[0], expectedRange[1], 0.0, expectedRange[2],
                             expectedRange[3], 0.0);
    expected.low.x -= anchor.x;
    expected.high.x -= anchor.x;
    expected.low.y -= anchor.y;
    expected.high.y -= anchor.y;

    auto const actual = transform.MultiplyRange(extents);
    EXPECT_TRUE(actual.IsAlmostEqual(expected, 1.0e-6));
}

// it("should produce identity transform for identity orientation, zero origin, and 1 scaling")
TEST(TextAnnotation_ComputeTransform, IdentityForIdentityOrientationZeroOriginOneScale)
{
    char const* verticals[] = {"top", "middle", "bottom"};
    char const* horizontals[] = {"left", "center", "right"};
    for (auto const* vertical : verticals) {
        for (auto const* horizontal : horizontals) {
            TransformRangeOptions o;
            o.hasAnchor = true;
            o.anchor = {vertical, horizontal};
            expectTransformedRange({0.0, -10.0, 20.0, 0.0}, o);
        }
    }
}

// it("should translate relative to anchor point")
TEST(TextAnnotation_ComputeTransform, TranslatesRelativeToAnchorPoint)
{
    char const* verticals[] = {"top", "middle", "bottom"};
    char const* horizontals[] = {"left", "center", "right"};
    for (auto const* vertical : verticals) {
        for (auto const* horizontal : horizontals) {
            TransformRangeOptions o;
            o.hasAnchor = true;
            o.anchor = {vertical, horizontal};
            o.hasOrigin = true;
            o.originX = -5.0;
            o.originY = 20.0;
            expectTransformedRange({-5.0, 10.0, 15.0, 20.0}, o);
        }
    }
}

// it("should rotate about fixed anchor point") — 9 组锚点 × 90°。
TEST(TextAnnotation_ComputeTransform, RotatesAboutFixedAnchorPoint)
{
    auto mk = [](char const* h, char const* v) {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {v, h};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        return o;
    };
    expectTransformedRange({0.0, 0.0, 10.0, 20.0}, mk("left", "top"));
    expectTransformedRange({-10.0, -10.0, 0.0, 10.0}, mk("left", "bottom"));
    expectTransformedRange({-5.0, -5.0, 5.0, 15.0}, mk("left", "middle"));
    expectTransformedRange({5.0, -15.0, 15.0, 5.0}, mk("center", "middle"));
    expectTransformedRange({10.0, -10.0, 20.0, 10.0}, mk("center", "top"));
    expectTransformedRange({0.0, -20.0, 10.0, 0.0}, mk("center", "bottom"));
    expectTransformedRange({20.0, -20.0, 30.0, 0.0}, mk("right", "top"));
    expectTransformedRange({10.0, -30.0, 20.0, -10.0}, mk("right", "bottom"));
    expectTransformedRange({15.0, -25.0, 25.0, -5.0}, mk("right", "middle"));
}

// it("should apply translation after rotation about fixed anchor point")
TEST(TextAnnotation_ComputeTransform, AppliesTranslationAfterRotation)
{
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"middle", "center"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = -5.0;
        o.originY = 10.0;
        expectTransformedRange({0.0, -5.0, 10.0, 15.0}, o);
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"bottom", "left"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = -1.0;
        o.originY = 2.0;
        expectTransformedRange({-11.0, -8.0, -1.0, 12.0}, o);
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"top", "right"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = 0.0;
        o.originY = 100.0;
        expectTransformedRange({20.0, 80.0, 30.0, 100.0}, o);
    }
}

// describe("scaling") — expectScaledTransform（:181-200，无 anchor 修正）。
static void expectScaledTransform(std::array<double, 4> const& expectedRange,
                                  TransformRangeOptions const& options)
{
    TextAnnotationProps props;
    if (options.hasAnchor)
        props.anchor = options.anchor;
    if (options.hasOrigin)
        props.offset = dqGeom::Point3d::From(options.originX, options.originY, 0.0);
    if (options.hasRotation)
        props.orientation =
            dqGeom::YawPitchRollAngles::CreateDegrees(options.rotationDegrees, 0.0, 0.0);
    auto annotation = TextAnnotation::fromJSON(&props);

    double const dimX = 20.0, dimY = 10.0;
    dqGeom::Range3d const extents(0.0, -dimY, 0.0, dimX, 0.0, 0.0);
    auto const transform = annotation.computeTransform(
        dqGeom::Range2d(0.0, -dimY, dimX, 0.0), 2.0);
    dqGeom::Range3d expected(expectedRange[0], expectedRange[1], 0.0, expectedRange[2],
                             expectedRange[3], 0.0);
    auto const actual = transform.MultiplyRange(extents);
    EXPECT_TRUE(actual.IsAlmostEqual(expected, 1.0e-6));
}

// it("should scale about fixed anchor point") — 9 组锚点 × scale 2。
TEST(TextAnnotation_ComputeTransform, ScalesAboutFixedAnchorPoint)
{
    auto mk = [](char const* h, char const* v) {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {v, h};
        return o;
    };
    expectScaledTransform({0.0, -20.0, 40.0, 0.0}, mk("left", "top"));
    expectScaledTransform({0.0, 0.0, 40.0, 20.0}, mk("left", "bottom"));
    expectScaledTransform({0.0, -10.0, 40.0, 10.0}, mk("left", "middle"));
    expectScaledTransform({-20.0, -10.0, 20.0, 10.0}, mk("center", "middle"));
    expectScaledTransform({-20.0, -20.0, 20.0, 0.0}, mk("center", "top"));
    expectScaledTransform({-20.0, 0.0, 20.0, 20.0}, mk("center", "bottom"));
    expectScaledTransform({-40.0, -20.0, 0.0, 0.0}, mk("right", "top"));
    expectScaledTransform({-40.0, 0.0, 0.0, 20.0}, mk("right", "bottom"));
    expectScaledTransform({-40.0, -10.0, 0.0, 10.0}, mk("right", "middle"));
}

// it("should rotate, then scale, then apply translation")
TEST(TextAnnotation_ComputeTransform, RotateThenScaleThenTranslate)
{
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"top", "left"};
        o.hasOrigin = true;
        o.originX = -5.0;
        o.originY = 20.0;
        expectScaledTransform({-5.0, 0.0, 35.0, 20.0}, o);  // scale then translate
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"top", "left"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        expectScaledTransform({0.0, 0.0, 20.0, 40.0}, o);  // rotation then scale
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"top", "left"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = -5.0;
        o.originY = 20.0;
        expectScaledTransform({-5.0, 20.0, 15.0, 60.0}, o);  // rotation, scale, then translate
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"middle", "center"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = -5.0;
        o.originY = 10.0;
        expectScaledTransform({-15.0, -10.0, 5.0, 30.0}, o);
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"bottom", "left"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = -1.0;
        o.originY = 2.0;
        expectScaledTransform({-21.0, 2.0, -1.0, 42.0}, o);
    }
    {
        TransformRangeOptions o;
        o.hasAnchor = true;
        o.anchor = {"top", "right"};
        o.hasRotation = true;
        o.rotationDegrees = 90.0;
        o.hasOrigin = true;
        o.originX = 0.0;
        o.originY = 100.0;
        expectScaledTransform({0.0, 60.0, 20.0, 100.0}, o);
    }
}

// describe("leaders")
TEST(TextAnnotation_Leaders, UndefinedWhenNoLeadersSet)
{
    TextAnnotationProps props;
    auto annotation = TextAnnotation::fromJSON(&props);
    EXPECT_FALSE(annotation.leaders.has_value());
}

TEST(TextAnnotation_Leaders, ReturnsLeadersWhenSet)
{
    TextAnnotationProps props;
    TextAnnotationLeader leader;
    leader.startPoint = dqGeom::Point3d::FromZero();
    leader.attachment = LeaderAttachment{};  // { mode: "Nearest" } 缺省
    props.leaders.push_back(leader);
    auto annotation = TextAnnotation::fromJSON(&props);
    ASSERT_TRUE(annotation.leaders.has_value());
    ASSERT_EQ(annotation.leaders->size(), 1u);
    EXPECT_TRUE((*annotation.leaders)[0] == leader);
}

}  // namespace
