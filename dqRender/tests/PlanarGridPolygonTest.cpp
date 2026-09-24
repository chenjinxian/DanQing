// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PlanarGrid.ts:52-111
//              (PlanarGridGeometry.create -> BuildPlanarGridPolygon)
// DanQing dqRender - procedural grid polygon (frustum∩plane) unit tests
#include "render/PlanarGridGraphic.h"

#include <dqCommon/Frustum.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqGeom;

// Ported from: PlanarGridGeometry.create (PlanarGrid.ts:52-111). An AABB frustum
// [-5,5]^3 cut by the z=0 ground plane -> the z=0 cross-section rectangle, UVs == xy
// (identity axes, spacing 1, origin 0), fan-triangulated into 2 triangles.
TEST(PlanarGridPolygonTest, BuildFromAabbFrustumGroundPlane)
{
    dqCommon::Frustum const f = dqCommon::Frustum::fromRange(Range3d(-5, -5, -5, 5, 5, 5));
    PlanarGridProps grid;
    grid.origin = Point3d::From(0, 0, 0);
    grid.rMatrix = Matrix3d::CreateIdentity();
    grid.spacing = Point2d{1.0, 1.0};
    grid.gridsPerRef = 10.0;

    auto poly = BuildPlanarGridPolygon(f, grid);
    ASSERT_TRUE(poly.has_value());
    EXPECT_EQ(poly->positions.size(), 4u);   // z=0 cross-section of the box
    EXPECT_EQ(poly->uvs.size(), 4u);
    EXPECT_EQ(poly->indices.size(), 6u);     // 4 verts -> 2 triangles -> 6 indices
    for (size_t i = 0; i < poly->positions.size(); ++i) {
        EXPECT_NEAR(poly->positions[i].z, 0.0, 1e-9);
        // 量化 UV（PlanarGrid.ts:70-77 QPoint2dList）：uv ∈ [0,1]，解码
        // uv_raw = qTexCoordParams.xy + qTexCoordParams.zw·uv。identity 旋转、
        // spacing 1、gridsPerRef 10 下 origin=fmod(-5,10)=-5、extent=10，
        // 解码回 p.x/p.y（1/65535 量化精度）。
        double const dx = poly->qTexCoordParams[0]
                        + poly->qTexCoordParams[2] * static_cast<double>(poly->uvs[i].x);
        double const dy = poly->qTexCoordParams[1]
                        + poly->qTexCoordParams[3] * static_cast<double>(poly->uvs[i].y);
        EXPECT_NEAR(dx, poly->positions[i].x, 1e-3);
        EXPECT_NEAR(dy, poly->positions[i].y, 1e-3);
    }
    for (uint32_t idx : poly->indices)
        EXPECT_LT(idx, poly->positions.size());
}

// Ported from: PlanarGridGeometry.create - frustum entirely above z=0 -> nullopt.
TEST(PlanarGridPolygonTest, ReturnsNoneWhenFrustumMissesPlane)
{
    dqCommon::Frustum const f = dqCommon::Frustum::fromRange(Range3d(-5, -5, 10, 5, 5, 20));
    PlanarGridProps grid;
    grid.spacing = Point2d{1.0, 1.0};
    auto poly = BuildPlanarGridPolygon(f, grid);
    EXPECT_FALSE(poly.has_value());
}
