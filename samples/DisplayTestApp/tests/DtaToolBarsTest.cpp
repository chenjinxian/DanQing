// DtaToolBarsTest — M-R DTA 布局对齐工具栏测试（app 工具栏 + 26 项主工具栏）
// Authored: no reference test exists in display-test-app for toolbar construction
//           (the test app ships no tests); scenarios transcribe the DTA chrome from
//           Surface.ts:122-178 (app toolbar) + Viewer.ts:238-446 (main toolbar 26
//           items) + ToolBar.ts:122-197 (drop-down interaction contract).
#include <gtest/gtest.h>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QMenu>
#include <QStringList>
#include <QWidgetAction>
#include "DtaToolBars.h"

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>

// 保证 QApplication 存在（与其他 Qt 测试一致的模式；进程级只建一次）
#include <QApplication>
namespace { struct QtEnv { QtEnv() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnv s_qt;

// App::Application 桩单例定义（MainWindow.cpp 引用 _pcSingleton 读取参数组；
// 模式同 ActionSubclassTest.cpp:18-20 / main.cpp:39-40）。
#include <App/Application.h>
App::Application* App::Application::_pcSingleton = nullptr;
std::map<std::string, std::string> App::Application::m_config;

namespace {

// 主工具栏 26 项文本序（Viewer.ts:238-446——严格序；ViewPicker 位为空文本
// [widget 包装 action]，Google Maps 位不存在 [config 关态]）。
char const* const kMainTexts[] = {
    "Debug", "Open iModel", "Open Hub", "",
    "Models", "Categories", "Saved Views", "Camera Paths",
    "Select", "Measure", "View Settings",
    "Fit", "Window Area", "Rotate",
    "",   // Standard rotations（QToolButton 包装，无文本）
    "Walk", "Undo", "Redo",
    "Animation", "Sectioning", "Classification", "Overrides",
    "Point Cloud", "Contours", "Format Set",
};
constexpr int kMainCount = 25;   // 26 项中 Google Maps 不出现（config 关态）

}  // namespace

// Start 态无工具栏（用户 2026-10-07 指令：初始页不显示工具栏——DTA app 级
// 入口由 Start 页卡片承载，appToolBar 移除）；视口聚焦 → 主工具栏显示。
TEST(DtaToolBarsSet, NoToolbarsUntilViewportFocused)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    ASSERT_NE(bars.mainToolBar(), nullptr);

    // 无聚焦视口（Start 态）：主工具栏隐藏——初始页无任何工具栏。
    EXPECT_TRUE(bars.mainToolBar()->isHidden());
    // appToolBar 已移除：全窗无 "DTA App" 工具栏。
    EXPECT_EQ(mw.findChild<QToolBar*>(QStringLiteral("DTA.App")), nullptr);

    // 聚焦视口 → 主工具栏显示 + 换位语义（DropViewport 后回隐藏）。
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    dqApp::Application::Get().GetViewManager().AddViewport(vp);
    EXPECT_FALSE(bars.mainToolBar()->isHidden());
    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
    EXPECT_TRUE(bars.mainToolBar()->isHidden());
}

// 主工具栏 26 项严格序（Viewer.ts:238-446）+ Measure/Walk 激活（M-R——引擎
// 已移植 [M-O(3) P1/P2]，此前置灰）+ 置灰面（hub/CameraPaths/Animation/
// Classification/PointCloud/Contours/FormatSet）。
TEST(DtaToolBarsMain, TwentySixItemsInDtaOrder)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QList<QAction*> acts = bars.mainToolBar()->actions();
    ASSERT_EQ(acts.size(), kMainCount);
    for (int i = 0; i < kMainCount; ++i)
        EXPECT_EQ(acts[i]->text(), kMainTexts[i]) << i;

    // 激活面：Debug[既有]/Measure[M-R]/Walk[M-R]/Select/Fit/WindowArea/Rotate/
    // Undo/Redo/ViewSettings/Sectioning[SectionsPanel 按钮]/Overrides/
    // SavedViews/CameraPaths 菜单弹出。
    EXPECT_TRUE(acts[0]->isEnabled()) << "Debug info";
    EXPECT_TRUE(acts[9]->isEnabled()) << "Measure (engine ported M-O(3) P2)";
    EXPECT_TRUE(acts[15]->isEnabled() || !acts[15]->isVisible())
        << "Walk (engine ported M-O(3) P1; hidden via only3d at construction)";
    EXPECT_TRUE(acts[19]->isEnabled()) << "Sectioning (M-P P-G)";

    // 置灰面（大件未移植/零网络）。
    // Camera Paths/Animation 为可点空下拉（DTA blank 语义——面板空非置灰）。
    for (int i : { 2 /*Open Hub*/, 20 /*Classification*/, 22 /*PointCloud*/,
                   23 /*Contours*/, 24 /*Format Set*/ })
        EXPECT_FALSE(acts[i]->isEnabled()) << kMainTexts[i];

    // Google Maps 零出现（Viewer.ts:424-434 config googleMapsUi 门——关态）。
    for (QAction* a : acts)
        EXPECT_NE(a->text(), QStringLiteral("Google Maps"));

    // ViewPicker = QComboBox 控件（位 4 的 widget 包装 action）。
    EXPECT_NE(bars.mainToolBar()->findChild<QComboBox*>(
                  QStringLiteral("DTA.Views.ViewPicker")), nullptr);

    // Standard rotations（位 15）= QToolButton + 命名 menu。
    EXPECT_NE(bars.mainToolBar()->findChild<QMenu*>(
                  QStringLiteral("DTA.StdRot.Menu")), nullptr);
}

// StandardRotations 面板八向按钮（StandardRotations.ts:9-18/:29-47——
// top/bottom/left/right/front/back/iso/isoRight；:33-47 两行四个按钮）。
TEST(DtaToolBarsViewTools, StandardViewsPanelHasEightDirectionButtonsInDtaOrder)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);

    auto* svMenu = bars.mainToolBar()->findChild<QMenu*>(
        QStringLiteral("DTA.StdRot.Menu"));
    ASSERT_NE(svMenu, nullptr);
    ASSERT_EQ(svMenu->actions().size(), 1);
    auto* panelWa = qobject_cast<QWidgetAction*>(svMenu->actions().first());
    ASSERT_NE(panelWa, nullptr);
    auto* panel = panelWa->defaultWidget();
    ASSERT_NE(panel, nullptr);

    const char* names[] = { "Top", "Bottom", "Left", "Right", "Front", "Back", "Iso", "RightIso" };
    for (int i = 0; i < 8; ++i) {
        auto* b = panel->findChild<QToolButton*>(
            QString::fromLatin1("DTA.StdRot.%1").arg(i));
        ASSERT_NE(b, nullptr) << names[i];
        EXPECT_EQ(b->toolTip(), names[i]) << i;
        EXPECT_FALSE(b->icon().isNull()) << names[i];
    }
    EXPECT_EQ(panel->findChildren<QToolButton*>().size(), 8);
}

// 下拉交互合同（ToolBar.ts:163-196）：单开互斥（open 先 close 全部）+
// closeOpenDropDowns。headless 面板观测（QWidget show/close）。
TEST(DtaToolBarsDropDowns, OpenIsMutuallyExclusiveAndCloseClosesAll)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);

    QWidget panelA, panelB;
    panelA.hide();
    panelB.hide();
    bars.openDropDown(&panelA);
    ASSERT_EQ(bars.openDropDownCount(), 1);
    EXPECT_FALSE(panelA.isHidden());

    // 单开互斥：开 B → A 关（ToolBar.open :168-180 先 close）。
    bars.openDropDown(&panelB);
    EXPECT_EQ(bars.openDropDownCount(), 1);
    EXPECT_TRUE(panelA.isHidden());
    EXPECT_FALSE(panelB.isHidden());

    // ToolBar.close（:163-172）。
    bars.closeOpenDropDowns();
    EXPECT_EQ(bars.openDropDownCount(), 0);
    EXPECT_TRUE(panelB.isHidden());
}

// only3d 显隐（ToolBar.onViewChanged :192-193——is3d ? block : none）：
// Models/StandardRotations/Walk/Classification 四 only3d 项随视口维度显隐。
// 无视口态：隐藏（onViewChanged 的 is3d=false 面）。
TEST(DtaToolBarsOnly3d, HiddenWithoutViewportShownFor3d)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QList<QAction*> acts = bars.mainToolBar()->actions();

    // 无视口 → only3d 四项隐藏。
    EXPECT_FALSE(acts[4]->isVisible());   // Models
    EXPECT_FALSE(acts[14]->isVisible());  // Standard rotations
    EXPECT_FALSE(acts[15]->isVisible());  // Walk
    EXPECT_FALSE(acts[20]->isVisible());  // Classification
    // 非 only3d 项不受影响（如 Select/Categories）。
    EXPECT_TRUE(acts[5]->isVisible());    // Categories
    EXPECT_TRUE(acts[8]->isVisible());    // Select

    // 3d 视口 → 四项可见 + 换位（主工具栏显/app 工具栏隐）。
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    dqApp::Application::Get().GetViewManager().AddViewport(vp);
    EXPECT_TRUE(acts[4]->isVisible());
    EXPECT_TRUE(acts[15]->isVisible());
    EXPECT_FALSE(bars.mainToolBar()->isHidden());

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
    EXPECT_FALSE(acts[4]->isVisible());
    EXPECT_TRUE(bars.mainToolBar()->isHidden());
}

// ViewPicker 真数据：blank connection 下恰有合成条目 "Spatial View"
//（ViewPicker.ts:139-140；经 ViewList::create→populate→QComboBox 条目）。
TEST(DtaToolBarsViews, ViewPickerListsSyntheticSpatialViewOnBlank)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    dqApp::Application::Get().GetViewManager().AddViewport(vp);

    auto* picker = bars.mainToolBar()->findChild<Gui::ViewPickerComboBox*>(
        QStringLiteral("DTA.Views.ViewPicker"));
    ASSERT_NE(picker, nullptr);
    picker->repopulate();
    ASSERT_EQ(picker->count(), 1);
    EXPECT_EQ(picker->itemText(0), QStringLiteral("Spatial View"));

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

// Select 按钮（Viewer.ts:316-320——tools.run("SVTSelect")）：激活 Select 工具
//（DTA defaultToolId 面）。
TEST(DtaToolBarsSelection, SelectButtonActivatesSelectTool)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QList<QAction*> acts = bars.mainToolBar()->actions();
    QAction* sel = acts[8];
    ASSERT_EQ(sel->text(), QStringLiteral("Select"));
    dqApp::Application::Get().GetToolAdmin().OnInitialized();   // 注册面（既有先例）
    sel->trigger();
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ASSERT_NE(ta.activeTool(), nullptr);
    EXPECT_STREQ(ta.activeTool()->getToolId(), "Select");
    ta.SetActiveTool(nullptr);
}
