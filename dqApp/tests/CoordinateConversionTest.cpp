// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/ViewingSpace.ts
//              ViewingSpace.npcToView (L404-407), viewToNpc (L393-398),
//              worldToView (L439), viewToWorld (L451)
// Ported from: itwinjs-core core/frontend/src/Viewport.ts
//              pixelsFromInches (L2099), pixelsPerInch (L1441-1444 = 96)
// Ported from: itwinjs-core core/frontend/src/tools/ToolAdmin.ts
//              CurrentInputState.fromPoint (L280-288),
//              changeButtonToDownPoint (L201-207)
// Authored: the exact-value assertions below are derived from the ortho
//           projection math (world -> NDC[-1,1] -> view pixels), not from a
//           specific itwinjs test. They lock the [0,1]-NPC <-> view <-> world
//           round-trip that the InputState coordinate wiring (Task 2) depends on.

#include "QtAppFixture.h"  // qtApp() — shared QApplication fixture (tests/ only)

#include <dqApp/ToolAdmin.h>  // InputState, InputSource, BeButton, BeButtonEvent
#include <dqApp/Viewport.h>
#include <dqApp/ViewingSpace.h>
#include <dqApp/ViewState.h>

#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <gtest/gtest.h>

using dqApp::InputSource;
using dqApp::InputState;
using dqApp::SpatialViewState;
using dqApp::Viewport;
using dqApp::ViewRect;
using dqApp::ViewingSpace;
using dqGeom::Point2d;
using dqGeom::Point3d;
using dqGeom::Vector3d;

namespace {
// Build a ViewingSpace for a 200^3 ortho view in an 800x600 rect. The frustum
// origin is its lower-left-rear CORNER (itwinjs convention) and extents are the
// box edge lengths, so the frustum spans world [0,200]^3 → NPC [0,1]^3 → view
// x in [0,800], view y in [600,0] (Y flipped), view z in [-32767,32767].
ViewingSpace MakeOrthoSpace()
{
    auto view = SpatialViewState::CreateBlank(
        nullptr, Point3d::From(0, 0, 0), Vector3d::From(200, 200, 200));
    ViewingSpace vs;
    ViewRect rect{0, 0, 800, 600};
    vs.update(*view->AsViewState3d(), rect);
    return vs;
}

void ExpectPoint(Point3d const& q, double x, double y, double z)
{
    EXPECT_NEAR(q.x, x, 1e-3);
    EXPECT_NEAR(q.y, y, 1e-3);
    EXPECT_NEAR(q.z, z, 1e-3);
}
}  // namespace

// ViewingSpace.npcToView maps the itwinjs [0,1]^3 NPC cube to view pixels via
// getViewCorners().fractionToPoint. Ported from ViewingSpace.npcToView
// (ViewingSpace.ts:404-407). low=(0,H,-32767), high=(W,0,32767): Y is flipped
// on screen; z spans ±32767 (the literal integer, not 32768/0x7FFF).
TEST(ViewingSpaceCoord, NpcToViewMapsCenterAndCorners)
{
    ViewingSpace vs = MakeOrthoSpace();
    ExpectPoint(vs.NpcToView(Point3d::From(0.5, 0.5, 0.5)), 400.0, 300.0, 0.0);      // center
    ExpectPoint(vs.NpcToView(Point3d::From(0.0, 0.0, 0.0)), 0.0, 600.0, -32767.0);   // low corner
    ExpectPoint(vs.NpcToView(Point3d::From(1.0, 1.0, 1.0)), 800.0, 0.0, 32767.0);    // high corner
}

// ViewingSpace.viewToNpc is the inverse of npcToView.
TEST(ViewingSpaceCoord, ViewToNpcInvertsNpcToView)
{
    ViewingSpace vs = MakeOrthoSpace();
    Point3d const npc = Point3d::From(0.25, 0.75, 0.5);
    Point3d const back = vs.ViewToNpc(vs.NpcToView(npc));
    ExpectPoint(back, npc.x, npc.y, npc.z);
}

// The world view-center (origin + rotation^T·extents/2 == NPC center) projects
// to the same view point as NpcToView(NpcCenter): worldToView and npcToView
// share the view-pixel space. (The frustum origin is a CORNER, not the center —
// WorldToView(origin) == NpcToView((0,0,0)) == the low corner.)
TEST(ViewingSpaceCoord, WorldToViewCenterMatchesNpcToViewCenter)
{
    ViewingSpace vs = MakeOrthoSpace();
    // View-center world point = origin + extents/2 (identity rotation here).
    Point3d const worldCenter = Point3d::From(100.0, 100.0, 100.0);
    Point3d const wcenter = vs.WorldToView(worldCenter);
    Point3d const vcenter = vs.NpcToView(Point3d::From(0.5, 0.5, 0.5));
    ExpectPoint(wcenter, vcenter.x, vcenter.y, vcenter.z);
}

// viewToWorld inverts worldToView.
TEST(ViewingSpaceCoord, ViewToWorldInvertsWorldToView)
{
    ViewingSpace vs = MakeOrthoSpace();
    Point3d const world = Point3d::From(37.0, -52.0, 11.0);
    Point3d const back = vs.ViewToWorld(vs.WorldToView(world));
    ExpectPoint(back, world.x, world.y, world.z);
}

// Viewport.pixelsFromInches = inches * pixelsPerInch, pixelsPerInch = 96
// (itwinjs Viewport.ts:2099 / 1441-1444).
TEST(ViewportPixels, FromInchesIs96PerInch)
{
    qtApp();
    auto view = SpatialViewState::CreateBlank(
        nullptr, Point3d::From(0, 0, 0), Vector3d::From(200, 200, 200));
    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);

    EXPECT_NEAR(vp->PixelsFromInches(1.0), 96.0, 1e-6);
    EXPECT_NEAR(vp->PixelsFromInches(0.15), 14.4, 1e-6);  // startDragDistanceInches

    delete vp;
}

// InputState.fromPoint with a real viewport converts a screen-space cursor to a
// world rawPoint via viewToWorld, and sets viewPoint.z to the view-center depth
// (npcToView(NpcCenter).z). Ported from ToolAdmin.ts:280-288.
TEST(InputStateCoord, FromPointConvertsScreenToWorldWithViewport)
{
    qtApp();
    auto view = SpatialViewState::CreateBlank(
        nullptr, Point3d::From(0, 0, 0), Vector3d::From(200, 200, 200));
    Viewport* vp = Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    vp->resize(800, 600);
    // Populate ViewingSpace (SetupFromView runs inside setupViewFromFrustum).
    auto frustum = vp->getFrustum(true);
    vp->setupViewFromFrustum(frustum);

    InputState st;
    st.fromPoint(vp, Point2d::From(400.0, 300.0), InputSource::Mouse);

    // viewPoint.z == npcToView(NpcCenter).z (the view-center depth).
    double const centerZ = vp->NpcToView(Point3d::From(0.5, 0.5, 0.5)).z;
    EXPECT_NEAR(st.viewPoint().z, centerZ, 1e-3);
    // rawPoint == ViewToWorld(viewPoint) (the world point under the cursor).
    Point3d const expectedRaw = vp->ViewToWorld(st.viewPoint());
    ExpectPoint(st.rawPoint(), expectedRaw.x, expectedRaw.y, expectedRaw.z);
    // The world rawPoint round-trips back to the cursor through WorldToView
    // (geometry-independent confirmation that ViewToWorld/WorldToView are
    // consistent inverses through fromPoint).
    Point3d const roundTrip = vp->WorldToView(st.rawPoint());
    EXPECT_NEAR(roundTrip.x, st.viewPoint().x, 0.5);
    EXPECT_NEAR(roundTrip.y, st.viewPoint().y, 0.5);
    EXPECT_NEAR(roundTrip.z, st.viewPoint().z, 1e-3);
    // The world rawPoint differs from the screen viewPoint — a real conversion
    // happened, not the identity stub.
    EXPECT_NE(st.rawPoint().x, st.viewPoint().x);

    delete vp;
}
