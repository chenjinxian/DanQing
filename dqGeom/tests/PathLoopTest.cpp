// SPDX-License-Identifier: Apache-2.0
// dqGeom Path/Loop tests
// Ported from: itwinjs-core core/geometry/src/test/curve/CurveCollection.test.ts
//               it("CyclicIndex") + it("startEndPoint")
//
// 仅移植自包含场景（create/tryAddChild/children/StartPoint/EndPoint/clone/cloneStroked/
// dgnBoundaryType/IsSameGeometryClass）。依赖 *Context / region 布尔的场景（isOpenPath、
// isClosedPath、region 合并）随依赖类型就绪补；cyclicCurvePrimitive / getPackedStrokes 为
// Phase-N，本测试跳过。
#include <gtest/gtest.h>

#include <dqGeom/Arc3d.h>
#include <dqGeom/CurveCollection.h>
#include <dqGeom/LineSegment3d.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point3d.h>

#include <vector>

using namespace dqGeom;

namespace {
constexpr double kTol = 1.0e-9;
void ExpectNear(const Point3d& expected, const std::optional<Point3d>& actual, double tol = kTol) {
    ASSERT_TRUE(actual.has_value());
    EXPECT_NEAR(expected.x, actual->x, tol);
    EXPECT_NEAR(expected.y, actual->y, tol);
    EXPECT_NEAR(expected.z, actual->z, tol);
}
} // namespace

// Ported from: CurveCollection.test.ts it("CyclicIndex") — tryAddChild + child access.
// (cyclicCurvePrimitive is Phase-N; here we verify tryAddChild/getChild/bounds.)
TEST(PathLoopTest, TryAddChildAndChildAccess) {
    auto loop = Loop::Create();
    auto path = Path::Create();
    auto line = LineSegment3d::create(Point3d::From(1, 2, 3), Point3d::From(6, 2, 3));
    auto linestring = LineString3d::create({Point3d::From(6, 2, 3), Point3d::From(5, 3, 9), Point3d::From(1, 2, 3)});

    EXPECT_TRUE(loop->TryAddChild(line));
    EXPECT_TRUE(loop->TryAddChild(linestring));
    EXPECT_TRUE(path->TryAddChild(line));
    EXPECT_TRUE(path->TryAddChild(linestring));
    EXPECT_FALSE(path->TryAddChild(nullptr));

    EXPECT_EQ(2u, loop->Curves().size());
    EXPECT_EQ(2u, path->Curves().size());
    EXPECT_TRUE(path->GetChild(0));
    EXPECT_TRUE(path->GetChild(1));
    EXPECT_FALSE(path->GetChild(5)); // out of range → null
}

// Ported from: CurveCollection.test.ts it("startEndPoint") — chain start/end from first/last child.
TEST(PathLoopTest, StartEndPoint) {
    // Path: linestring + arc + linestring. Start = first child f0; end = last child f1.
    auto path = Path::Create(std::vector<CurvePrimitivePtr>{
        LineString3d::create({Point3d::From(-4, -2, 0), Point3d::From(4, -2, 0)}),
        Arc3d::CreateXY(Point3d::From(4, 0, 0), 2.0),
        LineString3d::create({Point3d::From(4, 2, 0), Point3d::From(-4, 2, 0)}),
    });
    ExpectNear(Point3d::From(-4, -2, 0), path->StartPoint());
    ExpectNear(Point3d::From(-4, 2, 0), path->EndPoint());

    // Empty chain → no start/end.
    auto empty = Path::Create();
    EXPECT_FALSE(empty->StartPoint().has_value());
    EXPECT_FALSE(empty->EndPoint().has_value());
}

// Ported from: CurveCollection.test.ts verifyCurveCollection (clone + isAlmostEqual subset).
TEST(PathLoopTest, CloneIsDeepCopy) {
    auto path = Path::Create(std::vector<CurvePrimitivePtr>{
        LineString3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)}),
    });
    auto cloned = path->clone().StaticCast<Path>();
    ASSERT_TRUE(cloned);
    ASSERT_EQ(1u, cloned->Curves().size());
    // Deep copy: original and clone compare almost-equal; mutating original's child count detaches.
    EXPECT_TRUE(path->IsAlmostEqual(*cloned, kTol));
    path->Curves().clear();
    EXPECT_FALSE(path->IsAlmostEqual(*cloned, kTol));
}

// Ported from: Path.dgnBoundaryType (=1) / Loop.dgnBoundaryType (=2).
TEST(PathLoopTest, DgnBoundaryType) {
    EXPECT_EQ(1, Path::Create()->DgnBoundaryType());
    EXPECT_EQ(2, Loop::Create()->DgnBoundaryType());
}

// Ported from: CurveCollection curveCollectionType discriminator + IsSameGeometryClass (no RTTI).
TEST(PathLoopTest, DiscriminatorAndSameClass) {
    auto path = Path::Create();
    auto loop = Loop::Create();
    EXPECT_EQ(CurveCollectionType::Path, path->GetCurveCollectionType());
    EXPECT_EQ(CurveCollectionType::Loop, loop->GetCurveCollectionType());
    EXPECT_TRUE(path->IsSameGeometryClass(*path));
    EXPECT_FALSE(path->IsSameGeometryClass(*loop));
    EXPECT_FALSE(loop->IsSameGeometryClass(*Path::Create()->clone()));
}

// Ported from: Loop.createPolygon — point array wraps as a single closed LineString3d child.
TEST(PathLoopTest, CreatePolygonClosure) {
    auto loop = Loop::CreatePolygon({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0)});
    ASSERT_EQ(1u, loop->Curves().size());
    auto const& ls = static_cast<LineString3d const&>(*loop->Curves().front());
    // Closure point appended (first == last).
    EXPECT_EQ(4u, ls.PointCount());
    EXPECT_TRUE(ls.Points().front().AlmostEqual(ls.Points().back()));
}

// Ported from: Path.cloneStroked — children stroked into a single LineString3d child.
TEST(PathLoopTest, CloneStroked) {
    auto path = Path::Create(std::vector<CurvePrimitivePtr>{
        LineString3d::create({Point3d::From(0, 0, 0), Point3d::From(1, 0, 0)}),
        LineString3d::create({Point3d::From(1, 0, 0), Point3d::From(2, 0, 0)}),
    });
    auto stroked = path->CloneStroked({});
    ASSERT_TRUE(stroked);
    ASSERT_EQ(1u, stroked->Curves().size());
    auto const& ls = static_cast<LineString3d const&>(*stroked->Curves().front());
    EXPECT_GE(ls.PointCount(), 2u);
}

// Authored: no reference unit test exists in itwinjs core-geometry for CurveCollection.isAnyRegionType
// (faithful port of CurveCollection.ts isAnyRegionType — Loop is a region type, Path is not).
TEST(PathLoopTest, IsAnyRegionType) {
    auto loop = Loop::Create();
    EXPECT_TRUE(loop->isAnyRegionType());
    auto path = Path::Create();
    EXPECT_FALSE(path->isAnyRegionType());
}
