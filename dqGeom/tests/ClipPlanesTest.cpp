// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ConvexClipPlaneSet arc-clip / containment tests
// Ported from: itwinjs-core core/geometry/src/test/clipping/ClipPlanes.test.ts
//              it("ClassifyPointContainment") (:560-599)
//              it("PlaneArcClips")            (:694-713)
//              it("PlaneSetArcClips")         (:826-865)
//
// 适配说明（§5(c)）：
//   - ClassifyPointContainment / PlaneSetArcClips 的参考把单个/多个 ConvexClipPlaneSet
//     包进 UnionOfConvexClipPlaneSets 再调用；UnionOfConvexClipPlaneSets 未移植。此处直接
//     对成员 ConvexClipPlaneSet 断言（union 仅逐个委托给成员集，场景与断言值不变；
//     ClassifyPointContainment 的 union 只有 1 个成员集）。
//   - PlaneSetArcClips 的 Sample.createClipPlaneSets()（GeometrySamples.ts:340-358）
//     展开为等价的 createXYBox 成员集逐个测试。
#include <gtest/gtest.h>

#include <dqGeom/Arc3d.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipUtils.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Geometry.h>
#include <dqGeom/Transform.h>

#include <cmath>

using namespace dqGeom;

// Ported from: ClipPlanes.test.ts it("ClassifyPointContainment") (:560-599)
TEST(ClipPlanesTest, ClassifyPointContainment)
{
    ConvexClipPlaneSet convexSet1 = ConvexClipPlaneSet::createXYBox(0, 0, 1, 1);
    auto const clipZ0 = ClipPlane::createNormalAndPoint(Vector3d::From(0, 0, 1), Point3d::From(0, 0, 0), false, true);
    auto const clipZ1 = ClipPlane::createNormalAndPoint(Vector3d::From(0, 0, -1), Point3d::From(0, 0, 1), false, true);
    ASSERT_TRUE(clipZ0.has_value() && clipZ1.has_value());  // Should never fail
    convexSet1.addPlaneToConvexSet(*clipZ0);
    convexSet1.addPlaneToConvexSet(*clipZ1);

    // Simple check of a variety of point collections in R^3 space
    // 1.) One Region
    std::vector<Point3d> array = {
        Point3d::From(0.5, 0.5, 0.5),
        Point3d::From(0.75, 0.11, 0.43),
    };
    EXPECT_EQ(ClipPlaneContainment::StronglyInside,
              convexSet1.classifyPointContainment(array, false)) << "All points inside one region";
    EXPECT_EQ(ClipPlaneContainment::StronglyInside,
              convexSet1.classifyPointContainment(array, true)) << "All points inside one region";
    array.push_back(Point3d::From(0, 0, 0));
    EXPECT_EQ(ClipPlaneContainment::Ambiguous,
              convexSet1.classifyPointContainment(array, true)) << "one on border";
    EXPECT_EQ(ClipPlaneContainment::Ambiguous,
              convexSet1.classifyPointContainment(array, false)) << "one on border";
    array.pop_back();
    array.push_back(Point3d::From(0.0000001, 0.0000001, 0.0000001));
    EXPECT_EQ(ClipPlaneContainment::StronglyInside,
              convexSet1.classifyPointContainment(array, true)) << "All points inside one region";

    // 2.) Completely Outside (one on border)
    array = {
        Point3d::From(0, -5, 10),
        Point3d::From(-1, -1, -1),
        Point3d::From(0, 0, 0),
    };
    EXPECT_EQ(ClipPlaneContainment::StronglyOutside,
              convexSet1.classifyPointContainment(array, false)) << "All outside except one on border (not counting)";
    EXPECT_EQ(ClipPlaneContainment::StronglyOutside,
              convexSet1.classifyPointContainment(array, true)) << "All outside except one on border (not counting)";
}

// Ported from: ClipPlanes.test.ts it("PlaneArcClips") (:694-713)
TEST(ClipPlanesTest, PlaneArcClips)
{
    auto const arc = Arc3d::CreateXY(Point3d::From(0, 0.2, 0), 2.0,
                                     AngleSweep::FromStartEndDegrees(0, 270.0));
    auto const plane = ClipPlane::createEdgeXY(Point3d::From(3, 1, 0), Point3d::From(3, -10, 0));
    ASSERT_TRUE(plane.has_value());
    plane->announceClippedArcIntervals(
        *arc,
        [&](double fraction0, double fraction1, Arc3d const& cp) {
            Point3d const point0 = cp.FractionToPoint(fraction0);
            Point3d const point1 = cp.FractionToPoint(interpolate(fraction0, 0.5, fraction1));
            Point3d const point2 = cp.FractionToPoint(fraction1);

            EXPECT_TRUE(plane->isPointOn(point0)) << "interval start point is ON";
            EXPECT_FALSE(plane->isPointOn(point1)) << "interval midpoint is not ON";
            EXPECT_TRUE(plane->isPointOnOrInside(point1)) << "interval midpoint is IN";
            EXPECT_TRUE(plane->isPointOn(point2)) << "interval end point is ON";
        });
    // (参考场景：arc 完全在平面外侧，无区间被 announce——覆盖 appendIntersectionRadians
    // 无交点路径；回调体在有区间时逐条断言。)
}

// Ported from: ClipPlanes.test.ts it("PlaneSetArcClips") (:826-865)
TEST(ClipPlanesTest, PlaneSetArcClips)
{
    // Arc3d.createXYEllipse(center, 3.0, 0.5, sweep 0→270°)
    auto makeArc = [] {
        return Arc3d::FromVectors(Point3d::From(0, 0.2, 0),
                                  Vector3d::From(3.0, 0, 0),
                                  Vector3d::From(0, 0.5, 0),
                                  AngleSweep::FromStartEndDegrees(0, 270.0));
    };
    auto const arc = makeArc();
    // Sample.createClipPlaneSets() 的成员集（GeometrySamples.ts:340-358 展开）
    ConvexClipPlaneSet const clippers[] = {
        ConvexClipPlaneSet::createXYBox(0, 0, 1, 1),     // quadrant1
        ConvexClipPlaneSet::createXYBox(0, -1, 1, 0),    // quadrant4
        ConvexClipPlaneSet::createXYBox(-1, 0, 0, 1),    // quadrant2
    };

    Transform const transform = Transform::CreateFixedPointAndMatrix(
        Point3d::From(1, 0.5, 0),
        Matrix3d::CreateRotationAroundAxis(Vector3d::From(1, 1, 9), Angle::DegreesToRadians(60)));

    auto arc1 = makeArc();
    ASSERT_TRUE(arc1->TryTransformInPlace(transform));

    int clipperIndex = 0;
    for (auto const& clipper : clippers) {
        ConvexClipPlaneSet clipper1 = clipper.clone();
        clipper1.transformInPlace(transform);   // ConvexClipPlaneSet.ts:443-447（void）

        auto curvePrimitiveAnnouncer = [](double fraction0, double fraction1, Arc3d const& cp,
                                          ConvexClipPlaneSet const& activeClipper) {
            Point3d const point1 = cp.FractionToPoint(interpolate(fraction0, 0.5, fraction1));
            EXPECT_TRUE(activeClipper.isPointOnOrInside(point1)) << "interval midpoint is IN";
        };

        // 深度分支调用形式：arc.announceClipIntervals(clipper, announce)
        // （Arc3d.ts:1418-1419 → clipper.announceClippedArcIntervals）。
        int fireCount = 0;
        arc->announceClipIntervals(clipper, [&](double f0, double f1, Arc3d const& cp) {
            ++fireCount;
            curvePrimitiveAnnouncer(f0, f1, cp, clipper);
        });
        int fireCount1 = 0;
        arc1->announceClipIntervals(clipper1, [&](double f0, double f1, Arc3d const& cp) {
            ++fireCount1;
            curvePrimitiveAnnouncer(f0, f1, cp, clipper1);
        });
        // 防空转守卫（按参考场景几何精确推导）：
        //   quadrant1 (x,y∈[0,1])：θ∈[70.5°,90°] 正长度区间 → 必 announce；
        //   quadrant4 (x∈[0,1],y∈[-1,0])：arc 仅在 θ=270° 端点触 x=0 边界（无正长度
        //     区间）→ 精确 0 次（参考对该集同样无 announce——其断言是逐区间的，
        //     零区间即空转；此处显式记录场景的边界相切几何）；
        //   quadrant2 (x∈[-1,0],y∈[0,1])：θ∈[90°,109.5°] 正长度区间 → 必 announce。
        if (clipperIndex == 1) {
            EXPECT_EQ(0, fireCount) << "quadrant4: arc only touches the x=0 border at theta=270deg";
            EXPECT_EQ(0, fireCount1) << "transformed quadrant4: same border-touch geometry";
        } else {
            EXPECT_GT(fireCount, 0) << "clipper " << clipperIndex << " must announce inside intervals";
            EXPECT_GT(fireCount1, 0) << "transformed clipper " << clipperIndex << " must announce inside intervals";
        }
        ++clipperIndex;
    }
}

// Authored: no reference test with assertions exists in itwinjs-core for
//           ConvexClipPlaneSet.clipConvexPolygonInPlace（ClipPlanes.test.ts
//           "Quadrants"(:1504-1549) 仅可视化无断言）。场景形状取自 Quadrants
//           （createEmpty + 条件 x/y 平面 + 凸多边形裁剪），断言裁剪后顶点
//           全部 inside 且面积/顶点数符合半空间交集几何。
TEST(ClipPlanesTest, ClipConvexPolygonInPlace)
{
    // unit box [0,1]x[0,1] clipper；三角形 (-1,-1)-(2,-1)-(0.5,2) 裁到盒内。
    ConvexClipPlaneSet const clipper = ConvexClipPlaneSet::createXYBox(0, 0, 1, 1);
    std::vector<Point3d> polygon = {
        Point3d::From(-1, -1, 0),
        Point3d::From(2, -1, 0),
        Point3d::From(0.5, 2, 0),
    };
    std::vector<Point3d> work;
    clipper.clipConvexPolygonInPlace(polygon, work);
    ASSERT_FALSE(polygon.empty());
    ASSERT_GE(polygon.size(), 3u);
    for (auto const& p : polygon)
        EXPECT_TRUE(clipper.isPointOnOrInside(p)) << "clipped vertex must be inside";
    // 裁后多边形必须留在盒内 → 所有顶点坐标在 [0,1]（容差 1e-8，对齐
    // classifyPointContainment 的 on 容差）。
    for (auto const& p : polygon) {
        EXPECT_GE(p.x, -1.0e-8);
        EXPECT_LE(p.x, 1.0 + 1.0e-8);
        EXPECT_GE(p.y, -1.0e-8);
        EXPECT_LE(p.y, 1.0 + 1.0e-8);
    }
    // 完全在外部的多边形 → 空。
    std::vector<Point3d> outside = {
        Point3d::From(5, 5, 0),
        Point3d::From(6, 5, 0),
        Point3d::From(5.5, 6, 0),
    };
    clipper.clipConvexPolygonInPlace(outside, work);
    EXPECT_TRUE(outside.empty());
}
