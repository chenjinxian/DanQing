// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewList tests (display-test-app view-selection layer)
//
// The ViewList/ViewPicker layer is display-test-app frontend code
// (test-apps/display-test-app/src/frontend/ViewPicker.ts); display-test-app ships no
// test suite, so no reference tests exist for it. These tests transcribe the reference
// CODE PATH executed for a blank connection into assertions:
//   ViewList.create → populate (ViewPicker.ts:69-144) → getView (ViewPicker.ts:34-51)
// with the closed-connection short-circuits of IModelConnection.Views
// (IModelConnection.ts:1488-1505 queryProps isClosed→[] and :1535-1538
// queryDefaultViewId isOpen→Id64.invalid).
#include <gtest/gtest.h>

#include <dqApp/BlankConnection.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/ViewState.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/RenderMode.h>
#include <dqCommon/ViewFlags.h>

using namespace dqApp;

namespace {

// The DTA blank-connection props (Surface.ts:185-188 openBlankConnection defaults).
dqBase::RefPtr<BlankConnection> createDtaBlankConnection()
{
    BlankConnectionProps props;
    props.name = "blank connection test";
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    props.locationIsCartographic = true;
    props.cartographicLocation = dqCommon::Cartographic::fromDegrees(-75.686694, 40.065757, 0.0);
    return BlankConnection::create(props);
}

}  // namespace

// Authored: no reference test exists in display-test-app for ViewList (the test app
//           ships no tests). Scenario from ViewPicker.ts:69-144 (populate): a blank
//           connection's getViewList short-circuits to [] (IModelConnection.ts:1490-1491),
//           so no real views are found, _defaultViewId stays invalid, and the synthetic
//           "Spatial View" entry is inserted (ViewPicker.ts:139-140).
TEST(ViewList, PopulateOnBlankConnectionInsertsSyntheticEntry)
{
    auto conn = createDtaBlankConnection();

    auto views = ViewList::create(conn.Get());

    // Exactly the one synthetic entry (ViewPicker.ts:139-140).
    ASSERT_EQ(views.length(), 1);
    auto const* entry = views.get(0);
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->id.isNull());                 // Id64.invalid
    EXPECT_EQ(entry->name, "Spatial View");
    EXPECT_EQ(entry->className, "BisCore:SpatialViewDefinition");  // SpatialViewState.classFullName
    EXPECT_FALSE(entry->isPrivate);
    // _defaultViewId is still invalid — the synthetic entry's id IS invalid
    // (ViewPicker.ts:127-137 leaves it invalid because the array was empty).
    EXPECT_TRUE(views.defaultViewId().isNull());
}

// Authored: no reference test exists in display-test-app for ViewList (the test app
//           ships no tests). Scenario from ViewPicker.ts:34-51 (getView): the blank
//           connection's views.load(invalid) fails (no RPC on a closed connection), the
//           catch falls back to manufactureSpatialView (ViewPicker.ts:39-44), and the
//           result is cached; every caller receives a CLONE so the cached view stays in
//           its initial persistent state (ViewPicker.ts:49-50).
TEST(ViewList, GetDefaultViewManufacturesAndClones)
{
    auto conn = createDtaBlankConnection();
    auto views = ViewList::create(conn.Get());

    auto view = views.getDefaultView(conn.Get());
    ASSERT_TRUE(view.IsValid());
    ASSERT_TRUE(view->isSpatialView());

    // manufactureSpatialView invariants (ViewPicker.ts:149-169):
    //   origin = ext.low, extents = diagonal, top view
    EXPECT_DOUBLE_EQ(view->GetOrigin().x, -1000.0);
    EXPECT_DOUBLE_EQ(view->GetOrigin().y, -1000.0);
    EXPECT_DOUBLE_EQ(view->GetOrigin().z, -100.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().x, 2000.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().y, 2000.0);
    EXPECT_DOUBLE_EQ(view->GetExtents().z, 200.0);
    auto const& flags = view->getViewFlags();
    EXPECT_EQ(flags.renderMode(), dqCommon::RenderMode::SmoothShade);
    EXPECT_TRUE(flags.lighting());
    EXPECT_TRUE(flags.backgroundMap());
    EXPECT_FALSE(flags.grid());
    EXPECT_EQ(view->GetDisplayStyle().getBackgroundColor(), dqCommon::ColorDef::white.getTbgr());
    EXPECT_TRUE(view->GetDisplayStyle().getEnvironment().displaySky);

    // ViewPicker.ts:49-50 — getView returns a clone; the cached view keeps its
    // initial (persistent) state. Two calls yield distinct objects with equal state,
    // and mutating the first clone must not leak into the cache.
    auto view2 = views.getDefaultView(conn.Get());
    ASSERT_TRUE(view2.IsValid());
    EXPECT_NE(view.Get(), view2.Get());
    view->SetOrigin(dqGeom::Point3d::From(7.0, 8.0, 9.0));
    auto view3 = views.getDefaultView(conn.Get());
    ASSERT_TRUE(view3.IsValid());
    EXPECT_DOUBLE_EQ(view3->GetOrigin().x, -1000.0);  // persistent state intact
}

// Authored: no reference test exists in itwinjs-core for the closed-connection
//           short-circuits of IModelConnection.Views; asserts the reference
//           implementation contracts for a blank (always-closed) connection:
//           - queryProps returns [] when isClosed (IModelConnection.ts:1490-1491),
//             so getViewList is empty for both wantPrivate values (:1518-1526);
//           - queryDefaultViewId returns Id64.invalid when not isOpen (:1535-1538);
//           - load() cannot succeed without a backend (ViewPicker.ts:38-44 catches
//             the reference's RPC failure — DanQing reports failure as a null RefPtr,
//             the §3.4 throw→Result-style adaptation).
TEST(IModelConnectionViews, BlankConnectionShortCircuits)
{
    auto conn = createDtaBlankConnection();

    EXPECT_TRUE(conn->GetViews().getViewList({false}).empty());
    EXPECT_TRUE(conn->GetViews().getViewList({true}).empty());
    EXPECT_TRUE(conn->GetViews().queryDefaultViewId().isNull());
    EXPECT_FALSE(conn->GetViews().load(dqBase::DqId()).IsValid());
}
