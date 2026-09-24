// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — tests for BackgroundMapGeometry depth-range helpers.
// Ported from: itwinjs-core core/frontend/src/BackgroundMapGeometry.ts:28-65,91-413
//              core/frontend/src/DisplayStyleState.ts:734-800
// (no upstream unit test exists for getFrustumPlaneIntersectionDepthRange,
//  BackgroundMapGeometry.getFrustumIntersectionDepthRange, or
//  getGlobalGeometryAndHeightRange — all tests below are Authored.)
#include <gtest/gtest.h>

#include "../src/BackgroundMapGeometry.h"

#include <dqApp/BlankConnection.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewingSpace.h>
#include <dqApp/ViewState.h>
#include <dqCommon/Cartographic.h>
#include <dqCommon/CoordSystem.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/Npc.h>
#include <dqCommon/ViewFlags.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <algorithm>
#include <iostream>
#include <optional>

using dqApp::BackgroundMapGeometry;
using dqApp::BlankConnection;
using dqApp::BlankConnectionProps;
using dqApp::getFrustumPlaneIntersectionDepthRange;
using dqApp::getGlobalGeometryAndHeightRange;
using dqApp::SpatialViewState;
using dqApp::StandardView;
using dqApp::ViewingSpace;
using dqApp::ViewRect;
using dqCommon::CoordSystem;
using dqCommon::Frustum;
using dqCommon::ViewFlags;
using dqCommon::ViewFlagsProperties;
using dqGeom::Plane3dByOriginAndUnitNormal;
using dqGeom::Point3d;
using dqGeom::Range1d;
using dqGeom::Range3d;
using dqGeom::Vector3d;

namespace {

// The horizon frustum shared by the depth-range tests: a rigid ortho box looking
// forward-and-down from height ~30. The box sits entirely above z=0 (all corners
// z >= 10). The depth axis (Front->Rear) tilts down so the 4 depth edges hit z=0
// at fraction < 0 (forward of the front face) — defined but outside [0,1].
Frustum MakeHorizonFrustum()
{
    Frustum f;
    f.points[static_cast<int>(dqCommon::Npc::LeftBottomRear)]    = Point3d{0.0,   0.0, 30.0};
    f.points[static_cast<int>(dqCommon::Npc::RightBottomRear)]   = Point3d{0.0,  10.0, 30.0};
    f.points[static_cast<int>(dqCommon::Npc::LeftTopRear)]       = Point3d{2.0,   0.0, 40.0};
    f.points[static_cast<int>(dqCommon::Npc::RightTopRear)]      = Point3d{2.0,  10.0, 40.0};
    f.points[static_cast<int>(dqCommon::Npc::LeftBottomFront)]   = Point3d{100.0, 0.0, 10.0};
    f.points[static_cast<int>(dqCommon::Npc::RightBottomFront)]  = Point3d{100.0, 10.0, 10.0};
    f.points[static_cast<int>(dqCommon::Npc::LeftTopFront)]      = Point3d{102.0, 0.0, 20.0};
    f.points[static_cast<int>(dqCommon::Npc::RightTopFront)]     = Point3d{102.0, 10.0, 20.0};
    return f;
}

}  // namespace

// Authored: no reference test exists in itwinjs-core for
// BackgroundMapGeometry.getFrustumPlaneIntersectionDepthRange. Scenario derived
// from the horizon failure mode the function exists to handle
// (BackgroundMapGeometry.ts:33-55): an orthographic view near edge-on to the
// ground plane, where the depth edges hit z=0 far outside the [0,1] segment.
// The reference (line 38) accepts ANY defined fraction for ortho — including
// extrapolated <0 / >1 — so the depth range extends to bracket z=0 and the grid
// keeps filling at the horizon. A [0,1] clamp (the prior DanQing deviation)
// rejects every edge at the horizon -> null range -> grid vanishes.
TEST(BackgroundMapGeometryDepthRange, OrthoHorizonExtrapolatedFractionsAccumulated)
{
    Frustum const f = MakeHorizonFrustum();
    Plane3dByOriginAndUnitNormal const ground(Point3d{0.0, 0.0, 0.0},
                                              Vector3d{0.0, 0.0, 1.0});
    Range1d const range = getFrustumPlaneIntersectionDepthRange(f, ground);

    // Faithful behavior: extrapolated crossings are accumulated, so the depth
    // range is non-null (brackets z=0) -> grid fills at the horizon.
    EXPECT_FALSE(range.isNull());
    EXPECT_GT(range.Length(), 0.0);
}

// Authored: no reference test exists in itwinjs-core for this function.
// Regression guard — the normal (non-horizon) case where the ground plane cuts
// through the frustum's depth segment. Fractions lie in [0,1] and are accepted
// by BOTH the old clamp and the faithful predicate, so the range is non-null
// before and after the horizon fix.
TEST(BackgroundMapGeometryDepthRange, OrthoGroundCrossesWithinSegment)
{
    // Rigid ortho frustum looking straight down; box straddles z=0 (z from +20
    // at the near/rear face to -20 at the far/front face). Each depth edge
    // crosses z=0 at fraction 0.5 — inside [0,1].
    Frustum f;
    f.points[static_cast<int>(dqCommon::Npc::LeftBottomRear)]    = Point3d{0.0,  0.0,  20.0};
    f.points[static_cast<int>(dqCommon::Npc::RightBottomRear)]   = Point3d{10.0, 0.0,  20.0};
    f.points[static_cast<int>(dqCommon::Npc::LeftTopRear)]       = Point3d{0.0,  10.0, 20.0};
    f.points[static_cast<int>(dqCommon::Npc::RightTopRear)]      = Point3d{10.0, 10.0, 20.0};
    f.points[static_cast<int>(dqCommon::Npc::LeftBottomFront)]   = Point3d{0.0,  0.0, -20.0};
    f.points[static_cast<int>(dqCommon::Npc::RightBottomFront)]  = Point3d{10.0, 0.0, -20.0};
    f.points[static_cast<int>(dqCommon::Npc::LeftTopFront)]      = Point3d{0.0,  10.0, -20.0};
    f.points[static_cast<int>(dqCommon::Npc::RightTopFront)]     = Point3d{10.0, 10.0, -20.0};

    Plane3dByOriginAndUnitNormal const ground(Point3d{0.0, 0.0, 0.0},
                                              Vector3d{0.0, 0.0, 1.0});
    Range1d const range = getFrustumPlaneIntersectionDepthRange(f, ground);

    // The 4 depth edges cross z=0 (fraction 0.5, inside [0,1]) -> accumulated, so
    // the range is non-null. All 4 crossings are coplanar (z=0) under a
    // straight-down view, so the range is a single depth (zero length) — that is
    // valid, not a bug; only null-ness is asserted here.
    EXPECT_FALSE(range.isNull());
}

// Authored: no reference test exists in itwinjs-core for BackgroundMapGeometry.getPlane.
// getPlane(offset) = Plane3dByOriginAndUnitNormal(Point3d(0,0,bias+offset), Vector3d(0,0,1))
// — the background-map ground plane (BackgroundMapGeometry.ts:239-241).
TEST(BackgroundMapGeometryPlane, GetPlaneIsGroundXYThroughElevationBias)
{
    BlankConnectionProps props;
    props.name = "bg-plane";
    props.extents = Range3d::CreateXYZXYZ(-1000.0, -1000.0, -100.0, 1000.0, 1000.0, 100.0);
    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());
    // GlobeMode.Plane → geometry = cartesianPlane（BackgroundMapGeometry.ts:105）。
    BackgroundMapGeometry const geom(10.0, dqCommon::GlobeMode::Plane, conn.Get());  // bimElevationBias = 10 (groundBias)

    Plane3dByOriginAndUnitNormal const p0 = geom.getPlane(0.0);
    EXPECT_NEAR(p0.origin.x, 0.0, 1e-9);     // iModel global origin (x,y) = (0,0)
    EXPECT_NEAR(p0.origin.y, 0.0, 1e-9);
    EXPECT_NEAR(p0.origin.z, 10.0, 1e-9);    // z = bias + 0
    EXPECT_NEAR(p0.normal.z, 1.0, 1e-9);     // iModel +Z (NOT the ecef/cartographic normal)

    Plane3dByOriginAndUnitNormal const ph = geom.getPlane(-1.0);
    EXPECT_NEAR(ph.origin.z, 9.0, 1e-9);     // z = bias + offset
}

// Authored: no reference test exists in itwinjs-core for
// BackgroundMapGeometry.getRayIntersection (BackgroundMapGeometry.ts:243-277).
// 场景锚定 DTA 实测：deco 示例空处悬停 pickDepthPoint 返回 BackgroundMap 源、
// isDefaultDepth=false（2026-09-20 CDP 取证）——本用例锁住该源的求交原语。
TEST(BackgroundMapGeometryRay, PlaneBranchHitsGroundPlane)
{
    BlankConnectionProps props;
    props.name = "bg-ray-plane";
    props.extents = Range3d::CreateXYZXYZ(-1000.0, -1000.0, -100.0, 1000.0, 1000.0, 100.0);
    props.locationIsCartographic = true;
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(-75.0, 40.0, 0.0);
    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());
    BackgroundMapGeometry const geom(0.0, dqCommon::GlobeMode::Plane, conn.Get());

    // 垂直向下：命中 z=0 平面（bias=0），法线 (0,0,1)（:268-274 平面分支）。
    auto const hit = geom.getRayIntersection(
        dqGeom::Ray3d::create(Point3d::From(5.0, 5.0, 10.0), Vector3d::From(0.0, 0.0, -1.0)),
        /*positiveOnly=*/false);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->origin.x, 5.0, 1e-9);
    EXPECT_NEAR(hit->origin.y, 5.0, 1e-9);
    EXPECT_NEAR(hit->origin.z, 0.0, 1e-9);
    EXPECT_NEAR(hit->direction.z, 1.0, 1e-9);

    // positiveOnly=true + 反向（向上）射线：交点在射线身后 → 无命中（:270 门）。
    auto const miss = geom.getRayIntersection(
        dqGeom::Ray3d::create(Point3d::From(5.0, 5.0, 10.0), Vector3d::From(0.0, 0.0, 1.0)),
        /*positiveOnly=*/true);
    EXPECT_FALSE(miss.has_value());
}

// Authored: no reference test exists（同上）。椭球分支的 cartesian 平面修正
// （:259-264）：射线在 cartesianRange 内正面命中椭球 → 修正到 z=0 平面——
// DTA deco 示例（Exton ecef + extents ±1000 展开）空处悬停的实际路径。
TEST(BackgroundMapGeometryRay, EllipsoidBranchCorrectsToCartesianPlaneInRange)
{
    BlankConnectionProps props;
    props.name = "bg-ray-ellipsoid";
    props.extents = Range3d::CreateXYZXYZ(-1.0, -1.0, -1.0, 13.0, 2.0, 2.0);  // deco 示例 extents
    props.locationIsCartographic = true;
    // Exton, PA——DTA openBlankConnection 默认（Surface.ts:185-188）
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(-75.686694, 40.065757, 0.0);
    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());
    BackgroundMapGeometry const geom(0.0, dqCommon::GlobeMode::Ellipsoid, conn.Get());

    // 项目上方垂直向下：椭球命中在 cartesianRange 内 → 修正到 z=0 平面。
    auto const hit = geom.getRayIntersection(
        dqGeom::Ray3d::create(Point3d::From(6.0, 0.5, 50.0), Vector3d::From(0.0, 0.0, -1.0)),
        /*positiveOnly=*/false);
    ASSERT_TRUE(hit.has_value()) << "椭球分支应命中（参考 nearest-fraction 选择）";
    EXPECT_NEAR(hit->origin.x, 6.0, 1e-9);
    EXPECT_NEAR(hit->origin.y, 0.5, 1e-9);
    EXPECT_NEAR(hit->origin.z, 0.0, 1e-6)
        << "cartesian 修正后恒在 z=0 平面（BackgroundMapGeometry.ts:260-263）";
    EXPECT_NEAR(hit->direction.x, 0.0, 1e-9);
    EXPECT_NEAR(hit->direction.y, 0.0, 1e-9);
    EXPECT_NEAR(hit->direction.z, 1.0, 1e-9)
        << "修正后法线 = cartesian 平面法线 (0,0,1)";
}

// Authored: no reference test exists in itwinjs-core for
// BackgroundMapGeometry.getFrustumIntersectionDepthRange. This is the load-bearing
// test for blocker 1 (grid-off → white sky): the PLANAR branch must accumulate
// the extrapolated horizon crossings into a deep, non-null range — the depth the
// sky sphere's u_worldEye needs to stay non-degenerate when grid is off but a
// background map is on (the DTA default). The planar ground planes are z = bias +
// heightRange.{low,high} (BackgroundMapGeometry.ts:310-313), i.e. z ∈ {-1,+1} for
// the blank-connection heightRange (-1,1) at groundBias 0 — essentially the z=0
// ground plane, so the depth range tracks the gridPlane (z=0) intersection.
TEST(BackgroundMapGeometryDepthRange, PlanarBranchYieldsDeepRangeAtHorizon)
{
    BlankConnectionProps props;
    props.name = "bg-planar";
    props.extents = Range3d::CreateXYZXYZ(-1000.0, -1000.0, -100.0, 1000.0, 1000.0, 100.0);
    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());
    Frustum const f = MakeHorizonFrustum();

    // groundBias = 0 (applyTerrain=false default); GlobeMode.Plane → 平面分支。
    BackgroundMapGeometry const geom(0.0, dqCommon::GlobeMode::Plane, conn.Get());
    Range1d const r = geom.getFrustumIntersectionDepthRange(
        f, Range3d::CreateNull(), Range1d::CreateXX(-1.0, 1.0),
        std::nullopt, /*doGlobalScope=*/false);

    // Extrapolated crossings (edges meet z≈0 far forward of the front face) are
    // accumulated → non-null, substantial depth range. This is exactly what was
    // missing when grid was off (the unported globalGeometry branch) and made the
    // sky collapse to white.
    EXPECT_FALSE(r.isNull());
    EXPECT_GT(r.Length(), 50.0);  // well past the <5m → 100m band fallback

    // The planar branch (z=±1) tracks the gridPlane (z=0) intersection — both
    // horizon-extended — confirming the background map substitutes for the grid
    // in the depth-range computation (so grid can be left OFF without shrinking
    // the sky's frustum).
    Range1d const gridRange = getFrustumPlaneIntersectionDepthRange(
        f, Plane3dByOriginAndUnitNormal(Point3d{0.0, 0.0, 0.0}, Vector3d{0.0, 0.0, 1.0}));
    ASSERT_FALSE(gridRange.isNull());
    EXPECT_GT(r.Length(), gridRange.Length() * 0.5);
    EXPECT_LT(r.Length(), gridRange.Length() * 2.0 + 200.0);
}

// Authored: no reference test exists in itwinjs-core for
// DisplayStyleState.getGlobalGeometryAndHeightRange. The visibility gate must
// fire only when an ecef-bearing iModel is present AND viewFlags.backgroundMap
// is on (DisplayStyleState.ts:734-737) — the DTA blank-connection precondition.
TEST(BackgroundMapGeometryGlobalGeometry, VisibleOnlyWhenEcefAndBackgroundMapFlag)
{
    BlankConnectionProps props;
    props.name = "bg-gate";
    props.extents = Range3d::CreateXYZXYZ(-1000.0, -1000.0, -100.0, 1000.0, 1000.0, 100.0);
    props.locationIsCartographic = true;
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(-75.0, 40.0, 0.0);
    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());

    ViewFlagsProperties vpOn;
    vpOn.backgroundMap = true;
    ViewFlags const bgOn(vpOn);
    ViewFlags const bgOff;  // backgroundMap defaults to false

    // No iModel (connectionless fixture) → never visible.
    EXPECT_FALSE(getGlobalGeometryAndHeightRange(nullptr, bgOn).has_value());
    // iModel present but backgroundMap off → not visible.
    EXPECT_FALSE(getGlobalGeometryAndHeightRange(conn.Get(), bgOff).has_value());
    // iModel present + backgroundMap on → visible, ellipsoid geometry + (-1,1) range.
    auto gg = getGlobalGeometryAndHeightRange(conn.Get(), bgOn);
    ASSERT_TRUE(gg.has_value());
    EXPECT_NEAR(gg->heightRange.low, -1.0, 1e-9);
    EXPECT_NEAR(gg->heightRange.high, 1.0, 1e-9);

    // _hasEarthLocation gate (DisplayStyleState.ts:734-735): 无 ECEF 位置的连接
    // 即使 backgroundMap 开启也不可见（参考要求 ecefLocation defined）。
    BlankConnectionProps noEcefProps;
    noEcefProps.name = "bg-no-ecef";
    noEcefProps.extents = Range3d::CreateXYZXYZ(-1000.0, -1000.0, -100.0, 1000.0, 1000.0, 100.0);
    auto noEcefConn = BlankConnection::create(noEcefProps);
    ASSERT_TRUE(noEcefConn.IsValid());
    EXPECT_FALSE(getGlobalGeometryAndHeightRange(noEcefConn.Get(), bgOn).has_value())
        << "no ecefLocation → background map geometry not visible (reference gate)";
}

// Authored: no reference test exists for adjustZPlanes' background-map branch.
// End-to-end (math-level) proof of blocker 1 (grid-off → white sky): with a
// background map visible and grid OFF (the DTA default), the REAL
// ViewingSpace::adjustZPlanes must still produce a deep, horizon-extending
// frustum via the planar background-map branch — not the shallow extents-only
// depth that (before the port, when backgroundMap was ignored) collapsed the
// sky's u_worldEye and made the viewport white. Drives the actual
// BlankConnection → SpatialViewState → ViewingSpace → getFrustum chain.
TEST(BackgroundMapGeometryAdjustZPlanes, BackgroundMapOnGridOffYieldsDeepFrustum)
{
    BlankConnectionProps props;
    props.name = "bg-adjust";
    props.extents = Range3d::CreateXYZXYZ(-1000.0, -1000.0, -100.0, 1000.0, 1000.0, 100.0);
    props.locationIsCartographic = true;
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(-75.0, 40.0, 0.0);
    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());

    auto const ext = conn->GetProjectExtents();

    // Frustum view-Z depth (the quantity adjustZPlanes' delta.z sets) for a given
    // (backgroundMap, grid) flag pair, Iso rotation. This is exactly the depth
    // ComputeSkySphereWorldPosAndEye consumes to place the sky's u_worldEye.
    auto depthFor = [&](bool bg, bool grid) -> double {
        auto view = SpatialViewState::CreateBlank(
            conn.Get(), ext.low, ext.Diagonal(), StandardView::Iso());
        auto& style = view->GetDisplayStyle();
        auto vp = style.getViewFlags().Properties();
        vp.backgroundMap = bg;
        vp.grid = grid;
        style.setViewFlags(ViewFlags(vp));

        ViewingSpace vs;
        ViewRect rect{0, 0, 800, 600};
        vs.update(*view->AsViewState3d(), rect);

        Frustum fw;
        vs.getFrustum(fw, CoordSystem::World);
        auto const rotOpt = fw.getRotation();
        if (!rotOpt.has_value()) return -1.0;
        Vector3d const viewZ = rotOpt->RowZ();
        double dmin = 1e300, dmax = -1e300;
        for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
            double const d = viewZ.DotProduct(fw.points[i]);
            dmin = std::min(dmin, d);
            dmax = std::max(dmax, d);
        }
        return dmax - dmin;
    };

    double const bgOnGridOff  = depthFor(true,  false);
    double const bgOffGridOff = depthFor(false, false);
    double const bgOffGridOn  = depthFor(false, true);

    std::cout << "  adjustZPlanes depth (Iso): bgOnGridOff=" << bgOnGridOff
              << " bgOffGridOff=" << bgOffGridOff
              << " bgOffGridOn=" << bgOffGridOn << "\n";

    // THE FIX: background-map is no longer ignored — grid-off-with-bg is strictly
    // deeper than the extents-only path (bg off, grid off), which is the case
    // that collapsed the sky's u_worldEye and drew the viewport white before the
    // port. (Before the port these two were equal: backgroundMap had no effect.)
    ASSERT_GT(bgOffGridOff, 0.0);  // sanity: extents-only depth is positive
    EXPECT_GT(bgOnGridOff, bgOffGridOff);

    // Grid-off-with-bg carries the depth at least as well as grid-on-without-bg
    // (the known-good case that rendered the sky correctly). The background map
    // now substitutes for the grid plane in the depth-range computation, so grid
    // can default OFF (DTA-faithful) without shrinking the sky's frustum.
    ASSERT_GT(bgOffGridOn, 0.0);
    EXPECT_GE(bgOnGridOff, bgOffGridOn * 0.95);
}
