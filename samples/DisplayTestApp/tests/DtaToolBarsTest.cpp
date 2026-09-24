// DtaToolBarsTest — DTA-categorized toolbar set tests
// Authored: no reference test exists in display-test-app for toolbar construction
//           (the test app ships no tests); scenarios transcribe the DTA chrome from
//           Surface.ts:122-180 (app toolbar) + Viewer.ts:236-447 (viewer toolbar).
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

// DTA Surface 工具栏（Surface.ts:122-180）：Open iModel from disk / Open Blank
// Connection / Analysis Style / Decoration Geometry / Cesium Renderer 示例。
// Authored: no reference test exists in display-test-app for toolbar construction;
//           scenario transcribes Surface.ts:122-180 (app toolbar) entry order + enabled state.
// 2026-09-11 UI 调整（用户指令）：DTA Surface 起始工具栏的 5 个功能不属于示例程序的
// 常驻工具栏——它们是"新建/打开"入口，移到 Start 页卡片（与 DTA 的打开前工具栏
// 语义一致：DTA 进入 view 后这些入口不再出现）。此测试锁定 File 工具栏不再存在。
TEST(DtaToolBarsFile, SurfaceEntriesMovedToStartPage)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    // 六条工具栏：五条分类 + Deco Example（2026-09-17 Surface.ts:155-165
    // Decoration Geometry 示例入口作为独立工具栏加入——测试漂移修正）。
    auto toolbars = mw.findChildren<QToolBar*>();
    EXPECT_EQ(toolbars.size(), 6);
    for (QToolBar* tb : toolbars)
        EXPECT_NE(tb->windowTitle(), "File")
            << "Surface.ts:122-180 的 5 项已移至 Start 页卡片，工具栏不再有 File";
}

// 五条工具栏按 DTA 功能分类存在（design §2；File 已于 2026-09-11 移到 Start 页）。
// Authored: no reference test exists in display-test-app for toolbar construction;
//           scenario transcribes the DTA chrome category set (Surface.ts + Viewer.ts).
TEST(DtaToolBarsSet, FiveToolbarsInCategoryOrder)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QToolBar* all[] = { bars.viewsToolBar(), bars.selectionToolBar(),
                        bars.viewSettingsToolBar(), bars.viewToolsToolBar(), bars.analysisToolBar() };
    const char* titles[] = { "Views", "Selection", "View Settings", "View Tools", "Analysis" };
    for (int i = 0; i < 5; ++i) {
        ASSERT_NE(all[i], nullptr) << titles[i];
        EXPECT_EQ(all[i]->windowTitle(), titles[i]);
    }
}

// Authored: no reference test exists in display-test-app; scenario transcribes
//           Viewer.ts:236-447 的 Viewer 工具栏前段（Debug/Open×2/ViewPicker/Models/
//           Categories/SavedViews/CameraPaths）+ blank 下的空/合成条目语义。
// 2026-09-11 修订（用户指令）：ViewPicker 不是按钮——DTA 是 HTML <select>
//（ViewPicker.ts:177），Qt 对应 QComboBox（FreeCAD WorkbenchSelector 模式）。
TEST(DtaToolBarsViews, ContentsMatchDtaOrderAndBlankSemantics)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QToolBar* tb = bars.viewsToolBar();
    ASSERT_NE(tb, nullptr);

    // ViewPicker = QComboBox 控件（非按钮/菜单）。
    auto* picker = tb->findChild<QComboBox*>(QStringLiteral("DTA.Views.ViewPicker"));
    ASSERT_NE(picker, nullptr);

    QList<QAction*> acts = tb->actions();
    ASSERT_EQ(acts.size(), 8);
    // 第 4 位是 picker 的 widget 包装 action（空文本）；其余为按钮动作。
    const char* texts[] = { "Debug", "Open iModel", "Open Hub", "",
                            "Models", "Categories", "Saved Views", "Camera Paths" };
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(acts[i]->text(), texts[i]) << i;
    // 置灰：Open iModel / Open Hub（Viewer.ts:238-268 对应项未实现）。
    // Debug 已点亮（2026-09-21 Debug Info 全量移植，测试漂移修正）。
    EXPECT_TRUE(acts[0]->isEnabled()) << "Debug info panel implemented";
    EXPECT_FALSE(acts[1]->isEnabled());
    EXPECT_FALSE(acts[2]->isEnabled());
    // 下拉按钮类（Models/Categories/SavedViews/CameraPaths）：DTA DropDown 无
    // 箭头（ToolBar.ts:99-121）——action 不带 menu，点击经 triggered 手动 popup；
    // 菜单以命名子对象挂在工具栏下。
    for (int i = 4; i < 8; ++i) {
        EXPECT_EQ(acts[i]->menu(), nullptr) << i;   // 无箭头
        EXPECT_NE(tb->findChild<QMenu*>(
                      QStringLiteral("DTA.DropDown.") + acts[i]->text()),
                  nullptr) << i;                     // 点击弹出的菜单存在
    }
}

// Authored: no reference test exists in display-test-app for ViewPicker toolbar
//           wiring; scenario transcribes ViewPicker.ts:139-140 的合成条目语义。
// ViewPicker 真数据：blank connection 下恰有合成条目 "Spatial View"
//（ViewPicker.ts:139-140；经 ViewList::create→populate→QComboBox 条目）。
// 注：本测试创建真实 BlankConnection + Viewport（无 GL，showEvent 不触发）。
TEST(DtaToolBarsViews, ViewPickerListsSyntheticSpatialViewOnBlank)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);

    dqApp::BlankConnectionProps props;
    props.name = "blank connection test";
    props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    ASSERT_NE(vp, nullptr);
    dqApp::Application::Get().GetViewManager().AddViewport(vp);

    // ViewPicker 是 QComboBox（<select> 对应控件）；弹出前 repopulate（showPopup
    // 内部路径，测试直接调用具体类型）。
    auto* picker = bars.viewsToolBar()->findChild<Gui::ViewPickerComboBox*>(
        QStringLiteral("DTA.Views.ViewPicker"));
    ASSERT_NE(picker, nullptr);
    picker->repopulate();
    EXPECT_EQ(picker->count(), 1);
    EXPECT_EQ(picker->itemText(0), QStringLiteral("Spatial View"));

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    delete vp;
}

// Authored: 无参考测试。Viewer.ts:315-319 — Element selection 按钮 =
// IModelApp.tools.run("SVTSelect")；DanQing 等价：创建 Select 并 SetActiveTool。
TEST(DtaToolBarsSelection, SelectButtonActivatesSelectTool)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ta.OnInitialized();          // 幂等注册（"Select" 工厂）
    ta.SetActiveTool(nullptr);   // 归一化前置状态

    QAction* sel = nullptr;
    for (QAction* a : bars.selectionToolBar()->actions())
        if (a->text() == "Select") sel = a;
    ASSERT_NE(sel, nullptr);
    sel->trigger();
    EXPECT_NE(ta.GetActiveTool(), nullptr);

    QAction* measure = nullptr;
    for (QAction* a : bars.selectionToolBar()->actions())
        if (a->text() == "Measure") measure = a;
    ASSERT_NE(measure, nullptr);
    EXPECT_FALSE(measure->isEnabled());  // Measure 未移植（置灰）
    ta.SetActiveTool(nullptr);           // 清理
}

// Authored: 无参考测试；Viewer.ts:337-386 — Fit / Window area / Rotate /
//           Standard rotations 8 向 / Walk / Undo / Redo / Analysis 置灰组。
// 2026-09-11 修订：Standard Views = QToolButton 弹出面板（DTA ToolBarDropDown
// 形态），非文本菜单——actions()[3] 是其 widget 包装 action（空文本）。
TEST(DtaToolBarsViewTools, ContentsMatchDtaOrder)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QToolBar* tb = bars.viewToolsToolBar();
    ASSERT_NE(tb, nullptr);
    QList<QAction*> acts;
    for (QAction* a : tb->actions()) if (!a->isSeparator()) acts.push_back(a);
    ASSERT_EQ(acts.size(), 5);
    const char* texts[] = { "Fit", "Window Area", "Rotate", "", "Walk" };
    for (int i = 0; i < 5; ++i) EXPECT_EQ(acts[i]->text(), texts[i]) << i;
    EXPECT_TRUE(acts[0]->isEnabled());    // Fit ✅
    EXPECT_TRUE(acts[1]->isEnabled());    // Window Area ✅（View.WindowArea 已移植，W4）
    EXPECT_TRUE(acts[2]->isEnabled());    // Rotate ✅
    // Standard Views = QToolButton（弹出 2×4 面板，见下一个测试）
    EXPECT_NE(acts[3]->objectName(), QString());
    EXPECT_FALSE(acts[4]->isEnabled());   // Walk（View.LookAndMove 未移植）
}

// Standard Views 弹出面板 = DTA 的 8 向 2×4 网格（StandardRotations.ts:9-18
// 顺序：top/bottom/left/right/front/back/iso/isoRight；:33-47 两行四个按钮）。
// Qt 形态：QToolButton + QMenu 内 QWidgetAction 包装的网格面板（8 个 QToolButton
// 按钮带 DTA 字形图标，objectName DTA.StdRot.<i> 表网格线性序）。
// Authored: no reference test exists in display-test-app for the Standard Views
//           panel; scenario transcribes StandardRotations.ts:9-18/:29-47.
TEST(DtaToolBarsViewTools, StandardViewsPanelHasEightDirectionButtonsInDtaOrder)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);

    // 经 widget 包装 action → QToolButton → 菜单 → 面板。
    QAction* sv = nullptr;
    for (QAction* a : bars.viewToolsToolBar()->actions())
        if (a->objectName() == QLatin1String("DTA.ViewTools.StandardRotations")) sv = a;
    ASSERT_NE(sv, nullptr);
    auto* svWa = qobject_cast<QWidgetAction*>(sv);
    ASSERT_NE(svWa, nullptr);
    auto* svBtn = qobject_cast<QToolButton*>(svWa->defaultWidget());
    ASSERT_NE(svBtn, nullptr);
    EXPECT_EQ(svBtn->menu(), nullptr);   // DTA DropDown 无箭头（ToolBar.ts:99-121）
    auto* svMenu = svBtn->findChild<QMenu*>(QStringLiteral("DTA.StdRot.Menu"));
    ASSERT_NE(svMenu, nullptr);          // 点击手动 popup 的菜单
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

// Analysis 工具栏：Undo/Redo（View.Undo/View.Redo 工具，真 action）+ 7 个置灰分析项
//（Viewer.ts:370-436）。
// Authored: no reference test exists in display-test-app for the analysis
//           toolbar; scenario transcribes Viewer.ts:370-436 (Undo/Redo enabled
//           actions + disabled analysis entries).
TEST(DtaToolBarsAnalysis, AnalysisUndoRedoEnabledRestDisabled)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    QList<QAction*> acts;
    for (QAction* a : bars.analysisToolBar()->actions()) if (!a->isSeparator()) acts.push_back(a);
    ASSERT_EQ(acts.size(), 9);
    const char* texts[] = { "Undo", "Redo", "Animation", "Sectioning", "Classification",
                            "Overrides", "Point Cloud", "Contours", "Format Set" };
    for (int i = 0; i < 9; ++i)
        EXPECT_EQ(acts[i]->text(), texts[i]) << i;
    EXPECT_TRUE(acts[0]->isEnabled());   // Undo → View.Undo 工具（ViewTool.ts:4111-4120）
    EXPECT_TRUE(acts[1]->isEnabled());   // Redo → View.Redo 工具（ViewTool.ts:4125-4134）
    for (int i = 2; i < 9; ++i)
        EXPECT_FALSE(acts[i]->isEnabled()) << texts[i];
}

// Authored: 无参考测试；DTA 语义（Surface.ts:97-119）= 无视口时视口工具不可用。
// DanQing 桌面形态：工具栏常显、整体置灰。2026-09-11：File 工具栏已移至 Start 页，
// 其余 5 条全部随视口置灰。
TEST(DtaToolBarsEnablement, ViewToolbarsDisabledWithoutViewport)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    // 初始无视口：5 条全部置灰。
    for (QToolBar* tb : { bars.viewsToolBar(), bars.selectionToolBar(),
                          bars.viewSettingsToolBar(), bars.viewToolsToolBar(), bars.analysisToolBar() })
        EXPECT_FALSE(tb->isEnabled());
}

// Authored: no reference test exists in display-test-app for viewport-presence
//           enablement (test app ships no tests); scenario transcribes the DTA
//           semantics (Surface.ts:97-119) that viewport tools require a selected
//           viewport — desktop form: toolbars always visible, disabled as a whole.
TEST(DtaToolBarsEnablement, EnabledWhenViewportSelected)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
    auto conn = dqApp::BlankConnection::create(props);
    auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
    auto* vp = dqApp::Viewport::Create(nullptr, view);
    dqApp::Application::Get().GetViewManager().AddViewport(vp);

    for (QToolBar* tb : { bars.viewsToolBar(), bars.selectionToolBar(),
                          bars.viewSettingsToolBar(), bars.viewToolsToolBar(), bars.analysisToolBar() })
        EXPECT_TRUE(tb->isEnabled());

    dqApp::Application::Get().GetViewManager().DropViewport(vp);
    for (QToolBar* tb : { bars.viewsToolBar(), bars.selectionToolBar(),
                          bars.viewSettingsToolBar(), bars.viewToolsToolBar(), bars.analysisToolBar() })
        EXPECT_FALSE(tb->isEnabled());
    delete vp;
}

// 2026-09-11 图标化（用户指令）：工具栏按钮全部换用 DTA 对应图标（Viewer.ts 的
// iconUnicode 字形 + .static-assets SVG）；ViewPicker 除外——参考是 <select> 文本
// 控件（ViewPicker.ts:176-190），无图标。
// Authored: no reference test exists in display-test-app (test app ships no tests);
//           icon spec transcribed from Viewer.ts:238-447 + StandardRotations.ts:9-18.
TEST(DtaToolBarsIcons, AllActionsCarryDtaIcons)
{
    QMainWindow mw;
    Gui::DtaToolBarSet bars(&mw);
    for (QToolBar* tb : { bars.viewsToolBar(), bars.selectionToolBar(),
                          bars.viewSettingsToolBar(), bars.viewToolsToolBar(),
                          bars.analysisToolBar() }) {
        for (QAction* a : tb->actions()) {
            if (a->isSeparator()) continue;
            // 跳过控件包装 action：ViewPicker（<select> 文本形态）与 Standard
            // Rotations（QToolButton 弹出面板，图标在按钮上，由专测覆盖）。
            if (a->objectName() == QLatin1String("DTA.Views.ViewPicker") ||
                a->objectName() == QLatin1String("DTA.ViewTools.StandardRotations"))
                continue;
            EXPECT_FALSE(a->icon().isNull())
                << qPrintable(QString("%1 / %2 must carry a DTA icon")
                                  .arg(tb->windowTitle(), a->text()));
        }
    }
}
