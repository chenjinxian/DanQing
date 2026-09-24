// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/render/primitives/Primitives.test.ts
//              describe("GeometryList") / it("LineStringPointString")
// DanQing dqRender — Geometry records + GeometryList tests.
//
// 保真依据（CLAUDE.md §5）：GeometryList/LineStringPointString 场景移植自 Primitives.test.ts；
// 参考 Loop/Solid 子测试用 BSplineSurface3d.addUVGridBody（DanQing 未移植），按 §5c 仅移植自包含场景，
// 补 Authored createFromPolyface/createFromSolidPrimitive(Box) 覆盖 _getPolyfaces 路径。
#include <gtest/gtest.h>

#include "render/GeometryList.h"
#include "render/GeometryPrimitives.h"

#include <dqCommon/GraphicParams.h>
#include <dqGeom/Box.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/Transform.h>

#include <vector>

using namespace dqRender;
using dqGeom::Point3d;
using dqGeom::Range3d;
using dqGeom::Transform;

namespace {
DisplayParams linearParams() {
    return DisplayParams::createForLinear(dqCommon::GraphicParams{});
}
DisplayParams meshParams() {
    return DisplayParams::createForMesh(dqCommon::GraphicParams{}, false);
}
}  // namespace

// Ported from: Primitives.test.ts describe("GeometryList") / it("LineStringPointString").
TEST(GeometryPrimitivesTest, LineStringAndPointString) {
    GeometryList glist;
    EXPECT_TRUE(glist.isEmpty());

    const DisplayParams dp = linearParams();
    const Transform identity = Transform::CreateIdentity();
    const Range3d range = Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1);

    std::vector<Point3d> pts = { Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(1, 1, 0) };
    auto g0 = Geometry::createFromLineString(pts, identity, range, dp, std::nullopt);
    // Line strings produce strokes, not polyfaces.
    EXPECT_FALSE(g0->getPolyfaces(0.001).has_value());
    auto strokes = g0->getStrokes(0.001);
    ASSERT_TRUE(strokes.has_value());
    ASSERT_EQ(1u, strokes->size());
    EXPECT_EQ(1u, strokes->front().strokes.size());

    auto g1 = Geometry::createFromPointString(pts, identity, range, dp, std::nullopt);
    EXPECT_FALSE(g1->getPolyfaces(0.001).has_value());
    ASSERT_TRUE(g1->getStrokes(0.001).has_value());

    Geometry* firstBefore = glist.first();
    EXPECT_EQ(nullptr, firstBefore);
    glist.push(std::move(g0));
    EXPECT_EQ(g0.get(), nullptr); // moved-from
    ASSERT_EQ(1u, glist.length());
    EXPECT_NE(nullptr, glist.first());
}

// Authored: reference Primitives.test.ts Loop/Solid cases use BSplineSurface3d.addUVGridBody (not ported);
// this covers the polyface Geometry record's _getPolyfaces path with a hand-built IndexedPolyface.
TEST(GeometryPrimitivesTest, PolyfaceProducesPolyfacePrimitive) {
    auto pf = dqGeom::IndexedPolyface::create();
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    const DisplayParams dp = meshParams();
    auto g = Geometry::createFromPolyface(pf, Transform::CreateIdentity(),
                                         Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 0), dp, std::nullopt);
    auto polyfaces = g->getPolyfaces(0.001);
    ASSERT_TRUE(polyfaces.has_value());
    EXPECT_EQ(1u, polyfaces->size());
    EXPECT_FALSE(g->getStrokes(0.001).has_value());
}

// Authored: covers SolidPrimitiveGeometry._getPolyfaces via PolyfaceBuilder::AddGeometryQuery → AddBox.
TEST(GeometryPrimitivesTest, SolidPrimitiveBoxProducesPolyfacePrimitive) {
    auto box = dqGeom::Box::CreateRange(
        Range3d::create({ Point3d::From(0, 0, 0), Point3d::From(1, 1, 1) }), true);

    const DisplayParams dp = meshParams();
    auto g = Geometry::createFromSolidPrimitive(box, Transform::CreateIdentity(),
                                                Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1), dp, std::nullopt);
    auto polyfaces = g->getPolyfaces(0.001);
    ASSERT_TRUE(polyfaces.has_value());
    EXPECT_EQ(1u, polyfaces->size());
}

// Authored: GeometryList.computeRange unions the tileRanges of its records.
TEST(GeometryListTest, ComputeRangeUnionsTileRanges) {
    GeometryList glist;
    const DisplayParams dp = linearParams();
    std::vector<Point3d> a = { Point3d::From(0, 0, 0), Point3d::From(1, 0, 0) };
    std::vector<Point3d> b = { Point3d::From(5, 5, 5), Point3d::From(6, 6, 6) };
    glist.push(Geometry::createFromLineString(a, Transform::CreateIdentity(),
                                              Range3d::CreateXYZXYZ(0, 0, 0, 1, 0, 0), dp, std::nullopt));
    glist.push(Geometry::createFromLineString(b, Transform::CreateIdentity(),
                                              Range3d::CreateXYZXYZ(5, 5, 5, 6, 6, 6), dp, std::nullopt));

    Range3d range = glist.computeRange();
    EXPECT_NEAR(range.low.x, 0.0, 1e-10);
    EXPECT_NEAR(range.high.x, 6.0, 1e-10);
    EXPECT_NEAR(range.high.z, 6.0, 1e-10);
}
