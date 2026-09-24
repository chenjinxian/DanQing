// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/ViewState.ts
//              ViewState.calculateFrustum()        (lines 703-715)
//              ViewState.setupFromFrustum()        (lines 734-785)
//              ViewState3d.moveCameraWorld()       (lines 1968-1981)
//              ViewState3d.is3d()                  (line 1505)
//              ViewState3d.allow3dManipulations()  (lines 1433-1435)
// Ported from: itwinjs-core core/frontend/src/Viewport.ts
//              Viewport.getFrustum()               (line 2113)
//              Viewport.getWorldFrustum()          (line 2116)
//              Viewport.setupViewFromFrustum()     (line 2289)
//              Viewport.scroll()                   (lines 2121-2141)
//              Viewport.isCameraOn                 (line 1741)
//              ScreenViewport.viewRect             (line 3505)
//              Viewport.pickDepthPoint()           (line 3394)
// Authored: no reference test exists in itwinjs-core/imodel-native for a
//           ViewState3d.GetFrustum ↔ SetupFromFrustum round-trip gate or
//           for moveCameraWorld frustum-shift assertions; the values below
//           are derived from origin/extents/rotation identity (the canonical
//           round-trip correctness gate described in the itwinjs methods).
//           The ViewportFrustum.* tests below verify DanQing glue: the thin
//           Viewport wrappers delegate to the Task-3 ViewState3d API, so the
//           assertions mirror the FrustumApi.* invariants through the
//           Viewport boundary.
#include <gtest/gtest.h>

#include <dqApp/ViewState.h>
#include <dqApp/StandardView.h>
#include <dqApp/Viewport.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/Npc.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <QApplication>

using dqApp::SpatialViewState;
using dqApp::Viewport;
using dqCommon::Frustum;
using dqCommon::Npc;

// Ported from: itwinjs-core ViewState.calculateFrustum + setupFromFrustum
// Round-trip gate: GetFrustum → SetupFromFrustum → GetFrustum must reproduce
// the same 8 corners to within Frustum::isSame() tolerance.
TEST(FrustumApi, GetFrustumRoundTripsThroughSetupFromFrustum)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(100, 100, 100));

    Frustum f1, f2;
    view->GetFrustum(f1);
    ASSERT_TRUE(view->SetupFromFrustum(f1));
    view->GetFrustum(f2);
    EXPECT_TRUE(f1.isSame(f2));
}

// Ported from: itwinjs-core ViewState.calculateFrustum + setupFromFrustum
// Round-trip gate with a non-zero origin (verifies origin != corner case).
TEST(FrustumApi, GetFrustumRoundTripsWithNonZeroOrigin)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(1000, -500, 250),
        dqGeom::Vector3d::From(200, 150, 80));

    Frustum f1, f2;
    view->GetFrustum(f1);
    ASSERT_TRUE(view->SetupFromFrustum(f1));
    view->GetFrustum(f2);
    EXPECT_TRUE(f1.isSame(f2));
}

// Ported from: itwinjs-core ViewState3d.moveCameraWorld (ViewState.ts:1972-1981)
// Orthographic branch: moves origin by distance; frustum center must follow
// by the same delta.
TEST(FrustumApi, MoveCameraWorldShiftsFrustumCenter)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(100, 100, 100));

    EXPECT_FALSE(view->IsCameraOn());  // orthographic by default

    Frustum fBefore;
    view->GetFrustum(fBefore);
    auto centerBefore = fBefore.getCenter();

    dqGeom::Vector3d dist = dqGeom::Vector3d::From(50, -25, 10);
    view->MoveCameraWorld(dist);

    Frustum fAfter;
    view->GetFrustum(fAfter);
    auto centerAfter = fAfter.getCenter();

    EXPECT_NEAR(centerAfter.x, centerBefore.x + dist.x, 1e-9);
    EXPECT_NEAR(centerAfter.y, centerBefore.y + dist.y, 1e-9);
    EXPECT_NEAR(centerAfter.z, centerBefore.z + dist.z, 1e-9);
}

// Ported from: itwinjs-core ViewState3d.moveCameraWorld (ViewState.ts:1972-1981)
// Verifies that the view origin moves by exactly the supplied delta.
TEST(FrustumApi, MoveCameraWorldShiftsOrigin)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(10, 20, 30),
        dqGeom::Vector3d::From(100, 100, 100));

    auto originBefore = view->GetOrigin();

    dqGeom::Vector3d dist = dqGeom::Vector3d::From(5, -7, 2);
    view->MoveCameraWorld(dist);

    auto originAfter = view->GetOrigin();
    EXPECT_NEAR(originAfter.x, originBefore.x + dist.x, 1e-9);
    EXPECT_NEAR(originAfter.y, originBefore.y + dist.y, 1e-9);
    EXPECT_NEAR(originAfter.z, originBefore.z + dist.z, 1e-9);
}

// Ported from: itwinjs-core ViewState3d.is3d (ViewState.ts:1505) +
//              ViewState3d.allow3dManipulations (ViewState.ts:1433-1435)
TEST(FrustumApi, Is3dAndAllow3dManipulationsForSpatialView)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(100, 100, 100));

    EXPECT_TRUE(view->is3d());
    EXPECT_TRUE(view->Allow3dManipulations());
}

// Ported from: itwinjs-core ViewingSpace.getFrustum (sys=CoordSystem.View)
// includeOrientation=false returns the view-local frustum (axis-aligned NPC
// cube scaled by extents, no rotation, no translation).
TEST(FrustumApi, GetFrustumWithoutOrientationIsAxisAligned)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(1000, 2000, 3000),
        dqGeom::Vector3d::From(100, 200, 50));

    // Apply a non-trivial rotation (Front view) so we can verify the rotation
    // is dropped when includeOrientation=false.
    view->SetStandardView(static_cast<int>(dqApp::StandardViewId::Front));

    Frustum fWorld, fView;
    view->GetFrustum(fWorld, true);
    view->GetFrustum(fView, false);

    // View-local frustum must be the axis-aligned extents box [0..extents].
    auto const& lbr = fView.getCorner(Npc::LeftBottomRear);
    auto const& rtf = fView.getCorner(Npc::RightTopFront);
    EXPECT_NEAR(lbr.x, 0.0, 1e-9);
    EXPECT_NEAR(lbr.y, 0.0, 1e-9);
    EXPECT_NEAR(lbr.z, 0.0, 1e-9);
    EXPECT_NEAR(rtf.x, 100.0, 1e-9);
    EXPECT_NEAR(rtf.y, 200.0, 1e-9);
    EXPECT_NEAR(rtf.z, 50.0, 1e-9);
}

// Ported from: itwinjs-core ViewState.computeWorldToNpc (ViewState.ts:625-701) +
//              ViewState.calculateFrustum (ViewState.ts:707-715).
// Authored: no reference test exists in itwinjs-core/imodel-native for
//           ViewState.computeWorldToNpc directly (the perspective primitive
//           Map4d.createVectorFrustum is covered by t_DMap4d.cpp:129
//           VectorFrustumExample, ported in Map4dTest). The assertions pin the
//           contract from the reference definition: the worldToNpc map's
//           transform1 maps the NPC cube to the view volume
//           [origin, origin + rotationᵀ·extents] (ViewState.ts:692-700), the
//           map carries an inverse pair (transform0·transform1 = I), and
//           calculateFrustum reproduces those same corners.
TEST(FrustumApi, ComputeWorldToNpcMapsNpcCubeToViewVolume)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(100, -50, 25),
        dqGeom::Vector3d::From(200, 120, 80));
    // Non-trivial rotation so rotationᵀ ≠ identity.
    view->SetStandardView(static_cast<int>(dqApp::StandardViewId::Front));

    auto const result = view->computeWorldToNpc();
    ASSERT_TRUE(result.map.has_value());
    EXPECT_NEAR(result.frustFraction, 1.0, 1e-12);  // ortho → fraction 1.0

    auto const& map = *result.map;
    auto const origin = view->GetOrigin();
    auto const extents = view->GetExtents();
    auto const& rotation = view->getRotation();

    Frustum npc;
    npc.initNpc();
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        // transform1 (npcToWorld): NPC cube corner → world frustum corner.
        dqGeom::Point3d const world = map.Transform1().MultiplyPoint3dQuietNormalize(npc.points[i]);
        // Expected: origin + rotationᵀ · (extents · npc)  (ViewState.ts:692-700).
        dqGeom::Vector3d const scaled = dqGeom::Vector3d::From(
            extents.x * npc.points[i].x,
            extents.y * npc.points[i].y,
            extents.z * npc.points[i].z);
        dqGeom::Vector3d const rotated = rotation.MultiplyTransposeVector(scaled);
        EXPECT_NEAR(world.x, origin.x + rotated.x, 1e-6);
        EXPECT_NEAR(world.y, origin.y + rotated.y, 1e-6);
        EXPECT_NEAR(world.z, origin.z + rotated.z, 1e-6);
    }

    // transform0 · transform1 = identity (Map4d carries an inverse pair).
    auto const product = map.Transform0().MultiplyMatrixMatrix(map.Transform1());
    EXPECT_TRUE(product.IsIdentity(1.0e-9));

    // calculateFrustum produces the same corners as transform1 on the NPC cube.
    Frustum calc;
    ASSERT_TRUE(view->calculateFrustum(calc));
    for (int i = 0; i < dqCommon::kNpcCornerCount; ++i) {
        dqGeom::Point3d const w = map.Transform1().MultiplyPoint3dQuietNormalize(npc.points[i]);
        EXPECT_NEAR(calc.points[i].x, w.x, 1e-6);
        EXPECT_NEAR(calc.points[i].y, w.y, 1e-6);
        EXPECT_NEAR(calc.points[i].z, w.z, 1e-6);
    }
}

// ---------------------------------------------------------------------------
// Viewport frustum / view wrappers (Task 4)
// Ported from: itwinjs-core core/frontend/src/Viewport.ts
//              getFrustum / getWorldFrustum / setupViewFromFrustum / scroll /
//              isCameraOn / viewRect / pickDepthPoint.
//
// Viewport derives from QWidget, so constructing one requires a QApplication.
// gtest_main does not create one; register a global Environment that does.
// AddGlobalTestEnvironment runs at static-init time, before main, and its
// SetUp() is invoked by gtest after InitGoogleTest.
// ---------------------------------------------------------------------------
namespace {
int g_qtArgc = 1;
char g_qtArg0[] = "dqAppTest";
char* g_qtArgv[] = {g_qtArg0, nullptr};
QApplication* g_qApp = nullptr;

class QApplicationEnv : public ::testing::Environment {
public:
    void SetUp() override {
        // Guard on the Qt-global instance, not a private flag: per-TU QtEnv
        // statics (WindowAreaTest/GridAppDiag/ViewUndoTest/SkyRotateDiag/
        // StandardViewAnimationTest/LookToolTest) may already have created the
        // process QApplication before main. A private-flag guard here would
        // construct a second application object → Qt assert "there should be
        // only one application object" (QCoreApplication, debug builds).
        if (qApp == nullptr) g_qApp = new QApplication(g_qtArgc, g_qtArgv);
    }
};
::testing::Environment* const g_envReg =
    ::testing::AddGlobalTestEnvironment(new QApplicationEnv);
}  // namespace

// Ported from: itwinjs-core Viewport.getFrustum (Viewport.ts:2113) +
//              ScreenViewport.viewRect (Viewport.ts:3505) +
//              Viewport.isCameraOn (Viewport.ts:1741)
// Verifies the thin Viewport wrappers mirror the underlying ViewState3d, that
// viewRect tracks the QWidget dimensions, and that isCameraOn reports the
// ViewState3d camera state.
// Authored: Viewport wrappers are DanQing glue — assertions mirror the
//           FrustumApi.GetFrustumWithoutOrientationIsAxisAligned invariants
//           through the Viewport boundary (no reference test for the wrapper
//           layer itself in itwinjs-core/imodel-native).
TEST(ViewportFrustum, GetFrustumMirrorsView)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(200, 200, 200));

    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    dqCommon::Frustum fv = vp->getFrustum(true);
    dqCommon::Frustum fview;
    view->GetFrustum(fview, true);
    EXPECT_TRUE(fv.isSame(fview));

    // viewRect() = {0, 0, width(), height()}. QWidget default-initializes to a
    // 640x480 geometry (Qt's default top-level widget size); we don't assume a
    // specific default, only that viewRect tracks QWidget::width()/height().
    EXPECT_EQ(vp->viewRect().left, 0);
    EXPECT_EQ(vp->viewRect().top, 0);
    EXPECT_EQ(vp->viewRect().right, vp->width());
    EXPECT_EQ(vp->viewRect().bottom, vp->height());

    // Default ViewState3d has camera off.
    EXPECT_FALSE(vp->isCameraOn());

    delete vp;
}

// Ported from: itwinjs-core Viewport.getWorldFrustum (Viewport.ts:2116)
// getWorldFrustum() must equal getFrustum(true).
TEST(ViewportFrustum, GetWorldFrustumEqualsGetFrustumWorld)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(100, -50, 25),
        dqGeom::Vector3d::From(120, 80, 200));

    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    dqCommon::Frustum fw = vp->getWorldFrustum();
    dqCommon::Frustum fworld = vp->getFrustum(true);
    EXPECT_TRUE(fw.isSame(fworld));

    delete vp;
}

// Ported from: itwinjs-core Viewport.setupViewFromFrustum (Viewport.ts:2289)
// Round-trip through getFrustum → setupViewFromFrustum → getFrustum must
// reproduce the same corners to within Frustum::isSame() tolerance, PROVIDED
// the view extents already match the widget's aspect ratio (otherwise the
// SetupFromView → FixAspectRatio step inside setupViewFromFrustum will adjust
// extents.y, faithfully mirroring itwinjs which always calls setupFromView).
TEST(ViewportFrustum, SetupViewFromFrustumRoundTrips)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(250, 500, -100),
        dqGeom::Vector3d::From(180, 90, 60));

    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    // Match view extents.y to the widget's aspect ratio so FixAspectRatio is a
    // no-op (this isolates the round-trip from the aspect-adjustment step).
    float const w = static_cast<float>(vp->width());
    float const h = static_cast<float>(vp->height());
    ASSERT_GT(w, 0.0f);
    ASSERT_GT(h, 0.0f);
    double const xExt = 180.0;
    view->SetExtents(dqGeom::Vector3d::From(xExt, xExt * h / w, 60.0));

    dqCommon::Frustum f1 = vp->getFrustum(true);
    EXPECT_TRUE(vp->setupViewFromFrustum(f1));
    dqCommon::Frustum f2 = vp->getFrustum(true);
    EXPECT_TRUE(f1.isSame(f2));

    delete vp;
}

// Ported from: itwinjs-core Viewport.scroll (Viewport.ts:2121-2141)
// Orthographic branch (camera off): the simplified DanQing wrapper translates
// the view origin by `dist` in world coordinates. Verify origin moves by
// exactly the supplied delta.
TEST(ViewportFrustum, ScrollTranslatesOrigin)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(10, 20, 30),
        dqGeom::Vector3d::From(100, 100, 100));

    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    auto originBefore = view->GetOrigin();
    dqGeom::Vector3d dist = dqGeom::Vector3d::From(15, -7, 4);
    vp->scroll(dist);
    auto originAfter = view->GetOrigin();

    EXPECT_NEAR(originAfter.x, originBefore.x + dist.x, 1e-9);
    EXPECT_NEAR(originAfter.y, originBefore.y + dist.y, 1e-9);
    EXPECT_NEAR(originAfter.z, originBefore.z + dist.z, 1e-9);

    delete vp;
}

// Ported from: itwinjs-core ViewState3d.getRotation (used by Viewport
//              machinery; the Viewport wrapper is DanQing glue for tool access).
// For a ViewState3d, getRotation() must return the view's rotation matrix;
// identity for a freshly-created blank spatial view.
TEST(ViewportFrustum, GetRotationReturnsViewRotation)
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(100, 100, 100));

    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    dqGeom::Matrix3d rot = vp->getRotation();
    dqGeom::Matrix3d const& viewRot = view->getRotation();
    for (int i = 0; i < 9; ++i) {
        EXPECT_NEAR(rot.coffs[i], viewRot.coffs[i], 1e-9);
    }

    delete vp;
}

// Ported from: itwinjs-core Viewport.pickDepthPoint (Viewport.ts:3435-3503)
//              — 真实现后的回退链：无渲染目标时落到默认 ACS 平面（z=0）。
// Authored: no reference test exists（参考的 pickDepthPoint 依赖 WebGL pick
//           缓冲——无参考单测；行为锚定 Viewport.ts:3496-3500 ACS 分支）。
TEST(ViewportFrustum, PickDepthPointFallsBackToAcsPlane)
{
    // 真实现（Viewport.ts:3435-3503 移植，2026-09-19）：无渲染目标（无 pick 回读）
    // 时走回退链——默认 ACS 平面（z=0 过原点）。正交 Top 视图下从 (1,2,3) 沿
    // 视线方向交 z=0 → (1,2,0)，source=ACS。
    auto view = SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(100, 100, 100));

    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    auto const result = vp->pickDepthPoint(dqGeom::Point3d::From(1, 2, 3), 5.0);
    EXPECT_NEAR(result.origin.x, 1.0, 1e-9);
    EXPECT_NEAR(result.origin.y, 2.0, 1e-9);
    EXPECT_NEAR(result.origin.z, 0.0, 1e-9);
    EXPECT_EQ(result.source, dqApp::DepthPointSource::ACS);

    delete vp;
}
