// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/ViewingSpace.ts
//              ViewingSpace.getFrustum          (L471-503)
//              ViewingSpace.getPixelSizeAtPoint (L505-510)
//              ViewingSpace worldToNdc derivation (Decision 1: NdcFromNpc ×
//                worldToNpcMap.transform0 — the itwinjs in-shader NPC→NDC
//                conversion hoisted into the u_mvp matrix).
// Authored: no reference test exists in itwinjs-core/imodel-native for the
//           getFrustum / getPixelSizeAtPoint / GetWorldToNdcMatrix surface
//           directly — the reference ViewingSpace is constructed internally
//           and exercised through the Viewport suite (which needs Viewport +
//           TwoWayViewportSync infra DanQing lacks). Assertions below derive from
//           the reference method contracts: getFrustum seeds the NPC cube then
//           applies npcToWorld/npcToView; getPixelSizeAtPoint = world-distance
//           of one view-pixel; worldToNdc maps the NPC cube to NDC[-1,1].

#include <dqApp/ViewingSpace.h>
#include <dqApp/ViewState.h>
#include <dqCommon/CoordSystem.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/Npc.h>
#include <dqGeom/Map4d.h>
#include <dqGeom/Matrix4d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <array>
#include <cmath>

#include <gtest/gtest.h>

using dqApp::SpatialViewState;
using dqApp::ViewRect;
using dqApp::ViewingSpace;
using dqCommon::CoordSystem;
using dqCommon::Frustum;
using dqGeom::Point3d;
using dqGeom::Vector3d;

namespace {

// 200^3 ortho frustum (origin = corner) in an 800x600 rect — same fixture as
// CoordinateConversionTest.MakeOrthoSpace.
ViewingSpace MakeSpace()
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, Point3d::From(0, 0, 0), Vector3d::From(200, 200, 200));
    ViewingSpace vs;
    ViewRect rect{0, 0, 800, 600};
    vs.update(*view->AsViewState3d(), rect);
    return vs;
}

// Apply a column-major float[16] (GL convention) to a point with perspective divide.
Point3d ApplyMatrix(std::array<float, 16> const& m, Point3d const& p)
{
    double const x = p.x, y = p.y, z = p.z;
    double const rx = m[0] * x + m[4] * y + m[8] * z + m[12];
    double const ry = m[1] * x + m[5] * y + m[9] * z + m[13];
    double const rz = m[2] * x + m[6] * y + m[10] * z + m[14];
    double const rw = m[3] * x + m[7] * y + m[11] * z + m[15];
    if (std::abs(rw) < 1e-12) return Point3d::From(rx, ry, rz);
    double const inv = 1.0 / rw;
    return Point3d::From(rx * inv, ry * inv, rz * inv);
}

Frustum NpcCube() { Frustum f; f.initNpc(); return f; }
}  // namespace

// getFrustum(World) seeds the NPC cube then applies npcToWorld — each corner
// equals NpcToWorld(NPC corner). Ported from ViewingSpace.getFrustum (L499).
TEST(ViewingSpaceApi, GetFrustumWorldMatchesNpcToWorld)
{
    ViewingSpace vs = MakeSpace();
    Frustum fw, fnpc = NpcCube();
    vs.getFrustum(fw, CoordSystem::World);
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        Point3d const w = vs.NpcToWorld(fnpc.points[i]);
        EXPECT_NEAR(fw.points[i].x, w.x, 1e-6);
        EXPECT_NEAR(fw.points[i].y, w.y, 1e-6);
        EXPECT_NEAR(fw.points[i].z, w.z, 1e-6);
    }
}

// getFrustum(View) seeds the NPC cube then applies npcToView. Ported from
// ViewingSpace.getFrustum (L495).
TEST(ViewingSpaceApi, GetFrustumViewMatchesNpcToView)
{
    ViewingSpace vs = MakeSpace();
    Frustum fv, fnpc = NpcCube();
    vs.getFrustum(fv, CoordSystem::View);
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        Point3d const v = vs.NpcToView(fnpc.points[i]);
        EXPECT_NEAR(fv.points[i].x, v.x, 1e-6);
        EXPECT_NEAR(fv.points[i].y, v.y, 1e-6);
        EXPECT_NEAR(fv.points[i].z, v.z, 1e-6);
    }
}

// getPixelSizeAtPoint: default (NPC center) == explicit world-center, and 1
// view-pixel = 200 world units / 800 px = 0.25. Ported from
// ViewingSpace.getPixelSizeAtPoint (L505-510).
TEST(ViewingSpaceApi, GetPixelSizeAtPointIsOneViewPixelInWorld)
{
    ViewingSpace vs = MakeSpace();
    double const def = vs.getPixelSizeAtPoint();  // default = NPC center
    EXPECT_GT(def, 0.0);
    Point3d const worldCenter = Point3d::From(100.0, 100.0, 100.0);  // NPC center
    double const expl = vs.getPixelSizeAtPoint(&worldCenter);
    EXPECT_NEAR(def, expl, 1e-9);
    EXPECT_NEAR(def, 0.25, 1e-6);  // 200 units / 800 px
}

// WorldToNdcParity: GetWorldToNdcMatrix() maps each NPC-cube corner (in world
// coords via NpcToWorld) to NDC[-1,1] — corner c → (2c-1). Decision 1:
// NdcFromNpc × worldToNpcMap.transform0. Validates the float[16] encoding +
// the NPC[0,1]→NDC[-1,1] semantics the renderer's u_mvp depends on.
TEST(ViewingSpaceApi, WorldToNdcMapsNpcCubeToNdcCube)
{
    ViewingSpace vs = MakeSpace();
    auto const& ndc = vs.GetWorldToNdcMatrix();
    Frustum fnpc = NpcCube();
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        Point3d const world = vs.NpcToWorld(fnpc.points[i]);
        Point3d const n = ApplyMatrix(ndc, world);
        Point3d const expected = Point3d::From(
            2.0 * fnpc.points[i].x - 1.0,
            2.0 * fnpc.points[i].y - 1.0,
            2.0 * fnpc.points[i].z - 1.0);
        EXPECT_NEAR(n.x, expected.x, 1e-4);
        EXPECT_NEAR(n.y, expected.y, 1e-4);
        EXPECT_NEAR(n.z, expected.z, 1e-4);
    }
}
