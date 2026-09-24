// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/render/primitives/Primitives.test.ts (ToleranceRatio)
//              core/frontend/src/test/render/primitives/Strokes.test.ts
// DanQing dqRender — C1 leaf render vocabulary tests (Primitives / MeshPrimitive / Strokes / Polyface).
//
// 保真依据（CLAUDE.md §5）：ToleranceRatio 断言移植自 Primitives.test.ts；Triangle/TriangleSet/
// StrokesPrimitive/PolyfacePrimitive 参考无对应独立单元测试（Primitives.test.ts 侧重 Geometry/GeometryList，
// 属 C2），按 §5f 标 Authored。场景/断言来自参考类型语义（Primitives.ts / Strokes.ts / Polyface.ts）。
#include <gtest/gtest.h>

#include "render/MeshPrimitive.h"
#include "render/Polyface.h"
#include "render/Primitives.h"
#include "render/Strokes.h"

#include <dqCommon/GraphicParams.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <vector>

using namespace dqRender;

// Ported from: Primitives.test.ts describe("ToleranceRatio") / it("ToleranceRatio works as expected").
TEST(ToleranceRatioTest, ConstantsMatchReference) {
    EXPECT_DOUBLE_EQ(ToleranceRatio::vertex, 0.1);
    EXPECT_DOUBLE_EQ(ToleranceRatio::facetArea, 0.1);
}

// Authored: no reference unit test for Triangle/TriangleList in itwinjs-core (Primitives.test.ts covers
// Geometry, not Triangle). Behaviors defined by Primitives.ts:23-98.
TEST(TriangleTest, SetIndicesDegenerateAndRoundTrip) {
    Triangle t;
    t.setIndices(1, 2, 3);
    EXPECT_FALSE(t.isDegenerate());
    t.setIndices(4, 4, 5);
    EXPECT_TRUE(t.isDegenerate());

    TriangleList list;
    t.setIndices(1, 2, 3);
    t.setEdgeVisibility(true, false, true);
    list.addTriangle(t);
    ASSERT_EQ(1u, list.length());
    EXPECT_EQ(3u * 1, list.indices().size());

    Triangle got = list.getTriangle(0);
    EXPECT_EQ(1u, got.index(0));
    EXPECT_EQ(2u, got.index(1));
    EXPECT_EQ(3u, got.index(2));
    EXPECT_TRUE(got.isEdgeVisible(0));
    EXPECT_FALSE(got.isEdgeVisible(1));
    EXPECT_TRUE(got.isEdgeVisible(2));
}

// Authored: TriangleSet dedup — inserting the same triangle (any vertex order with same index set) twice
// yields one key and invokes onInsert once. Behaviors defined by Primitives.ts:157-164 (SortedArray dedup).
TEST(TriangleSetTest, DedupsEquivalentTriangles) {
    TriangleSet set;
    int insertCount = 0;
    auto onInsert = [&](const TriangleKey&) { ++insertCount; };

    Triangle a, b;
    a.setIndices(1, 2, 3);
    b.setIndices(3, 1, 2); // same index set, different order → same TriangleKey
    set.insertKey(a, onInsert);
    set.insertKey(b, onInsert);

    EXPECT_EQ(1u, set.size());
    EXPECT_EQ(1, insertCount);
}

// Authored: no reference unit test for MeshPrimitiveType (enum). Behaviors defined by MeshPrimitive.ts:13-17.
TEST(MeshPrimitiveTest, EnumValuesAndPoint3dList) {
    EXPECT_NE(MeshPrimitiveType::Mesh, MeshPrimitiveType::Polyline);
    EXPECT_NE(MeshPrimitiveType::Polyline, MeshPrimitiveType::Point);

    Point3dList list;
    list.range = dqGeom::Range3d();
    list.add(dqGeom::Point3d::From(1, 2, 3));
    list.add(dqGeom::Point3d::From(4, 5, 6));
    EXPECT_EQ(2u, list.points.size());
}

// Authored: no reference unit test for StrokesPrimitive (Strokes.test.ts covers Geometry stroking, not the
// primitive itself). Verifies create populates fields and transform mutates stroked points in place
// (Strokes.ts:26-48).
TEST(StrokesPrimitiveTest, CreateAndTransform) {
    dqCommon::GraphicParams gf;
    DisplayParams dp = DisplayParams::createForLinear(gf);
    StrokesPrimitive sp = StrokesPrimitive::create(dp, /*isDisjoint=*/false, /*isPlanar=*/true);
    sp.strokes.emplace_back(std::vector<dqGeom::Point3d>{dqGeom::Point3d::From(1, 2, 3)});
    ASSERT_EQ(1u, sp.strokes.size());

    sp.transform(dqGeom::Transform::CreateTranslation(10, 0, 0));
    EXPECT_NEAR(sp.strokes[0].points[0].x, 11.0, 1e-10);
}

// Authored: no reference unit test for PolyfacePrimitive (Polyface.ts:13-34). Verifies create stores the
// polyface, clone produces an independent copy, transform delegates to TryTransformInPlace.
TEST(PolyfacePrimitiveTest, CreateCloneTransform) {
    dqCommon::GraphicParams gf;
    DisplayParams dp = DisplayParams::createForMesh(gf, false);

    auto pf = dqGeom::IndexedPolyface::create();
    pf->AddPoint(dqGeom::Point3d::From(0, 0, 0));
    pf->AddPoint(dqGeom::Point3d::From(1, 0, 0));
    pf->AddPoint(dqGeom::Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();

    PolyfacePrimitive pp = PolyfacePrimitive::create(dp, pf, /*displayEdges=*/true, /*isPlanar=*/true);
    EXPECT_TRUE(pp.displayEdges);
    EXPECT_TRUE(pp.isPlanar);
    EXPECT_EQ(pf.Get(), pp.indexedPolyface().Get());

    PolyfacePrimitive cloned = pp.clone();
    EXPECT_NE(pf.Get(), cloned.indexedPolyface().Get()); // clone is an independent copy

    EXPECT_TRUE(pp.transform(dqGeom::Transform::CreateTranslation(5, 0, 0)));
}
