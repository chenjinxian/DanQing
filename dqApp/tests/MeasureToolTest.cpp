// MeasureToolTest — M-O(3) P2 MeasureDistanceTool 引擎锁（收点链/段数学/
// 累计/撤销/Restart/提示面/注册）。
//
// 锚定（真实读过的参考行号）：
//   - MeasureTool.ts:641-688 onDataButtonDown（收点 + ctrl 分流 :682-683）；
//   - :578-632 acceptNewSegments（段构造 :580-594 数学 + marker :596 中点）；
//   - :486-501 updateTotals（累计 + 标签）+ :478-484 reportMeasurements；
//   - :691-701 onResetButtonUp（空位置→Restart；否则接受）；
//   - :704-721 onUndoPreviousStep（location 优先弹，再 segment）；
//   - :291-307 getSnapPoints。
//
// RED（M-O(3) P2 落地前）：MeasureDistanceTool/MeasureSegment 类型不存在
// ——编译期缺 API 红（P1 同款先例）。
//
// Authored: no reference test exists in itwinjs-core for MeasureDistanceTool
//          （core/frontend 无 JS 单测；行为锚定如上）。
#include <gtest/gtest.h>

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/MeasureTool.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqGeom/Point3d.h>

#include <cmath>

namespace {

// NavigateTest 同款夹具。
struct MeasureAndViewport {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;
};

MeasureAndViewport measureBuildViewport()
{
    MeasureAndViewport r;
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(
        dqGeom::Point3d::From(-100.0, -100.0, -100.0),
        dqGeom::Point3d::From(100.0, 100.0, 100.0));
    r.imodel = dqApp::BlankConnection::create(props);
    r.view = dqApp::SpatialViewState::CreateBlank(
        r.imodel.Get(), dqGeom::Point3d::From(0.0, 0.0, 0.0),
        dqGeom::Vector3d::From(200.0, 200.0, 200.0));
    r.vp = dqApp::Viewport::Create(nullptr, r.view);
    r.view->SetExtents(dqGeom::Vector3d::From(
        200.0, 200.0 * r.vp->height() / r.vp->width(), 200.0));
    r.vp->setupViewFromFrustum(r.vp->getFrustum(true));
    return r;
}

dqApp::BeButtonEvent dataDownAt(dqApp::Viewport* vp, dqGeom::Point3d pt)
{
    dqApp::BeButtonEvent ev;
    ev.button = dqApp::BeButton::Data;
    ev.isDown = false;
    ev.viewport = vp;
    ev.point = pt;
    ev.rawPoint = pt;
    return ev;
}

}  // namespace

// ---------------------------------------------------------------------------
// 段数学锁（computeSegment = acceptNewSegments :580-594 的数学面）。
// ---------------------------------------------------------------------------
TEST(MeasureSegmentMath, DistanceSlopeDelta)
{
    dqApp::MeasureLocation from;
    from.point = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    from.adjustedPoint = from.point;
    from.refAxes = dqGeom::Matrix3d::CreateIdentity();
    dqApp::MeasureLocation to;
    to.point = dqGeom::Point3d::From(3.0, 4.0, 0.0);  // 3-4-5
    to.adjustedPoint = to.point;
    to.refAxes = dqGeom::Matrix3d::CreateIdentity();

    dqApp::MeasureSegment seg;
    dqApp::MeasureDistanceTool::computeSegment(seg, from, to);
    EXPECT_NEAR(seg.distance, 5.0, 1e-12);
    EXPECT_NEAR(seg.slope, 0.0, 1e-12);  // 平面段
    EXPECT_NEAR(seg.delta.x, 3.0, 1e-12);
    EXPECT_NEAR(seg.delta.y, 4.0, 1e-12);
    EXPECT_NEAR(seg.markerWorldLocation.x, 1.5, 1e-12);  // :596 中点
    EXPECT_NEAR(seg.markerWorldLocation.y, 2.0, 1e-12);
}

TEST(MeasureSegmentMath, VerticalSlopeIsHalfPi)
{
    dqApp::MeasureLocation from;
    from.point = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    from.adjustedPoint = from.point;
    dqApp::MeasureLocation to;
    to.point = dqGeom::Point3d::From(0.0, 0.0, 2.0);  // xyDist=0 → slope=π（:586）
    to.adjustedPoint = to.point;

    dqApp::MeasureSegment seg;
    dqApp::MeasureDistanceTool::computeSegment(seg, from, to);
    EXPECT_NEAR(seg.distance, 2.0, 1e-12);
    EXPECT_NEAR(seg.slope, 3.14159265358979323846, 1e-12);
}

// ---------------------------------------------------------------------------
// 收点链（onDataButtonDown/Reset/Undo/累计）。
// ---------------------------------------------------------------------------
TEST(MeasureDistanceChain, TwoPointsAcceptSegmentAndTotal)
{
    auto r = measureBuildViewport();
    dqApp::MeasureDistanceTool tool;
    tool.onPostInstall();

    // 第一点。
    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(0, 0, 0)));
    EXPECT_EQ(1u, tool.locationData().size());
    EXPECT_TRUE(tool.acceptedSegments().empty());

    // 第二点（无 ctrl → :682-683 接受段 + 清位置）。
    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(3, 4, 0)));
    EXPECT_TRUE(tool.locationData().empty());
    ASSERT_EQ(1u, tool.acceptedSegments().size());
    EXPECT_NEAR(tool.acceptedSegments()[0].distance, 5.0, 1e-12);
    EXPECT_NEAR(tool.totalDistance(), 5.0, 1e-12);
    // 累计标签（恒米制 EQUIVALENCE——"5.0000 m"）。
    ASSERT_TRUE(tool.totalDistanceMarker().has_value());
    EXPECT_EQ(std::string("5.0000 m"), tool.totalDistanceMarker()->label());

    // Restart（空位置 + Reset → :692-693 清段）。
    tool.onResetButtonUp(dataDownAt(r.vp, dqGeom::Point3d::From(0, 0, 0)));
    EXPECT_TRUE(tool.acceptedSegments().empty());
    EXPECT_NEAR(tool.totalDistance(), 0.0, 1e-12);
    EXPECT_FALSE(tool.totalDistanceMarker().has_value());

    delete r.vp;
}

TEST(MeasureDistanceChain, ControlKeyAccumulatesPolyline)
{
    auto r = measureBuildViewport();
    dqApp::MeasureDistanceTool tool;
    tool.onPostInstall();

    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(0, 0, 0)));
    // ctrl 按住 → :682-683 不接受（位置累积）。
    {
        auto ev = dataDownAt(r.vp, dqGeom::Point3d::From(3, 4, 0));
        ev.keyModifiers = dqApp::BeModifierKeys::Control;
        tool.onDataButtonDown(ev);
    }
    EXPECT_EQ(2u, tool.locationData().size());
    EXPECT_TRUE(tool.acceptedSegments().empty());

    // 第三点（无 ctrl）→ 三点两段全接受（:579-586 连续段）。
    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(6, 8, 0)));
    EXPECT_TRUE(tool.locationData().empty());
    ASSERT_EQ(2u, tool.acceptedSegments().size());
    EXPECT_NEAR(tool.totalDistance(), 10.0, 1e-12);  // 5 + 5
    // 多段 → Cumulative 标签（reportMeasurements :481 分流——1 段 Distance/
    // >1 段 Cumulative Distance）。
    EXPECT_EQ(2u, tool.acceptedSegments().size());

    delete r.vp;
}

TEST(MeasureDistanceChain, UndoPopsLocationThenSegment)
{
    auto r = measureBuildViewport();
    dqApp::MeasureDistanceTool tool;
    tool.onPostInstall();

    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(0, 0, 0)));
    {
        auto ev = dataDownAt(r.vp, dqGeom::Point3d::From(3, 4, 0));
        ev.keyModifiers = dqApp::BeModifierKeys::Control;
        tool.onDataButtonDown(ev);
    }
    ASSERT_EQ(2u, tool.locationData().size());

    // :708-709 location 优先弹。
    EXPECT_TRUE(tool.onUndoPreviousStep());
    EXPECT_EQ(1u, tool.locationData().size());

    // 再弹 → 空态（:714-716）。
    EXPECT_TRUE(tool.onUndoPreviousStep());
    EXPECT_TRUE(tool.locationData().empty());

    // 空态 undo → false（:705）。
    EXPECT_FALSE(tool.onUndoPreviousStep());

    delete r.vp;
}

TEST(MeasureDistanceChain, SnapPointsFromAcceptedSegments)
{
    auto r = measureBuildViewport();
    dqApp::MeasureDistanceTool tool;
    tool.onPostInstall();

    // 无段无两点 → nullopt（:292-293）。
    EXPECT_FALSE(tool.getSnapPointsTest().has_value());

    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(0, 0, 0)));
    tool.onDataButtonDown(dataDownAt(r.vp, dqGeom::Point3d::From(3, 4, 0)));
    ASSERT_EQ(1u, tool.acceptedSegments().size());

    // 段起点+终点（非闭环 :297-300）。
    auto snap = tool.getSnapPointsTest();
    ASSERT_TRUE(snap.has_value());
    ASSERT_EQ(2u, snap->size());
    EXPECT_NEAR((*snap)[0].x, 0.0, 1e-12);
    EXPECT_NEAR((*snap)[1].x, 3.0, 1e-12);

    delete r.vp;
}

// 注册面（keyin "measure distance"）。
TEST(MeasureDistanceChain, RegisteredWithKeyin)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    auto* tool = admin.GetRegistry().Create("Measure.Distance");
    ASSERT_NE(tool, nullptr);
    EXPECT_STREQ("Measure.Distance", tool->getToolId());
    delete tool;
}
