// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/render/primitives/GeometryAccumulator.test.ts
// DanQing dqRender — GeometryAccumulator tests (D′ accumulator port)
//
// 保真依据（CLAUDE.md §5）：逐 it 移植 GeometryAccumulator.test.ts 的 addPath/addLoop/addPolyface/
// addGeometry/clear/toMeshBuilderMap/toMeshes（saveToGraphicList 留 PR F——finish→GraphicBranch 路由）。
// 参考经 Geometry.createFromLoop→getPolyfaces 取 polyface；DanQing 直接建 4 点 quad IndexedPolyface
//（addPolyface 的被测语义是"累积一个 polyface 几何"，与 polyface 来源无关；直接构造避免 Loop 镶嵌
// 的 SweepContour TODO fallback）。GraphicParams 字段直赋（DanQing GraphicParams 公有 lineColor/fillColor）。
#include <gtest/gtest.h>

#include "render/DisplayParams.h"
#include "render/GeometryAccumulator.h"
#include "render/GeometryPrimitives.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/GraphicParams.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <vector>

using namespace dqRender;
using namespace dqCommon;
using namespace dqGeom;

namespace {

// 4-point unit quad in the z=0 plane, triangulated as 2 facets (1,2,3) and (1,3,4).
dqBase::RefPtr<IndexedPolyface> createQuadPolyface()
{
    auto pf = IndexedPolyface::create(false, false, false);
    pf->AddPoint(Point3d::From(0, 0, 0));
    pf->AddPoint(Point3d::From(1, 0, 0));
    pf->AddPoint(Point3d::From(1, 1, 0));
    pf->AddPoint(Point3d::From(0, 1, 0));
    pf->AddPointIndex(1);
    pf->AddPointIndex(2);
    pf->AddPointIndex(3);
    pf->TerminateFacet();
    pf->AddPointIndex(1);
    pf->AddPointIndex(3);
    pf->AddPointIndex(4);
    pf->TerminateFacet();
    return pf;
}

// 1:1 GeometryAccumulator.test.ts gfParams (lineColor white, fillColor black) → DisplayParams.createForMesh.
DisplayParams meshDisplayParams()
{
    GraphicParams gf;
    gf.lineColor = ColorDef::white;
    gf.fillColor = ColorDef::black;  // forces region outline flag (reference comment)
    return DisplayParams::createForMesh(gf, false);
}

// 1:1 gfParams2 (lineColor white) → DisplayParams.createForLinear.
DisplayParams linearDisplayParams()
{
    GraphicParams gf;
    gf.lineColor = ColorDef::white;
    return DisplayParams::createForLinear(gf);
}

}  // namespace

// Ported from: GeometryAccumulator.test.ts it("addPath works as expected").
TEST(GeometryAccumulatorTest, AddPath)
{
    GeometryAccumulator accum;
    const std::vector<Point3d> points{Point3d::From(0, 0, 0), Point3d::From(1, 0, 0)};
    auto pth = Path::Create(points);  // 1:1 Path.create(LineString3d.create(points))
    const DisplayParams displayParams = linearDisplayParams();

    EXPECT_TRUE(accum.geometries().isEmpty());
    EXPECT_TRUE(accum.addPath(pth, displayParams, Transform::CreateIdentity(), false));
    EXPECT_EQ(accum.geometries().length(), 1u);
}

// Ported from: GeometryAccumulator.test.ts it("addLoop works as expected").
TEST(GeometryAccumulatorTest, AddLoop)
{
    GeometryAccumulator accum;
    const std::vector<Point3d> points{
        Point3d::From(0, 0, 0), Point3d::From(1, 0, 0),
        Point3d::From(1, 1, 0), Point3d::From(0, 1, 0)};
    auto loop = Loop::CreatePolygon(points);  // 1:1 Loop.create(LineString3d.create(points))
    const DisplayParams displayParams = meshDisplayParams();

    EXPECT_TRUE(accum.geometries().isEmpty());
    EXPECT_TRUE(accum.addLoop(loop, displayParams, Transform::CreateIdentity(), false));
    EXPECT_EQ(accum.geometries().length(), 1u);
}

// Ported from: GeometryAccumulator.test.ts it("addPolyface works as expected").
TEST(GeometryAccumulatorTest, AddPolyface)
{
    GeometryAccumulator accum;
    const auto pf = createQuadPolyface();
    ASSERT_EQ(pf->Data().PointCount(), 4u);
    const DisplayParams displayParams = meshDisplayParams();

    EXPECT_TRUE(accum.geometries().isEmpty());
    EXPECT_TRUE(accum.addPolyface(pf, displayParams, Transform::CreateIdentity()));
    EXPECT_EQ(accum.geometries().length(), 1u);
}

// Ported from: GeometryAccumulator.test.ts it("addGeometry works as expected").
TEST(GeometryAccumulatorTest, AddGeometry)
{
    GeometryAccumulator accum;
    EXPECT_TRUE(accum.geometries().isEmpty());
    EXPECT_TRUE(accum.isEmpty());
    // 1:1 FakeGeometry — a minimal real Geometry record (point string) to exercise addGeometry push.
    const std::vector<Point3d> pts{Point3d::From(0, 0, 0)};
    auto geom = Geometry::createFromPointString(
        pts, Transform::CreateIdentity(), Range3d{}, linearDisplayParams(), std::nullopt);
    EXPECT_TRUE(accum.addGeometry(std::move(geom)));
    EXPECT_EQ(accum.geometries().length(), 1u);
}

// Ported from: GeometryAccumulator.test.ts it("clear works as expected").
TEST(GeometryAccumulatorTest, Clear)
{
    GeometryAccumulator accum;
    EXPECT_TRUE(accum.isEmpty());
    const std::vector<Point3d> pts{Point3d::From(0, 0, 0)};
    auto geom = Geometry::createFromPointString(
        pts, Transform::CreateIdentity(), Range3d{}, linearDisplayParams(), std::nullopt);
    accum.addGeometry(std::move(geom));
    EXPECT_FALSE(accum.isEmpty());
    accum.clear();
    EXPECT_TRUE(accum.isEmpty());
}

// Shared setup for toMeshBuilderMap / toMeshes: a polyface (mesh) + a path (polyline) → 2 distinct keys.
struct GeometryAccumulatorMeshingTest : public ::testing::Test {
    GeometryAccumulator accum;
    void SetUp() override
    {
        ASSERT_TRUE(accum.addPolyface(createQuadPolyface(), meshDisplayParams(), Transform::CreateIdentity()));
        const std::vector<Point3d> pathPts{Point3d::From(0, 0, 0), Point3d::From(1, 0, 0)};
        auto pth = Path::Create(pathPts);
        ASSERT_TRUE(accum.addPath(pth, linearDisplayParams(), Transform::CreateIdentity(), false));
        ASSERT_EQ(accum.geometries().length(), 2u);
    }
};

// Ported from: GeometryAccumulator.test.ts it("toMeshBuilderMap works as expected").
TEST_F(GeometryAccumulatorMeshingTest, ToMeshBuilderMap)
{
    GeometryOptions opts;
    opts.wantEdges = false;
    opts.preserveOrder = false;
    auto map = accum.toMeshBuilderMap(opts, 0.22, std::nullopt);
    EXPECT_EQ(map->size(), 2u);
}

// Ported from: GeometryAccumulator.test.ts it("toMeshes works as expected").
TEST_F(GeometryAccumulatorMeshingTest, ToMeshes)
{
    GeometryOptions opts;
    opts.wantEdges = false;
    opts.preserveOrder = false;
    MeshList meshes = accum.toMeshes(opts, 0.22, std::nullopt);
    EXPECT_EQ(meshes.length(), 2u);
}

// Ported from: GeometryAccumulator.test.ts it("saveToGraphicList works as expected") — DEFERRED to PR F
// (PrimitiveBuilder.finish→GraphicBranch routing via RenderSystem::createMeshGraphics). Tracked in plan.
