// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom tests — ClipVector
//
// Ported from: itwinjs-core core/geometry/src/test/clipping/ClipVector.test.ts
//              describe("ClipVector") / describe("StringifiedClipVector")（可移植子集）
//
// 排除登记（参考有、依赖未移植——见 ClipVector.h / ClipPrimitive.h 头注 TODO）：
//  - "Transformations and matrix multiplication" 的 multiplyPlanesByMatrix4d（Matrix4d）
//  - OuterAndMask / OuterAndMaskLargeCoordinateAndTransform / ClipperInterfaces
//    （mask 洞 + 凹多边形——parsePolygonPlanes TODO：AlternatingCCTreeNode/Triangulator）
// 场景与断言值 1:1 取自参考；仅按 §5(e) 映射 Checker → EXPECT。
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

#include <limits>
#include <vector>

namespace dqGeom {
namespace {

// EXPENSIVE -- Returns true if two convex sets are equal, allowing reordering.
// Ported from: ClipPrimitives.test.ts:35-51 (convexSetsAreEqual)
bool convexSetsAreEqual(ConvexClipPlaneSet const& convexSet0, ConvexClipPlaneSet const& convexSet1) {
    if (convexSet0.planes.size() != convexSet1.planes.size())
        return false;
    for (ClipPlane const& plane0 : convexSet0.planes) {
        bool foundMatch = false;
        for (ClipPlane const& plane1 : convexSet1.planes) {
            if (plane0.isAlmostEqual(plane1)) {
                foundMatch = true;
                break;
            }
        }
        if (!foundMatch)
            return false;
    }
    return true;
}

// Ported from: ClipPrimitives.test.ts:53-74 (clipPlaneSetsAreEqual)
bool clipPlaneSetsAreEqual(UnionOfConvexClipPlaneSets const* set0, UnionOfConvexClipPlaneSets const* set1) {
    if (set0 == nullptr && set1 == nullptr)
        return true;
    if (set0 == nullptr || set1 == nullptr)
        return false;
    if (set0->convexSets().size() != set1->convexSets().size())
        return false;
    for (ConvexClipPlaneSet const& convexSet0 : set0->convexSets()) {
        bool foundMatch = false;
        for (ConvexClipPlaneSet const& convexSet1 : set1->convexSets()) {
            if (convexSetsAreEqual(convexSet0, convexSet1)) {
                foundMatch = true;
                break;
            }
        }
        if (!foundMatch)
            return false;
    }
    return true;
}

// Ported from: ClipPrimitives.test.ts:76-93 (clipShapesAreEqual)
bool clipShapesAreEqual(ClipShape const& clip0, ClipShape const& clip1) {
    if (!clipPlaneSetsAreEqual(clip0.fetchClipPlanesRef(), clip1.fetchClipPlanesRef()))
        return false;
    if (clip0.invisible() != clip1.invisible())
        return false;
    if (clip0.polygon().size() != clip1.polygon().size())
        return false;
    for (size_t i = 0; i < clip0.polygon().size(); ++i)  // points in the same order
        if (!clip0.polygon()[i].AlmostEqual(clip1.polygon()[i]))
            return false;
    if (clip0.isMask() != clip1.isMask())
        return false;
    if ((clip0.zLowValid() != clip1.zLowValid()) || (clip0.zHighValid() != clip1.zHighValid())
        || (clip0.transformValid() != clip1.transformValid()))
        return false;
    if ((clip0.zLow() != clip1.zLow()) || (clip0.zHigh() != clip1.zHigh()))
        return false;
    if (clip0.transformValid()
        && (!clip0.transformFromClip()->IsAlmostEqual(*clip1.transformFromClip())
            || !clip1.transformToClip()->IsAlmostEqual(*clip1.transformToClip())))
        return false;
    return true;
}

// Ported from: ClipPrimitives.test.ts:95-102 (clipPrimitivesAreEqual)
bool clipPrimitivesAreEqual(ClipPrimitive const& clip0, ClipPrimitive const& clip1) {
    ClipShape const* shape0 = clip0.asClipShape();
    ClipShape const* shape1 = clip1.asClipShape();
    if (shape0 != nullptr && shape1 != nullptr)
        return clipShapesAreEqual(*shape0, *shape1);
    if (clipPlaneSetsAreEqual(clip0.fetchClipPlanesRef(), clip1.fetchClipPlanesRef()))
        return true;
    return false;
}

// EXPENSIVE -- Tests whether two ClipVectors are equivalent.
// Ported from: ClipVector.test.ts:90-98 (clipVectorsAreEqual)
bool clipVectorsAreEqual(ClipVector const& vector0, ClipVector const& vector1) {
    if (vector0.clips().size() != vector1.clips().size())
        return false;
    for (size_t i = 0; i < vector0.clips().size(); ++i)
        if (!clipPrimitivesAreEqual(*vector0.clips()[i], *vector1.clips()[i]))
            return false;
    return true;
}

// Fixture state from the reference beforeAll (ClipVector.test.ts:126-149) —
// all shapes are convex (block/triangle/convex pentagon/rectangle/triangle).
struct ClipVectorFixture {
    ClipShape::Ptr clipShape0 = ClipShape::createBlock(
        Range3d::CreateXYZXYZ(-5, -4, -50, -3, -2, 50), ClipMaskXYZRangePlanes::All);
    ClipShape::Ptr clipShape1 = ClipShape::createShape(
        {Point3d::From(4.5, 1), Point3d::From(6, 3), Point3d::From(3, 3)});
    ClipShape::Ptr clipShape2 = ClipShape::createShape(
        {Point3d::From(6, 1), Point3d::From(8, 1), Point3d::From(10, -3), Point3d::From(10, -5),
         Point3d::From(6, -5)},
        -0.2, -0.1);
    ClipShape::Ptr clipShape3 = ClipShape::createShape(
        {Point3d::From(2, -7), Point3d::From(6.5, -7), Point3d::From(6.5, -4), Point3d::From(3, -4)},
        -5.0, 5.0);
    ClipShape::Ptr clipShape4 = ClipShape::createShape(
        {Point3d::From(7, -8), Point3d::From(7.7, -4.5), Point3d::From(6.3, -4.5)}, -5.0, 5.0);

    ClipVector::Ptr clipVector012 =
        ClipVector::createCapture({clipShape0, clipShape1, clipShape2});
};

// Enumerated type for point manipulation at the extremities of a ClipVector's
// ClipShape.
// Ported from: ClipVector.test.ts:30-38 (const enum PointAdjustment)
enum class PointAdjustment { AddX, SubX, AddY, SubY, AddZ, SubZ, None };

// Ported from: ClipVector.test.ts:44-74 (makePointAdjustments)
// §5(e) 适配：参考的 None 分支不赋值（pointInside/pointOutside 保持 undefined，
// checkPointProximity 的 if 守卫跳过）——以 optional 表达同一缺席语义。
std::optional<Point3d> makePointAdjustment(Point3d const& point, PointAdjustment adjustment,
                                           bool wantInside) {
    double const delta = wantInside ? 0.0001 : -0.0001;
    switch (adjustment) {
        case PointAdjustment::AddX:
            return Point3d::From(point.x + delta, point.y, point.z);
        case PointAdjustment::SubX:
            return Point3d::From(point.x - delta, point.y, point.z);
        case PointAdjustment::AddY:
            return Point3d::From(point.x, point.y + delta, point.z);
        case PointAdjustment::SubY:
            return Point3d::From(point.x, point.y - delta, point.z);
        case PointAdjustment::AddZ:
            return Point3d::From(point.x, point.y, point.z + delta);
        case PointAdjustment::SubZ:
            return Point3d::From(point.x, point.y, point.z - delta);
        case PointAdjustment::None:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace

// Ported from: ClipVector.test.ts:151-199
// TEST(Suite, "ClipVector creation and to/from JSON")
TEST(ClipVectorTest, CreationAndToJsonFromJson) {
    ClipVectorFixture f;

    // Parse ClipPlanes from all ClipShapes in a ClipVector (must complete
    // before other tests cause the ClipShapes to cache their sets)
    ClipVector::Ptr const newlyCreatedClipVector = ClipVector::createCapture(
        {f.clipShape0, f.clipShape1, f.clipShape2, f.clipShape3, f.clipShape4});
    for (ClipPrimitive::Ptr const& clip : newlyCreatedClipVector->clips())
        EXPECT_FALSE(clip->arePlanesDefined());
    newlyCreatedClipVector->parseClipPlanes();
    for (ClipPrimitive::Ptr const& clip : newlyCreatedClipVector->clips())
        EXPECT_TRUE(clip->arePlanesDefined());

    // Test create methods and cloning/referencing
    ClipVector::Ptr const clipVectorTester0 = f.clipVector012->clone();
    ClipVector::Ptr clipVectorTester1 = ClipVector::createEmpty();
    EXPECT_FALSE(clipVectorTester1->isValid());  // empty ClipVector is not valid
    clipVectorTester1 = ClipVector::createCapture(clipVectorTester0->clips());
    EXPECT_TRUE(clipVectorTester1->isValid());
    clipVectorTester1 = ClipVector::create(clipVectorTester0->clips());
    EXPECT_TRUE(clipVectorTester1->isValid());
    clipVectorTester1 = ClipVector::createEmpty();
    size_t const arrLen = f.clipVector012->clips().size();
    for (size_t i = 0; i < arrLen; ++i) {
        EXPECT_NE(f.clipVector012->clips()[i].Get(), clipVectorTester0->clips()[i].Get());  // deep copies
        EXPECT_TRUE(clipPrimitivesAreEqual(*f.clipVector012->clips()[i], *clipVectorTester0->clips()[i]));
        clipVectorTester1->appendReference(f.clipVector012->clips()[i]);
        EXPECT_EQ(f.clipVector012->clips()[i].Get(),
                  clipVectorTester1->clips()[0].Get());  // appended by reference
        clipVectorTester1->clear();
        clipVectorTester1->appendClone(*f.clipVector012->clips()[i]);
        EXPECT_NE(f.clipVector012->clips()[i].Get(), clipVectorTester1->clips()[0].Get());
        EXPECT_TRUE(clipPrimitivesAreEqual(*f.clipVector012->clips()[i], *clipVectorTester1->clips()[0]));
        clipVectorTester1->clear();
    }

    // Test appendages to the ClipVector array
    clipVectorTester1->appendShape(f.clipShape2->polygon(), f.clipShape2->zLow(), f.clipShape2->zHigh(),
                                   nullptr, f.clipShape2->isMask(), f.clipShape2->invisible());
    ASSERT_NE(clipVectorTester1->clips()[0]->asClipShape(), nullptr);
    EXPECT_TRUE(clipShapesAreEqual(*f.clipShape2, *clipVectorTester1->clips()[0]->asClipShape()));

    // Test the to/from JSON methods
    ClipVectorProps const clipJSON = f.clipVector012->toJSON();
    ASSERT_EQ(clipJSON.size(), f.clipVector012->clips().size());
    for (ClipPrimitiveProps const& primitive : clipJSON) {
        EXPECT_TRUE(primitive.shape.has_value());
        EXPECT_FALSE(primitive.shape->points.empty());
    }
    ClipVector::Ptr const parsedClipVector = ClipVector::fromJSON(&clipJSON);
    EXPECT_TRUE(clipVectorsAreEqual(*f.clipVector012, *parsedClipVector));
}

// Ported from: ClipVector.test.ts:201-265
// TEST(Suite, "Point proximity and classification")
TEST(ClipVectorTest, PointProximityAndClassification) {
    ClipVectorFixture f;

    std::vector<std::vector<Point3d>> const shapeExtremities = {
        {
            Point3d::From(-5, -3),
            Point3d::From(-3, -2),
            Point3d::From(-4, -3, -50),
            Point3d::From(-4, -3, 50),
        },
        {
            Point3d::From(4.5, 3),
            Point3d::From(3.75, 2),
            Point3d::From(5.25, 2),
            Point3d::From(4.5, 2, 100000),
        },
        {
            Point3d::From(7, 1, -0.15),
            Point3d::From(9, -1, -0.15),
            Point3d::From(10, -4, -0.15),
            Point3d::From(6, -4, -0.15),
            Point3d::From(7, -3, -0.2),
            Point3d::From(7, -3, -0.1),
        },
    };
    std::vector<std::vector<PointAdjustment>> const shapePointAdjustments = {
        {PointAdjustment::AddX, PointAdjustment::SubX, PointAdjustment::AddZ, PointAdjustment::SubZ},
        {PointAdjustment::SubY, PointAdjustment::AddX, PointAdjustment::AddY, PointAdjustment::None},
        {PointAdjustment::SubY, PointAdjustment::SubX, PointAdjustment::SubX, PointAdjustment::AddX,
         PointAdjustment::AddZ, PointAdjustment::SubZ},
    };

    // Ensure that 'LineString' connecting boundaries is considered inside the ClipVector
    for (std::vector<Point3d> const& arr : shapeExtremities) {
        EXPECT_TRUE(f.clipVector012->isAnyLineStringPointInside(arr));
        EXPECT_TRUE(f.clipVector012->isLineStringCompletelyContained(arr));
    }

    // Check whether points are considered inside or outside at the extremities
    // of each contained ClipShape (single-shape ClipVector each round).
    for (size_t i = 0; i < f.clipVector012->clips().size(); ++i) {
        ClipVector::Ptr const clipVectorSingleShape = ClipVector::create({f.clipVector012->clips()[i]});
        for (size_t j = 0; j < shapeExtremities[i].size(); ++j) {
            Point3d const& pointOnEdge = shapeExtremities[i][j];
            // checkPointProximity (ClipVector.test.ts:81-87)：None 调整 → 两点均缺席，
            // if 守卫跳过（1:1 参考）。
            std::optional<Point3d> const pointInside =
                makePointAdjustment(pointOnEdge, shapePointAdjustments[i][j], true);
            std::optional<Point3d> const pointOutside =
                makePointAdjustment(pointOnEdge, shapePointAdjustments[i][j], false);
            EXPECT_TRUE(clipVectorSingleShape->pointInside(pointOnEdge))
                << "Point on ClipShape edge is inside ClipVector";
            if (pointInside.has_value())
                EXPECT_TRUE(clipVectorSingleShape->pointInside(*pointInside))
                    << "Point within ClipShape bounds is inside ClipVector";
            if (pointOutside.has_value())
                EXPECT_TRUE(!clipVectorSingleShape->pointInside(*pointOutside))
                    << "Point outside of ClipShape bounds is outside ClipVector";
            // classifyPointContainment exact numbers (1=StronglyInside, 2=Ambiguous, 3=StronglyOutside)
            EXPECT_EQ(clipVectorSingleShape->classifyPointContainment({pointOnEdge}),
                      ClipPlaneContainment::StronglyInside);  // edge point strongly inside
            if (pointInside.has_value())
                EXPECT_EQ(clipVectorSingleShape->classifyPointContainment({*pointInside}),
                          ClipPlaneContainment::StronglyInside);  // inner point strongly inside
            if (pointOutside.has_value())
                EXPECT_EQ(clipVectorSingleShape->classifyPointContainment({*pointOutside}),
                          ClipPlaneContainment::StronglyOutside);  // outer point strongly outside
            if (pointInside.has_value() && pointOutside.has_value())
                EXPECT_EQ(clipVectorSingleShape->classifyPointContainment({*pointInside, *pointOutside}),
                          ClipPlaneContainment::Ambiguous);  // outer AND inner points → ambiguous
        }
    }

    // pointInside only passes for points within intersecting ClipShapes
    EXPECT_FALSE(f.clipVector012->pointInside(Point3d::From(-4, -3, 0)));
    ClipVector::Ptr const intersectionClipVector = ClipVector::createCapture({
        ClipShape::createShape({Point3d::From(-5, 5), Point3d::From(-5, -5), Point3d::From(0.00001, 0)},
                               -0.00001, 0.00001),
        ClipShape::createShape({Point3d::From(5, 5), Point3d::From(5, -5), Point3d::From(-0.00001, 0)},
                               -0.00001, 0.00001),
    });
    EXPECT_TRUE(intersectionClipVector->pointInside(Point3d::From(0, 0, 0)));
    EXPECT_FALSE(intersectionClipVector->pointInside(Point3d::From(0.00011, 0, 0)));
    EXPECT_FALSE(intersectionClipVector->pointInside(Point3d::From(-0.00011, 0, 0)));
}

// Ported from: ClipVector.test.ts:267-284 ("Transformations and matrix
// matrix multiplication" — identity 子集；multiplyPlanesByMatrix4d 见排除登记)
TEST(ClipVectorTest, TransformInPlaceIdentityHasNoEffect) {
    ClipVectorFixture f;
    Transform const t0 = Transform::CreateIdentity();
    ClipVector::Ptr const clipVectorClone = f.clipVector012->clone();
    EXPECT_TRUE(clipVectorClone->transformInPlace(t0));
    EXPECT_TRUE(clipVectorsAreEqual(*clipVectorClone, *f.clipVector012));
}

// Ported from: ClipVector.test.ts:286-323
// TEST(Suite, "Extract boundary loops")
TEST(ClipVectorTest, ExtractBoundaryLoops) {
    ClipVectorFixture f;
    size_t const vectorLen = f.clipVector012->clips().size();
    ClipShape const* lastShape = f.clipVector012->clips()[vectorLen - 1]->asClipShape();
    ASSERT_NE(lastShape, nullptr);
    uint32_t const expClipMask =
        static_cast<uint32_t>(ClipMaskXYZRangePlanes::XAndY)
        | (lastShape->zLowValid() ? static_cast<uint32_t>(ClipMaskXYZRangePlanes::ZLow) : 0u)
        | (lastShape->zHighValid() ? static_cast<uint32_t>(ClipMaskXYZRangePlanes::ZHigh) : 0u);
    double expZLow = -std::numeric_limits<double>::max();
    double expZHigh = std::numeric_limits<double>::max();
    bool zLowFound = false;
    bool zHighFound = false;
    // Find final mask, zLow, and zHigh of clipVector012
    for (size_t i = vectorLen; i-- > 0;) {
        ClipShape const* shape = f.clipVector012->clips()[i]->asClipShape();
        ASSERT_NE(shape, nullptr);
        if (shape->zLowValid()) {
            zLowFound = true;
            expZLow = *shape->zLow();
        }
        if (shape->zHighValid()) {
            zHighFound = true;
            expZHigh = *shape->zHigh();
        }
        if (zLowFound && zHighFound)
            break;
    }
    std::vector<std::vector<Point3d>> loopPoints;
    std::vector<double> const retVal = f.clipVector012->extractBoundaryLoops(loopPoints);
    ASSERT_EQ(retVal.size(), 3u);
    EXPECT_EQ(static_cast<uint32_t>(retVal[0]), expClipMask);
    EXPECT_DOUBLE_EQ(retVal[1], expZLow);
    EXPECT_DOUBLE_EQ(retVal[2], expZHigh);
    ASSERT_EQ(loopPoints.size(), vectorLen);
    for (size_t loopNum = 0; loopNum < loopPoints.size(); ++loopNum) {
        ClipShape const* shape = f.clipVector012->clips()[loopNum]->asClipShape();
        ASSERT_NE(shape, nullptr);
        ASSERT_EQ(loopPoints[loopNum].size(), shape->polygon().size());
        for (size_t pointNum = 0; pointNum < loopPoints[loopNum].size(); ++pointNum)
            EXPECT_TRUE(loopPoints[loopNum][pointNum].AlmostEqual(shape->polygon()[pointNum]));
    }
}

// Ported from: ClipVector.test.ts:325-354
// TEST(Suite, "converts to compact string representation")
TEST(ClipVectorTest, ToCompactString) {
    ClipVector::Ptr cv = ClipVector::createEmpty();
    EXPECT_EQ(cv->toCompactString(), "_");

    ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createPlanes({});
    ClipPrimitive::Ptr primitive = ClipPrimitive::createCapture(convexSet, false);
    cv = ClipVector::createCapture({primitive});
    EXPECT_EQ(cv->toCompactString(), "0___");
    primitive = ClipPrimitive::createCapture(convexSet, true);
    cv = ClipVector::createCapture({primitive});
    EXPECT_EQ(cv->toCompactString(), "1___");

    auto const plane = ClipPlane::createNormalAndDistance(Vector3d::From(0, 1, 0), -5, true, false);
    ASSERT_TRUE(plane.has_value());
    convexSet = ConvexClipPlaneSet::createPlanes({*plane});
    primitive = ClipPrimitive::createCapture(convexSet);
    cv = ClipVector::createCapture({primitive});
    EXPECT_EQ(cv->toCompactString(), "010_1_0_-5____");

    std::vector<ClipPlane> const planes = {
        *plane, *ClipPlane::createNormalAndDistance(Vector3d::From(0, 0, -1), 0.00000000005, true, true)};
    convexSet = ConvexClipPlaneSet::createPlanes(planes);
    primitive = ClipPrimitive::createCapture(convexSet);
    cv = ClipVector::createCapture({primitive});
    EXPECT_EQ(cv->toCompactString(), "010_1_0_-5_30_0_-1_5e-11____");

    ConvexClipPlaneSet const set2 = ConvexClipPlaneSet::createPlanes(
        {*ClipPlane::createNormalAndDistance(Vector3d::From(1, 0, 0), 4, false, true)});
    cv = ClipVector::createCapture({primitive, ClipPrimitive::createCapture(set2, true)});
    EXPECT_EQ(cv->toCompactString(), "010_1_0_-5_30_0_-1_5e-11___121_0_0_4____");
}

// Ported from: ClipVector.test.ts:357-370 ("creates from ClipVector")
TEST(ClipVectorTest, StringifiedFromClipVector) {
    EXPECT_FALSE(StringifiedClipVector::fromClipVector(nullptr).has_value());
    EXPECT_FALSE(StringifiedClipVector::fromClipVector(ClipVector::createEmpty()).has_value());

    ClipVector::Ptr const cv = ClipVector::createCapture(
        {ClipPrimitive::createCapture(ConvexClipPlaneSet::createPlanes({}), false)});
    auto const scv = StringifiedClipVector::fromClipVector(cv);
    ASSERT_TRUE(scv.has_value());
    EXPECT_EQ(scv->clip.Get(), cv.Get());
    EXPECT_FALSE(scv->clipString.empty());
    EXPECT_EQ(scv->clipString, cv->toCompactString());

    auto const scv2 = StringifiedClipVector::fromClipVector(cv);
    ASSERT_TRUE(scv2.has_value());
    EXPECT_EQ(scv2->clipString, scv->clipString);
}

}  // namespace dqGeom


