// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom tests — ClipUtilities 剖切轮廓/范围族
//
// Ported from: itwinjs-core core/geometry/src/test/clipping/ClipUtilities.test.ts
//              describe("ClipUtilities") / exerciseClipper（可移植断言子集）
//
// 排除登记（参考有、依赖未移植）：
//  - createComplementaryClips 及其外环断言（ClipUtils.ts:1092-1116——依赖
//    CurveFactory.planePlaneIntersectionRay / Ray3d 面）
//  - 视觉几何采集（GeometryCoreTestIO.saveGeometry——参考测试的目检面，无数值断言）
// 场景与断言值 1:1 取自参考；仅按 §5(e) 映射 Checker → EXPECT。
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipUtilsLoops.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/UnionOfConvexClipPlaneSets.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

#include <vector>

namespace dqGeom {
namespace {

// Range of an array of geometry (loops) — reference helper rangeOfGeometry
// (ClipUtilities.test.ts:96-100).
Range3d rangeOfLoops(std::vector<dqBase::RefPtr<Loop>> const& loops) {
    Range3d range = Range3d::CreateNull();
    for (dqBase::RefPtr<Loop> const& g : loops)
        g->ExtendRange(range);
    return range;
}

bool rangesAlmostEqual(Range3d const& a, Range3d const& b) {
    auto pointNear = [](Point3d const& p, Point3d const& q) {
        return p.AlmostEqual(q, 1.0e-10);
    };
    return (a.isNull() && b.isNull())
        || (pointNear(a.low, b.low) && pointNear(a.high, b.high));
}

}  // namespace

// Ported from: ClipUtilities.test.ts:95-148 (exerciseClipper 数值断言子集) —
// ConvexClipPlaneSetComplement 用例的 clipper/rangeA 场景（:154-159）。
TEST(ClipUtilsLoopsTest, LoopsRangeAndIntersectFamily) {
    Range3d const outerRange = Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1);
    double const a = 0.2;
    double const b = 0.6;
    double const c = 0.4;
    Range3d const rangeA = Range3d::CreateXYZXYZ(a, a, a, b, b, c);
    ConvexClipPlaneSet const clipper = ConvexClipPlaneSet::createRange3dPlanes(rangeA);

    // clipperLoops / rangeLoops（includeConvexSetFaces / includeRangeFaces 两侧）
    std::vector<dqBase::RefPtr<Loop>> const clipperLoops =
        ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(clipper, outerRange, true, false, true);
    std::vector<dqBase::RefPtr<Loop>> const rangeLoops =
        ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(clipper, outerRange, false, true, true);

    // range100 == range101（参考 :115-118：rangeOfClipperIntersectionWithRange ==
    // loops 合并 range）——clipper 深埋于 ClipPrimitive / ClipVector 内同样成立。
    Range3d const range100 = ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(clipper, outerRange);
    Range3d range101 = rangeOfLoops(clipperLoops);
    range101.ExtendRange(rangeOfLoops(rangeLoops));
    EXPECT_TRUE(rangesAlmostEqual(range100, range101));

    EXPECT_TRUE(ClipUtilitiesLoops::doesConvexClipPlaneSetIntersectRange(clipper, outerRange));

    // clipper buried in ClipPrimitive（参考 :135-139）
    ClipPrimitive::Ptr const innerPrimitive = ClipPrimitive::createCapture(clipper);
    Range3d const range103 =
        ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(*innerPrimitive, outerRange, false);
    EXPECT_TRUE(rangesAlmostEqual(range103, range100));
    EXPECT_TRUE(ClipUtilitiesLoops::doesClipperIntersectRange(*innerPrimitive, outerRange));

    // clipper buried in ClipVector（参考 :141-144）
    ClipVector::Ptr const innerVector =
        ClipVector::create(std::vector<ClipPrimitive::Ptr>{innerPrimitive});
    Range3d const range104 =
        ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(*innerVector, outerRange, false);
    EXPECT_TRUE(rangesAlmostEqual(range104, range100));
    EXPECT_TRUE(ClipUtilitiesLoops::doesClipperIntersectRange(*innerVector, outerRange));

    // UnionOf 分派（参考 :510-517 union 分支——range 为成员 convex 集 range 的并）
    UnionOfConvexClipPlaneSets const unionOf =
        UnionOfConvexClipPlaneSets::createConvexSets({clipper});
    Range3d const rangeUnion = ClipUtilitiesLoops::rangeOfClipperIntersectionWithRange(unionOf, outerRange);
    EXPECT_TRUE(rangesAlmostEqual(rangeUnion, range100));
    EXPECT_TRUE(ClipUtilitiesLoops::doesClipperIntersectRange(unionOf, outerRange));

    // 相离的裁剪集 → 无相交（does 族 false；loops 空）
    ConvexClipPlaneSet const disjoint =
        ConvexClipPlaneSet::createRange3dPlanes(Range3d::CreateXYZXYZ(5, 5, 5, 6, 6, 6));
    EXPECT_FALSE(ClipUtilitiesLoops::doesClipperIntersectRange(disjoint, outerRange));
    EXPECT_TRUE(
        ClipUtilitiesLoops::loopsOfConvexClipPlaneIntersectionWithRange(disjoint, outerRange).empty());
}

}  // namespace dqGeom
