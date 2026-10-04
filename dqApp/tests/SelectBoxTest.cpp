// SelectBoxTest — M-O(3) P3 框选引擎锁（selectByPoints 族 + ctrl Invert +
// PickAtRect headless 面 + decorate 转发面）。
//
// 锚定（真实读过的参考行号）：
//   - SelectTool.ts:359-369 selectByPointsStart（Data/Reset 键 + 点入）；
//   - :371-391 selectByPointsEnd（Box/Line 分流 + useOverlapSelection）；
//   - :329-357 selectByPointsProcess（候选 + ctrl Invert/Replace + 空面 miss 清）；
//   - :278-285 useOverlapSelection（右→左 = overlap；Shift 反转）；
//   - :428-439 onMouseStartDrag/EndDrag；
//   - :414 processHit 的 ctrl Invert 分流；
//   - ElementSetTool.ts:637-721 getAreaSelectionCandidates（Box 收缩带：
//   outline = 边缘 2 device px 带内像素的 id；inside = contents - outline）。
//
// RED（M-O(3) P3 落地前）：selectByPoints 族/PickAtRect/ctrl Invert 不存在
// ——编译期缺 API 红（P1/P2 同款先例）。
//
// EQUIVALENCE（headless 面）：PickAtRect 经 SetPickRectHandlerForTest 逐读
// 回调驱动——收缩带五读[全域+四边缘带]可逐读差异注入（inside/outline 差集
// 直接可测）；真 GL 面由 DtaTest PickDumpScene 族覆盖。
//
// Authored: no reference test exists in itwinjs-core for SelectTool drag-box
//           （SelectTool.test.ts 无框选用例；行为锚定如上）。
#include <gtest/gtest.h>

#include "SelectionTool.h"  // dqApp/src 内部头（P3 直驱面——CMake src/ include）

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewState.h>

#include <dqGeom/Point3d.h>

namespace {

struct SelectBoxFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    SelectBoxFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(
            dqGeom::Point3d::From(-100.0, -100.0, -100.0),
            dqGeom::Point3d::From(100.0, 100.0, 100.0));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0.0, 0.0, 0.0),
            dqGeom::Vector3d::From(200.0, 200.0, 200.0));
        vp = dqApp::Viewport::Create(nullptr, view);
        view->SetExtents(dqGeom::Vector3d::From(
            200.0, 200.0 * vp->height() / vp->width(), 200.0));
        vp->setupViewFromFrustum(vp->getFrustum(true));
        dqApp::Application::Get().GetViewManager().AddViewport(vp);
    }

    ~SelectBoxFixture()
    {
        dqApp::Application::Get().GetViewManager().DropViewport(vp);
        delete vp;
    }
};

dqApp::BeButtonEvent dragEvent(dqApp::Viewport* vp, dqGeom::Point3d worldPt,
                               dqApp::BeButton button,
                               dqApp::BeModifierKeys mods
                               = dqApp::BeModifierKeys::None)
{
    dqApp::BeButtonEvent ev;
    ev.button = button;
    ev.isDown = false;
    ev.viewport = vp;
    ev.point = worldPt;
    ev.rawPoint = worldPt;
    dqGeom::Point3d const viewPt = vp->WorldToView(worldPt);
    ev.viewPoint = dqGeom::Point3d::From(viewPt.x, viewPt.y, 0.0);
    ev.keyModifiers = mods;
    return ev;
}

}  // namespace

// ---------------------------------------------------------------------------
// 起拖/收框生命周期（:359-369/:371-391）。
// ---------------------------------------------------------------------------
TEST(SelectBox, StartDragSetsSelectByPoints)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    auto start = dragEvent(f.vp, dqGeom::Point3d::From(10, 10, 0),
                           dqApp::BeButton::Data);
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onMouseStartDrag(start));
    EXPECT_TRUE(tool.isSelectByPoints());
    ASSERT_EQ(1u, tool.points().size());
}

TEST(SelectBox, StartDragIgnoresNonDataResetButton)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    auto start = dragEvent(f.vp, dqGeom::Point3d::From(10, 10, 0),
                           dqApp::BeButton::Middle);
    EXPECT_EQ(dqApp::EventHandled::No, tool.onMouseStartDrag(start));
    EXPECT_FALSE(tool.isSelectByPoints());
}

// ---------------------------------------------------------------------------
// 框选 Replace + 收缩带（无 ctrl——:346-348 ReplaceSelectionWithElement；
// ElementSetTool :676-701 的 inside/outline 差集——headless 逐读注入）。
// ---------------------------------------------------------------------------
TEST(SelectBox, BoxDragReplacesSelectionWithCandidates)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    // 预置既有选集（验证 Replace 覆盖）。
    QSet<uint32_t> prior;
    prior.insert(0x99);
    f.imodel->GetSelectionSet().Replace(prior);

    // 收缩带注入：全域读 = {0x1,0x2,0x3}（0x1/0x3 完全在内 + 0x2 触边）；
    // 边缘带读 = {0x2}（仅 0x2 有边缘像素）。inside = 全域 - outline =
    // {0x1, 0x3}——非 overlap 框选只收完全在内者（:693-700）。
    int32_t fullCalls = 0, bandCalls = 0;
    f.vp->SetPickRectHandlerForTest(
        [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
            // 带读 = 窄条（宽或高 ≤ 2 换算像素的薄矩形——测试里以 ≤8 px
            // 近似[框域数百 px]，全域读是大矩形。
            bool const isBand = (x1 - x0) <= 8 || (y1 - y0) <= 8;
            if (isBand) {
                ++bandCalls;
                return std::vector<uint32_t>{0x2};
            }
            ++fullCalls;
            return std::vector<uint32_t>{0x1, 0x2, 0x3};
        });

    auto start = dragEvent(f.vp, dqGeom::Point3d::From(-50, 50, 0),
                           dqApp::BeButton::Data);
    ASSERT_EQ(dqApp::EventHandled::Yes, tool.onMouseStartDrag(start));
    // 左→右（origin.x < corner.x）= 非 overlap（:283）。
    auto end = dragEvent(f.vp, dqGeom::Point3d::From(50, -50, 0),
                         dqApp::BeButton::Data);
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onMouseEndDrag(end));

    // 收缩带逻辑实跑（五读：1 全域 + 4 带）。
    EXPECT_EQ(1, fullCalls);
    EXPECT_EQ(4, bandCalls);
    // inside = {0x1, 0x3}（0x2 触边被收缩带剔除）。
    EXPECT_EQ(2, f.imodel->GetSelectionSet().size());
    EXPECT_TRUE(f.imodel->GetSelectionSet().Contains(0x1));
    EXPECT_TRUE(f.imodel->GetSelectionSet().Contains(0x3));
    EXPECT_FALSE(f.imodel->GetSelectionSet().Contains(0x2));
    EXPECT_FALSE(f.imodel->GetSelectionSet().Contains(0x99));  // Replace 覆盖
    // 收框清面（:388 initSelectTool）。
    EXPECT_FALSE(tool.isSelectByPoints());
}

// ---------------------------------------------------------------------------
// ctrl Invert（:349 —— InvertElementInSelection：在选移除/不在选加入）。
// ---------------------------------------------------------------------------
TEST(SelectBox, ControlDragInvertsCandidates)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    QSet<uint32_t> prior;
    prior.insert(0x1);  // 0x1 已在选——invert 后移除；0x2 加入。
    f.imodel->GetSelectionSet().Replace(prior);

    // overlap 形态（右→左拖 + :283）——收缩带不适用（:677 allowOverlaps →
    // outline 恒 undefined → contents 全收）。
    f.vp->SetPickRectHandlerForTest(
        [](int32_t, int32_t, int32_t, int32_t) {
            return std::vector<uint32_t>{0x1, 0x2};
        });
    auto start = dragEvent(f.vp, dqGeom::Point3d::From(50, 50, 0),
                           dqApp::BeButton::Data);
    ASSERT_EQ(dqApp::EventHandled::Yes, tool.onMouseStartDrag(start));
    auto end = dragEvent(f.vp, dqGeom::Point3d::From(-50, -50, 0),
                         dqApp::BeButton::Data,
                         dqApp::BeModifierKeys::Control);
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onMouseEndDrag(end));

    EXPECT_EQ(1, f.imodel->GetSelectionSet().size());
    EXPECT_FALSE(f.imodel->GetSelectionSet().Contains(0x1));
    EXPECT_TRUE(f.imodel->GetSelectionSet().Contains(0x2));
}

// ---------------------------------------------------------------------------
// 空面 miss 清（:337-342 —— 非 ctrl + processMiss）。
// ---------------------------------------------------------------------------
TEST(SelectBox, EmptyBoxClearsSelectionWithoutControl)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    QSet<uint32_t> prior;
    prior.insert(0x7);
    f.imodel->GetSelectionSet().Replace(prior);

    f.vp->SetPickRectHandlerForTest(
        [](int32_t, int32_t, int32_t, int32_t) {
            return std::vector<uint32_t>{};  // 空候选
        });
    auto start = dragEvent(f.vp, dqGeom::Point3d::From(-50, 50, 0),
                           dqApp::BeButton::Data);
    ASSERT_EQ(dqApp::EventHandled::Yes, tool.onMouseStartDrag(start));
    auto end = dragEvent(f.vp, dqGeom::Point3d::From(50, -50, 0),
                         dqApp::BeButton::Data);
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onMouseEndDrag(end));
    EXPECT_TRUE(f.imodel->GetSelectionSet().isEmpty());
}

// ---------------------------------------------------------------------------
// 点击面 ctrl Invert（onDataButtonUp :414）。
// ---------------------------------------------------------------------------
TEST(SelectBox, ClickControlInvertsSingleHit)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    QSet<uint32_t> prior;
    prior.insert(0x5);
    f.imodel->GetSelectionSet().Replace(prior);

    f.vp->SetPickResultForTest(0x5);  // 命中已在选的 0x5 → ctrl 移除
    auto click = dragEvent(f.vp, dqGeom::Point3d::From(0, 0, 0),
                           dqApp::BeButton::Data,
                           dqApp::BeModifierKeys::Control);
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onDataButtonUp(click));
    EXPECT_TRUE(f.imodel->GetSelectionSet().isEmpty());

    // 再 ctrl 点同元素 → 回选。
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onDataButtonUp(click));
    EXPECT_EQ(1, f.imodel->GetSelectionSet().size());
    EXPECT_TRUE(f.imodel->GetSelectionSet().Contains(0x5));
}

// ---------------------------------------------------------------------------
// Reset 起拖的跨线形态（:383-384 —— Pick 方法 + Reset 键 → Line）。
// ---------------------------------------------------------------------------
TEST(SelectBox, ResetDragUsesCrossingLine)
{
    SelectBoxFixture f;
    dqApp::SelectionTool tool;
    tool.onPostInstall();

    f.vp->SetPickRectHandlerForTest(
        [](int32_t, int32_t, int32_t, int32_t) {
            return std::vector<uint32_t>{0x11};
        });
    auto start = dragEvent(f.vp, dqGeom::Point3d::From(0, 0, 0),
                           dqApp::BeButton::Reset);
    ASSERT_EQ(dqApp::EventHandled::Yes, tool.onMouseStartDrag(start));
    auto end = dragEvent(f.vp, dqGeom::Point3d::From(30, 20, 0),
                         dqApp::BeButton::Reset);
    EXPECT_EQ(dqApp::EventHandled::Yes, tool.onMouseEndDrag(end));
    EXPECT_TRUE(f.imodel->GetSelectionSet().Contains(0x11));
    // Reset 收框清面（:472-477 cleanup 分支在 End 已清——Reset 释放不落入
    // onResetButtonUp 的框选清理分支）。
    EXPECT_FALSE(tool.isSelectByPoints());
}
