// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — BlankConnection + IModelConnection tests
// Ported from: itwinjs-core core/frontend/src/test/BlankConnection.test.ts
//              itwinjs-core core/frontend/src/test/SpatialViewState.test.ts
#include <gtest/gtest.h>

#include <dqApp/BlankConnection.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/DisplayStyle.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/ViewFlags.h>
#include <dqCommon/RenderMode.h>

using namespace dqApp;

// ---------------------------------------------------------------------------
// IModelConnection tests
// ---------------------------------------------------------------------------
// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for base
//           property access (the reference file tests only name preservation + onOpen).
//           Asserts the isClosed/isOpen contract from the reference implementation:
//           BlankConnection.isClosed always returns true (IModelConnection.ts:777-781),
//           so isOpen (= !isClosed, IModelConnection.ts:133) is always false — that is
//           what short-circuits every RPC/ECSQL path for a blank connection.
TEST(IModelConnectionTest, BlankConnectionIsIModelConnection)
{
    BlankConnectionProps props;
    props.name = "test";
    props.extents = dqGeom::Range3d::CreateXYZXYZ(-500, -500, -50, 500, 500, 50);

    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());

    // Verify IModelConnection base properties
    EXPECT_EQ(conn->GetName(), "test");
    EXPECT_EQ(conn->GetRootSubject(), "test");
    EXPECT_TRUE(conn->IsClosed());   // IModelConnection.ts:781 — always true for blank
    EXPECT_FALSE(conn->IsOpen());    // IModelConnection.ts:133 — isOpen = !isClosed
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for the
//           type guards; asserts the overrides at IModelConnection.ts:767
//           (isBlankConnection → true) and the base defaults (:102-113).
TEST(IModelConnectionTest, TypeGuards)
{
    auto conn = BlankConnection::create({});
    ASSERT_TRUE(conn.IsValid());

    EXPECT_TRUE(conn->IsBlankConnection());
    EXPECT_TRUE(conn->IsBlank());
    EXPECT_FALSE(conn->IsSnapshotConnection());
    EXPECT_FALSE(conn->IsBriefcaseConnection());
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for
//           projectExtents; asserts the props pass-through at IModelConnection.ts:790.
TEST(IModelConnectionTest, ProjectExtents)
{
    BlankConnectionProps props;
    props.name = "extents-test";
    props.extents = dqGeom::Range3d::CreateXYZXYZ(-1000, -1000, -100, 1000, 1000, 100);

    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());

    auto const& ext = conn->GetProjectExtents();
    EXPECT_DOUBLE_EQ(ext.low.x, -1000.0);
    EXPECT_DOUBLE_EQ(ext.low.y, -1000.0);
    EXPECT_DOUBLE_EQ(ext.low.z, -100.0);
    EXPECT_DOUBLE_EQ(ext.high.x, 1000.0);
    EXPECT_DOUBLE_EQ(ext.high.y, 1000.0);
    EXPECT_DOUBLE_EQ(ext.high.z, 100.0);
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for
//           globalOrigin; asserts the props pass-through at IModelConnection.ts:791.
TEST(IModelConnectionTest, GlobalOrigin)
{
    BlankConnectionProps props;
    props.name = "origin-test";
    props.extents = dqGeom::Range3d::CreateXYZXYZ(-100, -100, -10, 100, 100, 10);
    props.globalOrigin = {50, 60, 70};

    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());

    auto const& orig = conn->GetGlobalOrigin();
    EXPECT_DOUBLE_EQ(orig.x, 50.0);
    EXPECT_DOUBLE_EQ(orig.y, 60.0);
    EXPECT_DOUBLE_EQ(orig.z, 70.0);
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for close
//           lifecycle; asserts the reference close contract:
//           - BlankConnection.isClosed is ALWAYS true (IModelConnection.ts:777-781),
//             including before close() is ever called;
//           - close() is still valid to call and raises the close events
//             (IModelConnection.ts:779 doc + :804-806 close → beforeClose).
TEST(IModelConnectionTest, CloseLifecycle)
{
    auto conn = BlankConnection::create({});
    ASSERT_TRUE(conn.IsValid());
    // A BlankConnection is born closed (IModelConnection.ts:781).
    EXPECT_TRUE(conn->IsClosed());
    EXPECT_FALSE(conn->IsOpen());

    bool closeEventFired = false;
    dqBase::DqEventScope scope;  // RAII 断连（AddListener 返回断连令牌，析构不清——误作 scope 会残留悬空 listener）
    scope.add(conn->OnCloseInstance.AddListener(
        [&closeEventFired](IModelConnection*) { closeEventFired = true; }
    ));

    conn->Close();
    EXPECT_TRUE(conn->IsClosed());
    EXPECT_FALSE(conn->IsOpen());
    EXPECT_TRUE(closeEventFired);
}

// Ported from: itwinjs-core core/frontend/src/test/BlankConnection.test.ts:25-34
//              it("raises `onOpen` event when a new `BlankConnection` is created")
TEST(IModelConnectionTest, StaticOnOpenEvent)
{
    bool openEventFired = false;
    dqBase::DqEventScope scope;  // RAII 断连——静态事件 OnOpen 进程级残留，漏断连会让后续测试触发悬空 lambda（UB）
    scope.add(IModelConnection::OnOpen.AddListener(
        [&openEventFired](IModelConnection*) { openEventFired = true; }
    ));

    auto conn = BlankConnection::create({});
    EXPECT_TRUE(openEventFired);
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for
//           repeated close. Reference contract: BlankConnection.close() calls
//           beforeClose() UNCONDITIONALLY (IModelConnection.ts:804-806) — there is no
//           isClosed guard, unlike SnapshotConnection.close (IModelConnection.ts:872-884).
//           Each Close() therefore re-raises the close events.
TEST(IModelConnectionTest, CloseIsNotGuardedForBlankConnection)
{
    auto conn = BlankConnection::create({});
    int closeCount = 0;
    dqBase::DqEventScope scope;
    scope.add(conn->OnCloseInstance.AddListener(
        [&closeCount](IModelConnection*) { closeCount++; }
    ));

    conn->Close();
    conn->Close();  // unguarded in the reference — raises again
    EXPECT_EQ(closeCount, 2);
}

// ---------------------------------------------------------------------------
// BlankConnection tests (updated API)
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/frontend/src/test/BlankConnection.test.ts:15-23
//              it("preserves name")
TEST(BlankConnectionTest, CreateWithName)
{
    BlankConnectionProps props;
    props.name = "test";
    props.extents = dqGeom::Range3d::CreateXYZXYZ(-50, -100, -10, 50, 100, 10);

    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());
    EXPECT_EQ(conn->GetName(), "test");
    EXPECT_TRUE(conn->IsBlank());
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for
//           default-constructed props. BlankConnectionProps.name is required in the
//           reference (IModelConnection.ts:48); DanQing's C++ struct adds a "blank"
//           convenience default (all production callers pass a name explicitly).
TEST(BlankConnectionTest, CreateDefault)
{
    auto conn = BlankConnection::create({});
    ASSERT_TRUE(conn.IsValid());
    EXPECT_EQ(conn->GetName(), "blank");
}

// Authored: no reference test exists in itwinjs-core BlankConnection.test.ts for
//           cartographic location; asserts the EcefLocation.createFromCartographicOrigin
//           branch at IModelConnection.ts:792.
TEST(BlankConnectionTest, CartographicLocation)
{
    BlankConnectionProps props;
    props.name = "geo-test";
    props.extents = dqGeom::Range3d::CreateXYZXYZ(-500, -500, -50, 500, 500, 50);
    props.locationIsCartographic = true;
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(116.4, 39.9, 50.0);  // Beijing

    auto conn = BlankConnection::create(props);
    ASSERT_TRUE(conn.IsValid());
    EXPECT_EQ(conn->GetName(), "geo-test");

    // EcefLocation should have been created from Cartographic
    auto const& ecef = conn->GetEcefLocation();
    // ECEF origin should be on Earth's surface (~6.37e6 meters from center)
    double dist = std::sqrt(ecef.origin.x * ecef.origin.x +
                            ecef.origin.y * ecef.origin.y +
                            ecef.origin.z * ecef.origin.z);
    EXPECT_GT(dist, 6.0e6);   // > 6000 km from Earth center
    EXPECT_LT(dist, 7.0e6);   // < 7000 km from Earth center
}

// ---------------------------------------------------------------------------
// SpatialViewState tests
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/frontend/src/test/SpatialViewState.test.ts
//              TEST(SpatialViewStateTest, CreateBlank)
TEST(SpatialViewStateTest, CreateBlank)
{
    dqGeom::Point3d origin = {0, 0, 0};
    dqGeom::Vector3d extents = {1000, 1000, 100};

    auto view = SpatialViewState::CreateBlank(nullptr, origin, extents);
    ASSERT_TRUE(view.IsValid());

    EXPECT_DOUBLE_EQ(view->GetOrigin().x, 0.0);
    EXPECT_DOUBLE_EQ(view->GetOrigin().y, 0.0);
    EXPECT_DOUBLE_EQ(view->GetOrigin().z, 0.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().x, 1000.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().y, 1000.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().z, 100.0);
}

// Ported from: itwinjs-core core/frontend/src/SpatialViewState.ts:77-88 (createBlank raw
//              defaults). CreateBlank must NOT apply display-test-app overrides — those
//              live in manufactureSpatialView (ViewPicker.ts:149-169).
TEST(SpatialViewStateTest, DefaultViewFlags)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {100, 100, 100});
    auto const& flags = view->getViewFlags();

    EXPECT_EQ(flags.renderMode(), dqCommon::RenderMode::Wireframe);  // raw default
    EXPECT_FALSE(flags.lighting());
    EXPECT_TRUE(flags.materials());        // ViewFlags default (unchanged)
    EXPECT_FALSE(flags.backgroundMap());   // raw default (NOT the manufacture override)
    EXPECT_FALSE(flags.shadows());
    EXPECT_FALSE(flags.monochrome());
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/ViewPicker.ts:149-169
//              (manufactureSpatialView). Verifies the display-test-app view overrides are
//              applied on top of createBlank's raw defaults.
TEST(ManufactureSpatialViewTest, AppliesDisplayTestAppOverrides)
{
    dqApp::BlankConnectionProps props;
    props.name = "mfg";
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto imodel = dqApp::BlankConnection::create(props);

    auto view = dqApp::manufactureSpatialView(imodel.Get());
    ASSERT_TRUE(view.IsValid());

    // ViewPicker.ts:153 — origin = ext.low, extents = ext.high.minus(ext.low)
    EXPECT_DOUBLE_EQ(view->GetOrigin().x, -1000.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().x, 2000.0);

    // ViewPicker.ts:157-161 — viewFlags.copy({backgroundMap, lighting, SmoothShade})
    auto const& flags = view->getViewFlags();
    EXPECT_EQ(flags.renderMode(), dqCommon::RenderMode::SmoothShade);
    EXPECT_TRUE(flags.lighting());
    EXPECT_TRUE(flags.backgroundMap());
    // ViewPicker.ts leaves viewFlags.grid unchanged (default false) — DTA-faithful.
    // Background-map depth-range port (ViewingSpace.ts:191-204) keeps the sky's
    // frustum deep with grid off, so grid no longer needs to default on.
    EXPECT_FALSE(flags.grid());

    // ViewPicker.ts:163 — backgroundColor = ColorDef.white (tbgr 0x00FFFFFF, alpha 0 —
    // NOT 0xFFFFFFFF; ColorDef stores 0xTTBBGGRR and white has no transparency bits).
    EXPECT_EQ(view->GetDisplayStyle().getBackgroundColor(), dqCommon::ColorDef::white.getTbgr());
    // ViewPicker.ts:166 — environment.withDisplay({ sky: true })
    EXPECT_TRUE(view->GetDisplayStyle().getEnvironment().displaySky);
}

// Ported from: itwinjs-core core/frontend/src/test/SpatialViewState.test.ts
//              TEST(SpatialViewStateTest, CameraOffByDefault)
TEST(SpatialViewStateTest, CameraOffByDefault)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {100, 100, 100});
    EXPECT_FALSE(view->IsCameraOn());
}

// Authored: no reference test exists in itwinjs-core SpatialViewState.test.ts for the
//           createBlank rotation default. Per SpatialViewState.ts:77 docstring, when the
//           `rotation` argument is undefined, createBlank uses the top view
//           (StandardViewId.Top = identity matrix). DanQing previously forced Iso here as
//           a workaround for a wrong view origin; the faithful default is identity (Top).
TEST(SpatialViewStateTest, CreateBlankDefaultsToTopView)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {100, 100, 100});
    EXPECT_TRUE(view->getRotation().IsExactEqual(dqGeom::Matrix3d::CreateIdentity()));
}

// Authored: no reference test exists in itwinjs-core for createBlank's optional rotation
//           argument (SpatialViewState.test.ts exercises only the undefined→Top path).
//           Locks the rotation-provided path restored in audit D2.1
//           (SpatialViewState.ts:84-85: if (undefined !== rotation) view.setRotation(rotation)).
TEST(SpatialViewStateTest, CreateBlankAppliesProvidedRotation)
{
    auto const iso = dqApp::StandardView::Iso();
    auto view = SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {100, 100, 100}, iso);
    EXPECT_TRUE(view->getRotation().IsExactEqual(iso));
}

// Ported from: itwinjs-core core/frontend/src/test/SpatialViewState.test.ts
//              TEST(SpatialViewStateTest, SetExtents)
TEST(SpatialViewStateTest, SetExtents)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {100, 100, 100});

    dqGeom::Vector3d newExtents = {500, 500, 500};
    view->SetExtents(newExtents);

    EXPECT_DOUBLE_EQ(view->GetExtents().x, 500.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().y, 500.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().z, 500.0);
}

// Ported from: itwinjs-core core/frontend/src/ViewState.ts:811-822 (fixAspectRatio).
// Verifies the Y axis is ALWAYS the one adjusted to match the window aspect (the
// docstring at ViewState.ts:865-869: "always adjusts the Y axis"), and the view center
// is preserved (origin shifts by half the Y delta).
TEST(FixAspectRatioTest, AlwaysAdjustsYAxis)
{
    auto view = SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {1000, 1000, 100});
    // Square extents (x=y=1000); windowAspect=2.0 (wide window). Faithful:
    // extents.y = extents.x / aspect = 500; X and Z untouched.
    view->FixAspectRatio(2.0f);

    EXPECT_NEAR(view->GetExtents().x, 1000.0, 1e-6);   // X untouched
    EXPECT_NEAR(view->GetExtents().y, 500.0, 1e-6);    // Y = x / aspect
    EXPECT_NEAR(view->GetExtents().z, 100.0, 1e-6);    // Z untouched
    // Center preserved: origin += 0.5 * (origExtents - extents) = 0.5*(1000-500) = +250
    // (identity rotation -> world Y delta equals view-local Y delta).
    EXPECT_NEAR(view->GetOrigin().y, 250.0, 1e-6);
    EXPECT_NEAR(view->GetOrigin().x, 0.0, 1e-6);
}

// Ported from: itwinjs-core SpatialViewState.computeFitRange (SpatialViewState.ts:145-160)
// null-range fallback -> computeBaseExtents (projectExtents x 1.0001). For a blank
// connection (no tile trees) this is the exercised path.
TEST(SpatialViewStateTest, ComputeFitRangeFallsBackToProjectExtents)
{
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-500, -500, -50, 500, 500, 50);
    auto imodel = dqApp::BlankConnection::create(props);
    auto view = SpatialViewState::CreateBlank(
        imodel.Get(), dqGeom::Point3d::From(0, 0, 0), dqGeom::Vector3d::From(1000, 1000, 100));
    auto fitRange = view->ComputeFitRange();
    // computeBaseExtents scales projectExtents about its center (origin) by 1.0001.
    EXPECT_NEAR(fitRange.low.x, -500.0 * 1.0001, 1.0);
    EXPECT_NEAR(fitRange.high.x, 500.0 * 1.0001, 1.0);
    EXPECT_NEAR(fitRange.high.z, 50.0 * 1.0001, 1.0);
}

// ---------------------------------------------------------------------------
// DisplayStyle tests
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/common/src/DisplayStyleSettings.ts
//              DisplayStyle3dSettings default backgroundColor =
//              ColorDef.fromJSON(undefined) = black (tbgr 0).
// DanQing previously defaulted to white — corrected to the faithful black default;
// the blank-connection view overrides to white via manufactureSpatialView.
TEST(DisplayStyleTest, DefaultBackgroundIsBlack)
{
    DisplayStyle style;
    EXPECT_EQ(style.getBackgroundColor(), 0x00000000u);
}

// Ported from: itwinjs-core core/frontend/src/test/ViewState.test.ts
//              TEST(DisplayStyleTest, setBackgroundColor)
TEST(DisplayStyleTest, setBackgroundColor)
{
    DisplayStyle style;
    style.setBackgroundColor(0xFF0000FF);  // red
    EXPECT_EQ(style.getBackgroundColor(), 0xFF0000FF);
}

// ---------------------------------------------------------------------------
// ViewFlags tests
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core core/frontend/src/test/ViewState.test.ts
//              TEST(ViewFlagsTest, DefaultValues)
TEST(ViewFlagsTest, DefaultValues)
{
    dqCommon::ViewFlags flags;
    EXPECT_EQ(flags.renderMode(), dqCommon::RenderMode::Wireframe);
    EXPECT_FALSE(flags.lighting());
    EXPECT_TRUE(flags.materials());
    EXPECT_FALSE(flags.backgroundMap());
    EXPECT_FALSE(flags.shadows());
    EXPECT_FALSE(flags.monochrome());
}
