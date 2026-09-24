// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/test/render/primitives/MeshPrimitives.test.ts
// DanQing dqRender — Mesh / MeshPolyline tests (D′ MeshPrimitives port)
//
// 保真依据（CLAUDE.md §5）：逐 it 移植 MeshPrimitives.test.ts。constructor/addPolyline/addTriangle/
// addVertex 断言 1:1。FakeDisplayParams → DisplayParams(Type::Linear, black, black)。VertexKey 直接
// 构造作 addVertex 入参（DanQing Mesh::addVertex 取 VertexKeyProps，VertexKey::props() 转换）。
// quantizePositions=true 在 DanQing 被忽略（QPoint 路径 Phase-N），但被测断言（length 计数）等价。
#include <gtest/gtest.h>

#include "render/MeshPrimitives.h"
#include "render/VertexKey.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/FeatureIndex.h>
#include <dqCommon/OctEncodedNormal.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <vector>

using namespace dqRender;
using namespace dqCommon;
using namespace dqGeom;

// 1:1 FakeDisplayParams (MeshPrimitives.test.ts:14-16): super(Type.Linear, black, black).
namespace {
DisplayParams fakeDisplayParams()
{
    return DisplayParams(DisplayParams::Type::Linear, ColorDef::black, ColorDef::black);
}
}  // namespace

// Ported from: MeshPrimitives.test.ts describe("MeshPrimitive Tests") / it("constructor").
TEST(MeshPrimitivesTest, Constructor)
{
    const DisplayParams displayParams = fakeDisplayParams();
    const Range3d range;  // createNull()
    const bool is2d = false;
    const bool isPlanar = true;

    Mesh::Props props;
    props.displayParams = displayParams;
    props.range = range;
    props.is2d = is2d;
    props.isPlanar = isPlanar;

    // Mesh type.
    props.type = MeshPrimitiveType::Mesh;
    auto m = Mesh::create(props);
    EXPECT_EQ(m->type(), MeshPrimitiveType::Mesh);
    EXPECT_TRUE(m->displayParams().equals(displayParams));
    EXPECT_FALSE(m->features().has_value());
    EXPECT_EQ(m->is2d(), is2d);
    EXPECT_EQ(m->isPlanar(), isPlanar);
    EXPECT_EQ(m->points().length(), 0u);
    ASSERT_NE(m->triangles(), nullptr);
    EXPECT_EQ(m->polylines(), nullptr);

    // Polyline type.
    props.type = MeshPrimitiveType::Polyline;
    m = Mesh::create(props);
    ASSERT_NE(m->polylines(), nullptr);
    EXPECT_EQ(m->triangles(), nullptr);

    // Point type.
    props.type = MeshPrimitiveType::Point;
    m = Mesh::create(props);
    ASSERT_NE(m->polylines(), nullptr);
    EXPECT_EQ(m->triangles(), nullptr);
}

// Ported from: MeshPrimitives.test.ts it("addPolyline").
TEST(MeshPrimitivesTest, AddPolyline)
{
    const DisplayParams displayParams = fakeDisplayParams();
    const Range3d range;
    Mesh::Props props;
    props.displayParams = displayParams;
    props.range = range;
    props.type = MeshPrimitiveType::Polyline;
    props.isPlanar = true;

    auto m = Mesh::create(props);
    ASSERT_NE(m->polylines(), nullptr);
    EXPECT_EQ(m->polylines()->size(), 0u);

    MeshPolyline mp(std::vector<uint32_t>{1, 2, 3});
    m->addPolyline(mp);
    EXPECT_EQ(m->polylines()->size(), 1u);

    // 1:1: a MeshPolyline with fewer than 2 indices is not added (Polyline type only).
    auto m2 = Mesh::create(props);
    ASSERT_NE(m2->polylines(), nullptr);
    EXPECT_EQ(m2->polylines()->size(), 0u);
    MeshPolyline mpShort(std::vector<uint32_t>{1});
    m2->addPolyline(mpShort);
    EXPECT_EQ(m2->polylines()->size(), 0u);
}

// Ported from: MeshPrimitives.test.ts it("addTriangle").
TEST(MeshPrimitivesTest, AddTriangle)
{
    const DisplayParams displayParams = fakeDisplayParams();
    const Range3d range;
    Mesh::Props props;
    props.displayParams = displayParams;
    props.range = range;
    props.type = MeshPrimitiveType::Mesh;
    props.isPlanar = true;

    const auto m = Mesh::create(props);
    ASSERT_NE(m->triangles(), nullptr);
    EXPECT_EQ(m->triangles()->length(), 0u);
    Triangle t;
    m->addTriangle(t);
    EXPECT_EQ(m->triangles()->length(), 1u);
}

// Ported from: MeshPrimitives.test.ts it("addVertex").
TEST(MeshPrimitivesTest, AddVertex)
{
    const DisplayParams displayParams = fakeDisplayParams();
    // 1:1 Range3d.createArray([0,0,0],[1000,1000,1000]).
    Range3d range;
    range.ExtendPoint(Point3d::FromZero());
    range.ExtendPoint(Point3d::From(1000, 1000, 1000));

    Mesh::Props props;
    props.displayParams = displayParams;
    props.range = range;
    props.type = MeshPrimitiveType::Mesh;
    props.isPlanar = true;

    // position only.
    auto m = Mesh::create(props);
    EXPECT_EQ(m->points().length(), 0u);
    const Point3d p = Point3d::From(100, 100, 100);
    VertexKeyProps vkp;
    vkp.position = p;
    vkp.fillColor = ColorDef::white.getTbgr();
    size_t index = m->addVertex(vkp);
    EXPECT_EQ(index, 0u);
    EXPECT_EQ(m->points().length(), 1u);
    EXPECT_EQ(m->normals().size(), 0u);
    EXPECT_EQ(m->uvParams().size(), 0u);

    // position + normal + uvParam.
    auto m2 = Mesh::create(props);
    EXPECT_EQ(m2->normals().size(), 0u);
    EXPECT_EQ(m2->uvParams().size(), 0u);
    EXPECT_EQ(m2->points().length(), 0u);
    const OctEncodedNormal oct(10);
    const Point2d param = Point2d::From(10, 10);
    VertexKeyProps vkp2;
    vkp2.position = p;
    vkp2.fillColor = ColorDef::white.getTbgr();
    vkp2.normal = oct;
    vkp2.uvParam = param;
    index = m2->addVertex(vkp2);
    EXPECT_EQ(m2->normals().size(), 1u);
    EXPECT_EQ(m2->uvParams().size(), 1u);
    EXPECT_EQ(m2->points().length(), 1u);

    // 1:1: VertexKey passed directly to addVertex (structural compat). DanQing: VertexKey::props().
    auto m3 = Mesh::create(props);
    VertexKey key(p, ColorDef::white.getTbgr(), oct, param);
    m3->addVertex(key.props());
    EXPECT_EQ(m3->points().length(), 1u);
    EXPECT_EQ(m3->normals().size(), 1u);
    EXPECT_EQ(m3->uvParams().size(), 1u);
}
