// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom tests — ClipPrimitive / ClipShape
//
// Ported from: itwinjs-core core/geometry/src/test/clipping/ClipPrimitives.test.ts
//              describe("ClipPrimitive")（可移植子集）
//
// 排除登记（参考有、依赖未移植——见 ClipPrimitive.h 头注 TODO）：
//  - "ClipShapePointTests" / "ClipVectorWithHole"（mask 形状解析——parsePolygonPlanes
//    TODO：AlternatingCCTreeNode/Triangulator）
//  - "ClipShape plane parsing for simple concave polygon" / "simple concave area
//    clips simple polygon" / "complex concave area clips polygon" / "NonConvexClipShapeClipPolygon"
//    （凹多边形——同上；后者另需 PolylineOps.addClosurePoint/sumAreaXY）
//  - "ClipPrimitiveMasking"（PolyfaceClip.clipPolyfaceUnionOfConvexClipPlaneSetsToBuilders 未移植）
// 场景与断言值 1:1 取自参考；仅按 §5(e) 映射 Checker → EXPECT。
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

#include <vector>

namespace dqGeom {
namespace {

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

// Ported from: ClipPrimitives.test.ts:95-102 (clipPrimitivesAreEqual)
bool clipPrimitivesAreEqual(ClipPrimitive const& clip0, ClipPrimitive const& clip1) {
    ClipShape const* shape0 = clip0.asClipShape();
    ClipShape const* shape1 = clip1.asClipShape();
    if (shape0 != nullptr && shape1 != nullptr) {
        // clipShapesAreEqual（ClipPrimitives.test.ts:76-93；按参考只比平面集即可判等此处用法）
        return clipPlaneSetsAreEqual(shape0->fetchClipPlanesRef(), shape1->fetchClipPlanesRef())
            && shape0->invisible() == shape1->invisible();
    }
    if (clipPlaneSetsAreEqual(clip0.fetchClipPlanesRef(), clip1.fetchClipPlanesRef()))
        return true;
    return false;
}

}  // namespace

// Ported from: ClipPrimitives.test.ts:362-407 ("GetRange")
TEST(ClipPrimitiveTest, GetRange) {
    ClipShape::Ptr clipPrimitive = ClipShape::createEmpty();
    int const numIterations = 10;
    int const scaleFactor = 6;
    for (int i = 0; i < numIterations; ++i) {
        int const p = i * scaleFactor;

        // Test with positive box
        ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createXYBox(p, p, p + 1, p + 1);
        convexSet.addZClipPlanes(false, static_cast<double>(p), static_cast<double>(p + 1));
        clipPrimitive = ClipShape::createBlock(Range3d::CreateXYZXYZ(p, p, p, p + 1, p + 1, p + 1),
                                                ClipMaskXYZRangePlanes::All, false, false, nullptr);
        ASSERT_TRUE(clipPrimitive);
        EXPECT_FALSE(clipPrimitive->arePlanesDefined());
        clipPrimitive->fetchClipPlanesRef();
        EXPECT_TRUE(clipPrimitive->arePlanesDefined());
        Range3d convexSetRange = Range3d::CreateNull();
        convexSet.computePlanePlanePlaneIntersections(nullptr, &convexSetRange);

        EXPECT_TRUE(clipPrimitive->isValidPolygon());

        // Test with negative box
        convexSet = ConvexClipPlaneSet::createXYBox(-p - 1, -p - 1, -p, -p);
        convexSet.addZClipPlanes(false, static_cast<double>(p), static_cast<double>(p + 1));
        clipPrimitive->setPolygon({Point3d::From(-p - 1, -p - 1), Point3d::From(-p - 1, -p),
                                   Point3d::From(-p, -p), Point3d::From(-p, -p - 1)});
        convexSetRange = Range3d::CreateNull();
        convexSet.computePlanePlanePlaneIntersections(nullptr, &convexSetRange);
    }

    // Exercise check for z-clips
    EXPECT_TRUE(clipPrimitive->containsZClip());  // normal along the z-axis expected
    ClipShape::Ptr const openShape = ClipShape::createShape(
        {Point3d::From(1, 2), Point3d::From(50, 50), Point3d::From(100, -1)});
    ASSERT_TRUE(openShape);
    EXPECT_FALSE(openShape->containsZClip());  // open top and bottom → no z-clip

    // Exercise invisibility switch
    for (bool a : {false, true}) {
        clipPrimitive->setInvisible(a);
        EXPECT_EQ(clipPrimitive->invisible(), a);
    }
}

// Ported from: ClipPrimitives.test.ts:409-433 ("Transformations")
TEST(ClipPrimitiveTest, Transformations) {
    Point3d originalPoint = Point3d::From(5.7865, 1.24123, 0.000009);
    Point3d testPoint = originalPoint;

    // Created with identity transform - should create no changes
    Transform const identityTransform = Transform::CreateIdentity();
    ClipShape::Ptr clipShape = ClipShape::createEmpty(false, false, &identityTransform);
    clipShape->performTransformFromClip(testPoint);
    EXPECT_TRUE(testPoint.AlmostEqual(originalPoint, 1.0e-10))
        << "Point unchanged with identity transformFromClip";
    clipShape->performTransformToClip(testPoint);
    EXPECT_TRUE(testPoint.AlmostEqual(originalPoint, 1.0e-10))
        << "Point unchanged with identity transformToClip";

    // Created with translation - should translate
    originalPoint = Point3d::From(2, 5, -7);
    testPoint = originalPoint;
    Vector3d const translation = Vector3d::From(0, -1, 1);
    Transform const translationTransform = Transform::CreateTranslation(translation);
    clipShape = ClipShape::createEmpty(false, false, &translationTransform);
    clipShape->performTransformFromClip(testPoint);
    Point3d const expectedTranslated = Point3d::From(originalPoint.x + translation.x,
                                                     originalPoint.y + translation.y,
                                                     originalPoint.z + translation.z);
    EXPECT_TRUE(testPoint.AlmostEqual(expectedTranslated, 1.0e-10))
        << "Point translated by transformFromClip";
    clipShape->performTransformToClip(testPoint);
    EXPECT_TRUE(testPoint.AlmostEqual(originalPoint, 1.0e-10))
        << "Point translated back by transformToClip";
}

// Ported from: ClipPrimitives.test.ts:435-449
// ("ClipShape creation (linear) and point classification")
TEST(ClipPrimitiveTest, ClipShapeCreationLinearAndPointClassification) {
    // Create a ClipShape from 3 colinear points (degenerate!)
    ClipShape::Ptr const clipShape = ClipShape::createShape(
        {Point3d::From(-5, 0, 0), Point3d::From(5, 0, 0), Point3d::From(-5, 0, 0)}, -3.0, 3.0,
        nullptr, false, false);
    ASSERT_TRUE(clipShape);  // can create linear plane set ClipShape
    clipShape->fetchClipPlanesRef();
    EXPECT_EQ(clipShape->classifyPointContainment({Point3d::From(-5.00001, 0, 0)}, true),
              ClipPlaneContainment::StronglyOutside);  // outside the sides of the line
    EXPECT_EQ(clipShape->classifyPointContainment({Point3d::From(0, 3, 0), Point3d::From(2, -5, 0)}, true),
              ClipPlaneContainment::Ambiguous);  // points cross line within sides
    EXPECT_EQ(clipShape->classifyPointContainment({Point3d::From(0, -0.00001, 0), Point3d::From(0, 0.00001, 0)}, true),
              ClipPlaneContainment::Ambiguous);  // points cross line within sides
    EXPECT_EQ(clipShape->classifyPointContainment({Point3d::From(4.999, 0, 2.999), Point3d::From(0, 0, 0)}, true),
              ClipPlaneContainment::StronglyInside);  // on line and within sides
}

// Ported from: ClipPrimitives.test.ts:615-635 ("ClipPrimitive base class")
TEST(ClipPrimitiveTest, ClipPrimitiveBaseClass) {
    bool const invert = false;  // EDL sept 2021: invert bit has no effect — don't test true
    ConvexClipPlaneSet const clipper = ConvexClipPlaneSet::createXYBox(1, 1, 10, 8);
    ClipPrimitive::Ptr const prim0 = ClipPrimitive::createCapture(clipper, invert);
    ClipPrimitive::Ptr const prim1 = prim0->clone();
    ClipPrimitiveProps const json2 = prim0->toJSON();
    ClipPrimitive::Ptr const prim2 = ClipPrimitive::fromJSON(&json2);
    ASSERT_TRUE(prim2);
    EXPECT_TRUE(clipPrimitivesAreEqual(*prim0, *prim2));  // JSON round trip

    for (ClipPrimitive::Ptr const& prim : {prim0, prim1}) {
        EXPECT_EQ(!invert, prim->pointInside(Point3d::From(7, 2, 0)));
        EXPECT_EQ(invert, prim->pointInside(Point3d::From(-2, 0, 0)));
    }
}

// Ported from: ClipPrimitives.test.ts:672-725 ("jsonFragment")
TEST(ClipPrimitiveTest, JsonFragment) {
    // Wire fragment from the reference test (planes.clips JSON form).
    auto makePlaneProps = [](double dist, double nx, double ny, double nz, bool invis = false,
                             bool intr = false) {
        ClipPlaneProps props;
        props.normal = Vector3d::From(nx, ny, nz);
        props.dist = dist;
        if (invis)
            props.invisible = true;
        if (intr)
            props.interior = true;
        return props;
    };
    ConvexClipPlaneSetProps const planeSetProps = {
        makePlaneProps(-0.09250245365197406, 0, 6.123233995736765e-17, 0.9999999999999999),
        makePlaneProps(-4.284169242532198, 0, -6.123233995736765e-17, -0.9999999999999999),
        makePlaneProps(-0.09250245365197413, 0.9999999999999999, 0, 0),
        makePlaneProps(-4.620474647250288, -0.9999999999999999, 0, 0),
        makePlaneProps(-6.984123210872675, 0, -0.9999999999999999, 6.123233995736765e-17),
        makePlaneProps(-0.09250245365197496, 0, 0.9999999999999999, -6.123233995736765e-17),
    };
    ClipVectorProps json;
    ClipPrimitiveProps& primProps = json.emplace_back();
    ClipPrimitivePlanesPart& planesPart = primProps.planes.emplace();
    planesPart.clips = UnionOfConvexClipPlaneSetsProps{planeSetProps};

    ClipVector::Ptr const clipper = ClipVector::fromJSON(&json);
    ASSERT_TRUE(clipper);
    double const q = 10.0;  // big enough that adding/subtracting moves any inside point outside
    for (double x : {0.0, 4.0}) {
        for (double y : {0.0, 6.0}) {
            for (double z : {0.0, 4.0}) {
                EXPECT_TRUE(clipper->pointInside(Point3d::From(x, y, z)));
                EXPECT_FALSE(clipper->pointInside(Point3d::From(x + q, y, z)));
                EXPECT_FALSE(clipper->pointInside(Point3d::From(x - q, y, z)));
                EXPECT_FALSE(clipper->pointInside(Point3d::From(x, y + q, z)));
                EXPECT_FALSE(clipper->pointInside(Point3d::From(x, y - q, z)));
                EXPECT_FALSE(clipper->pointInside(Point3d::From(x, y, z + q)));
                EXPECT_FALSE(clipper->pointInside(Point3d::From(x, y, z - q)));
            }
        }
    }

    for (ClipPrimitive::Ptr const& p : clipper->clips()) {
        UnionOfConvexClipPlaneSets const* convexSets = p->fetchClipPlanesRef();
        ASSERT_NE(convexSets, nullptr);
        for (ConvexClipPlaneSet const& cs : convexSets->convexSets()) {
            Range3d r = Range3d::CreateNull();
            std::vector<Point3d> points;
            cs.computePlanePlanePlaneIntersections(&points, &r);
            for (Point3d const& xyz : points)
                EXPECT_TRUE(cs.isPointOnOrInside(xyz, 0.001));
        }
    }
}

}  // namespace dqGeom
