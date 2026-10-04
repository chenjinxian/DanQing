// DtaToolsWiringTest — M-L(3) 接线级功能的宿主侧回归（Keyin 字段 / 诊断三小件 /
// 视口同步 / ToolAssistance 提示表）。
//
// Authored: no reference test exists in display-test-app for the keyin field /
//           status-bar widgets / sync tools (test app ships no tests). Each
//           scenario pins the reference behavior it ports from:
//             - KeyinField（frontend-devtools KeyinField.ts + Surface.ts:61-73）
//               → submit 走 ToolRegistry::parseAndRun（ToolRegistry.test.ts 契约
//               的引擎侧锁在 dqAppTest ToolRegistryKeyin*）；
//             - TileLoadIndicator（TileLoadIndicator.ts update：idle 时 1.0）；
//             - RecordFpsTool（FpsMonitor.ts run/update：Recording... → FPS x.xx）；
//             - SaveImageTool（SaveImageTool.ts run：readImageBuffer → PNG；
//               文件写半边的等价物——见 SaveImageTool.h EQUIVALENCE 注）；
//             - SyncViewportsTool（SyncViewportsTool.ts run/parseAndRun 语义）；
//             - ToolAssistance 提示表（ViewTool.ts:628-655 + CoreTools.json 字符串）。
#include <gtest/gtest.h>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QImage>
#include <QLineEdit>
#include <QShortcut>
#include <QStatusBar>
#include <QCompleter>
#include <QStringListModel>
#include <QCheckBox>

#include <App/Application.h>

#include "Gui/DtaTools.h"
#include "Gui/DtaToolBars.h"
#include "Gui/FpsMonitor.h"
#include "Gui/KeyinField.h"
#include "Gui/MainWindow.h"
#include "Gui/SaveImageTool.h"
#include "Gui/SnapModeTool.h"
#include "Gui/ZoomToSelectedTool.h"
#include "Gui/SyncViewportsTool.h"
#include "Gui/TileLoadIndicator.h"
#include "Gui/TileTreePanel.h"
#include "Gui/CategoriesPanel.h"
#include "Gui/View3DInventor.h"
#include "DumpOpenHelper.h"
#include "InputHint.h"

#include <dqApp/Application.h>
#include <dqApp/GltfDecoration.h>  // M-O(2) 3f——DropDecorator 的完整类型上转
#include <dqApp/GltfImport.h>  // M-O(2) 3f——InstallGltfDecoration
#include <dqApp/NotificationManager.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewTool.h>  // M-O(2) 3i——PanViewTool/RotateViewTool 事件源

#include <dqRender/GltfReader.h>  // M-O(2) 3f——GltfScene/GltfReader

#include "Gui/GltfDecorationTool.h"  // M-O(2) 3f——实例化缝

#include <algorithm>
#include <cmath>
#include <optional>
#include <dqCommon/GridOrientationType.h>

#include <cstdio>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {
struct QtEnvDtw {
    QtEnvDtw()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvDtw s_qtDtw;

// MainWindow ctor 读 App::GetApplication() 的参数组（FreeCAD MainWindow.cpp:84）
// ——单例先于首个 MainWindow 建立（DisplayTestAppTest 的 ensureAppReady 同款）。
void ensureAppStubReady()
{
    if (!App::Application::_pcSingleton)
        App::Application::_pcSingleton = new App::Application();
}

void ensureEngineReady()
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "DtaToolsWiring";
        opts.applicationVersion = "1.0";
        app.Startup(opts);
    }
    // App 工具扫册（App.ts:393-458 SVTTools 的宿主对应物——真实 app 在 main.cpp
    // 调用；keyin 测试与真实 app 同注册面）。
    Gui::registerDtaTools();
}

void spinDtw(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// NotificationManager 消息捕获（main.cpp 的状态栏监听的测试等价物）。
struct MessageCapture {
    dqBase::DqEventScope scope;
    std::vector<std::string> messages;
    MessageCapture()
    {
        scope.add(dqApp::Application::Get().GetNotificationManager().OnMessageOutput.AddListener(
            [this](dqApp::NotifyMessageDetails const& details) {
                messages.push_back(details.briefMessage);
            }));
    }
    bool hasPrefix(char const* prefix) const
    {
        for (auto const& m : messages)
            if (0 == m.compare(0, strlen(prefix), prefix))
                return true;
        return false;
    }
};

// 真窗口 3D 视口（空连接——注册进 ViewManager 成为活动视口）。
struct ViewGuardDtw {
    std::unique_ptr<Gui::View3DInventor> view;
    explicit ViewGuardDtw(int w = 400, int h = 300)
    {
        view = std::make_unique<Gui::View3DInventor>(nullptr, nullptr, nullptr);
        view->resize(w, h);
        view->show();
        spinDtw(300);
    }
    ~ViewGuardDtw() { view.reset(); }
    dqApp::Viewport* vp() const { return view->getUeViewport(); }
};
}  // namespace

// ---------------------------------------------------------------------------
// Keyin 字段（状态栏装配 + submit → registry）
// ---------------------------------------------------------------------------

// Authored: no reference test (DTA ships none) — pins Surface.ts:52-60 的状态栏
// 三件套装配（keyin-entry / fps-container / tileLoadIndicatorContainer）。
TEST(DtaToolsWiring, StatusBarAssemblyMountsKeyinFpsTileIndicator)
{
    ensureAppStubReady();
    Gui::MainWindow mw;
    Gui::setupDtaStatusBar(&mw);
    mw.show();
    qApp->processEvents();

    auto* keyin = mw.statusBar()->findChild<Gui::KeyinField*>();
    ASSERT_NE(keyin, nullptr);
    EXPECT_NE(keyin->completer(), nullptr);  // KeyinField.ts 自动补全列表

    auto* fps = mw.statusBar()->findChild<Gui::FpsMonitor*>();
    ASSERT_NE(fps, nullptr);
    auto* tiles = mw.statusBar()->findChild<Gui::TileLoadIndicator*>();
    ASSERT_NE(tiles, nullptr);

    // 自动补全列表含已注册 keyin（"select elements"——CoreTools.json Select 键）。
    QStringListModel* model
        = qobject_cast<QStringListModel*>(keyin->completer()->model());
    ASSERT_NE(model, nullptr);
    EXPECT_TRUE(model->stringList().contains(QStringLiteral("select elements")));
}

// Authored: no reference test (DTA ships none) — pins KeyinField.submitKeyin →
// ToolRegistry::parseAndRun → RecordFpsTool.run 的接线（"dta record fps" 键入
// 真的驱动帧率记录）。
TEST(DtaToolsWiring, KeyinFieldSubmitRunsRecordFpsThroughRegistry)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;
    MessageCapture capture;

    Gui::MainWindow mw;
    Gui::setupDtaStatusBar(&mw);
    auto* keyin = mw.statusBar()->findChild<Gui::KeyinField*>();
    ASSERT_NE(keyin, nullptr);

    keyin->setText(QStringLiteral("dta record fps 3"));
    QKeyEvent submit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(keyin, &submit);

    // RecordFpsTool：Recording...（run 开头）+ FPS x.xx（update 收尾）。
    EXPECT_TRUE(capture.hasPrefix("Recording...")) << "keyin submit did not reach RecordFpsTool";
    EXPECT_TRUE(capture.hasPrefix("FPS ")) << "record did not complete with an FPS report";
    // 历史推入（KeyinField.ts pushHistory——键入串在历史顶部）。
    // （历史是私有面——submit 后文本框清空即其可观测面。）
    EXPECT_TRUE(keyin->text().isEmpty());
}

// Authored: no reference test (DTA ships none — snap 模式面在 DTA 经 UI 设置
// 与 DrawingAid 快捷键，无离线断言对应物）——M-M(6) 接线锁：keyin
// `dta snapmode <mode>` → AccuSnap 活跃模式切换（App.ts:486-489
// setActiveSnapMode 的引擎通道）+ 模式名解析面 + 无参恢复默认。
TEST(DtaToolsWiring, SnapModeKeyinSetsAccuSnapActiveMode)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;
    MessageCapture capture;

    // 模式名解析面（SnapMode 枚举名 1:1——HitDetail.ts:22-32）。
    EXPECT_EQ(Gui::SetActiveSnapModeTool::parseMode("Intersection"),
              dqApp::SnapMode::Intersection);
    EXPECT_EQ(Gui::SetActiveSnapModeTool::parseMode("MidPoint"),
              dqApp::SnapMode::MidPoint);
    EXPECT_FALSE(Gui::SetActiveSnapModeTool::parseMode("Bogus").has_value());

    Gui::MainWindow mw;
    Gui::setupDtaStatusBar(&mw);
    auto* keyin = mw.statusBar()->findChild<Gui::KeyinField*>();
    ASSERT_NE(keyin, nullptr);

    auto activeMode = []() -> int {
        int n = 0;
        auto const* p = dqApp::Application::Get().GetAccuSnap().getActiveSnapModes(n);
        return n > 0 ? static_cast<int>(*p) : -1;
    };
    int const before = activeMode();

    // keyin 设 Intersection（位值 64）。
    keyin->setText(QStringLiteral("dta snapmode Intersection"));
    QKeyEvent submit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(keyin, &submit);
    EXPECT_EQ(64, activeMode()) << "snapmode keyin did not reach AccuSnap";
    EXPECT_TRUE(capture.hasPrefix("[SNAP] active snap mode = 0x40"));

    // 无参 → 恢复默认 NearestKeypoint（App.ts:76 初值 2）。
    keyin->setText(QStringLiteral("dta snapmode"));
    QApplication::sendEvent(keyin, &submit);
    EXPECT_EQ(2, activeMode()) << "bare snapmode keyin must restore NearestKeypoint";

    // 未知名 → 报错不改变当前模式。
    keyin->setText(QStringLiteral("dta snapmode Bogus"));
    QApplication::sendEvent(keyin, &submit);
    EXPECT_EQ(2, activeMode()) << "unknown mode must not change the active snap";
    EXPECT_TRUE(capture.hasPrefix("[SNAP] unknown snap mode 'Bogus'"));

    (void)before;
}

// Authored: no reference test (DTA ships none — UI 下拉在浏览器侧无离线断言) —
// M-O(1) I2 接线锁：状态栏 Snap Mode 下拉（SnapModes.ts:30-50 addSnapModes 的
// 宿主对应物）——8 项参考名序 + 默认 Keypoint + 单模式选择经 setActiveSnapMode、
// Multi-snap 经 setActiveSnapModes(7 模式数组)（SnapModes.ts:10-28）。
TEST(DtaToolsWiring, SnapModesComboBoxSwitchesAccuSnapActiveModes)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;

    Gui::MainWindow mw;
    Gui::setupDtaStatusBar(&mw);
    mw.show();
    qApp->processEvents();

    auto* combo = mw.statusBar()->findChild<QComboBox*>(QStringLiteral("snapModes"));
    ASSERT_NE(combo, nullptr) << "snapModes combo not mounted in status bar";

    // 8 项参考名序（SnapModes.ts:38-46 entries 逐项）。
    ASSERT_EQ(combo->count(), 8);
    EXPECT_EQ(combo->itemText(0).toStdString(), "Keypoint");
    EXPECT_EQ(combo->itemText(1).toStdString(), "Nearest");
    EXPECT_EQ(combo->itemText(2).toStdString(), "Center");
    EXPECT_EQ(combo->itemText(3).toStdString(), "Origin");
    EXPECT_EQ(combo->itemText(4).toStdString(), "Intersection");
    EXPECT_EQ(combo->itemText(5).toStdString(), "Perpendicular Point");
    EXPECT_EQ(combo->itemText(6).toStdString(), "Tangent Point");
    EXPECT_EQ(combo->itemText(7).toStdString(), "Multi-snap");

    auto activeModes = []() -> std::vector<int> {
        int n = 0;
        auto const* p = dqApp::Application::Get().GetAccuSnap().getActiveSnapModes(n);
        std::vector<int> out;
        for (int i = 0; i < n; ++i)
            out.push_back(static_cast<int>(p[i]));
        return out;
    };

    // 默认值 = NearestKeypoint（SnapModes.ts:35 value）。
    EXPECT_EQ(combo->currentIndex(), 0);
    EXPECT_EQ(activeModes(), std::vector<int>({2}));

    // 单模式：Center（位 8）→ 活跃数组恰 [Center]。
    combo->setCurrentIndex(2);
    qApp->processEvents();
    EXPECT_EQ(activeModes(), std::vector<int>({8}));

    // Multi-snap（值 -1）→ 7 模式数组（SnapModes.ts:10-18 逐项序）。
    combo->setCurrentIndex(7);
    qApp->processEvents();
    EXPECT_EQ(activeModes(), std::vector<int>({2, 1, 64, 4, 16, 8, 32}));

    // 回 Keypoint → 恢复单模式。
    combo->setCurrentIndex(0);
    qApp->processEvents();
    EXPECT_EQ(activeModes(), std::vector<int>({2}));
}

// Authored: no reference test (DTA ships none) — pins KeyinField 的 ToolNotFound
// 报错路径（KeyinField.ts:151-153 的报文文本）。
TEST(DtaToolsWiring, KeyinFieldUnknownKeyinReportsToolNotFound)
{
    ensureAppStubReady();
    ensureEngineReady();
    MessageCapture capture;

    Gui::MainWindow mw;
    Gui::setupDtaStatusBar(&mw);
    auto* keyin = mw.statusBar()->findChild<Gui::KeyinField*>();
    ASSERT_NE(keyin, nullptr);

    keyin->setText(QStringLiteral("definitely not a keyin"));
    QKeyEvent submit2(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(keyin, &submit2);
    ASSERT_FALSE(capture.messages.empty());
    EXPECT_EQ(capture.messages.back(),
              "Cannot find a key-in that matches: definitely not a keyin");
}

// ---------------------------------------------------------------------------
// 诊断三小件
// ---------------------------------------------------------------------------

// Authored: no reference test (DTA ships none) — pins TileLoadIndicator.update
// (TileLoadIndicator.ts:21-39) 的 idle 形态（无在途请求 → 1.0）。
TEST(DtaToolsWiring, TileLoadIndicatorShowsCompleteWhenIdle)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;

    Gui::TileLoadIndicator indicator;
    guard.vp()->RenderFrame();
    dqApp::Application::Get().GetViewManager().RenderLoop();
    qApp->processEvents();

    // 空连接无瓦请求：ready==total==0 → pct 1.0 → 满格（TileLoadIndicator.ts:37）。
    EXPECT_EQ(indicator.value(), indicator.maximum());
}

// Authored: no reference test (DTA ships none) — pins FpsMonitor.enabled
// (FpsMonitor.ts:59-76)：勾选 → 全视口 continuousRendering。
TEST(DtaToolsWiring, FpsMonitorCheckboxTogglesContinuousRendering)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;
    EXPECT_FALSE(guard.vp()->continuousRendering());

    Gui::FpsMonitor monitor;
    monitor.findChild<QCheckBox*>()->setChecked(true);
    EXPECT_TRUE(guard.vp()->continuousRendering());
    monitor.findChild<QCheckBox*>()->setChecked(false);
    EXPECT_FALSE(guard.vp()->continuousRendering());
}

// Authored: no reference test (DTA ships none) — pins SaveImageTool.run 的
// 文件写半边（SaveImageTool.ts:44-50 readImageBuffer → PNG；文件写 =
// openImageDataUrlInNewWindow 的桌面等价物——SaveImageTool.h EQUIVALENCE 注）。
TEST(DtaToolsWiring, SaveImageWritesFrameToPng)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;
    guard.vp()->RenderFrame();
    qApp->processEvents();

    std::string const path
        = std::string(DANQING_TILE_ASSETS_DIR) + "/../../build/ml3-saveimage-test.png";
    ASSERT_TRUE(Gui::SaveImageTool::writeFrameToFile(*guard.vp(), QString::fromStdString(path)));

    QImage image(QString::fromStdString(path));
    EXPECT_FALSE(image.isNull());
    // 回读 = 渲染目标的设备像素（CSS 尺寸 × devicePixelRatio——ReadFrameForTest
    // 同语义，Viewport.cpp readImageBuffer 注释）。
    auto const rect = guard.vp()->viewRect();
    double const dpr = guard.vp()->devicePixelRatioF();
    EXPECT_EQ(image.width(), qRound(rect.width() * dpr));
    EXPECT_EQ(image.height(), qRound(rect.height() * dpr));
    // 帧非全黑（空连接的背景/网格有内容）。
    long nonzero = 0;
    for (int y = 0; y < image.height(); y += 7)
        for (int x = 0; x < image.width(); x += 7)
            if (image.pixel(x, y) != 0xff000000)
                ++nonzero;
    EXPECT_GT(nonzero, 0);
}

// ---------------------------------------------------------------------------
// 视口同步（SyncViewportsTool → ViewportSync）
// ---------------------------------------------------------------------------

// Authored: no reference test (DTA ships none) — pins SyncViewportsTool.run/
// parseAndRun (SyncViewportsTool.ts:43-73) + connectViewportViews 的跟随语义
//（ViewportSync.ts:52-100）。
TEST(DtaToolsWiring, SyncViewportsAllConnectsAndFollows)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw a(400, 300);
    ViewGuardDtw b(400, 300);

    Gui::SyncViewportsTool tool;
    ASSERT_TRUE(tool.parseAndRun({"all"}));

    // A 换视图（clone + LookAtVolume 到不同域）→ B 跟随（clone 应用）。
    auto changed = a.vp()->GetView()->Clone();
    dqGeom::Range3d const far{-5000.0, -5000.0, -500.0, 5000.0, 5000.0, 500.0};
    changed->AsViewState3d()->LookAtVolume(far);
    a.vp()->ChangeView(std::move(changed));
    qApp->processEvents();

    auto const* viewA = a.vp()->GetView();
    auto const* viewB = b.vp()->GetView();
    ASSERT_NE(viewA, nullptr);
    ASSERT_NE(viewB, nullptr);
    EXPECT_NEAR(viewB->GetOrigin().x, viewA->GetOrigin().x, 1e-6);
    EXPECT_NEAR(viewB->GetOrigin().y, viewA->GetOrigin().y, 1e-6);
    EXPECT_NEAR(viewB->GetExtents().x, viewA->GetExtents().x, 1e-6);

    // 断开（run() 无参 = disconnect——SyncViewportsTool.ts:45-47）后 A 变 B 不动。
    double const bOriginXBefore = viewB->GetOrigin().x;
    double const bExtentsXBefore = viewB->GetExtents().x;
    ASSERT_TRUE(tool.run());
    auto changed2 = a.vp()->GetView()->Clone();
    changed2->AsViewState3d()->LookAtVolume(dqGeom::Range3d(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0));
    a.vp()->ChangeView(std::move(changed2));
    qApp->processEvents();
    // B 保持断开前的姿态（不随 A 的 1³ 盒 fit 变化）。
    EXPECT_DOUBLE_EQ(viewB->GetOrigin().x, bOriginXBefore);
    EXPECT_DOUBLE_EQ(viewB->GetExtents().x, bExtentsXBefore);
}

// Authored: no reference test (DTA ships none — 参考的 zoomToElements 依赖
// RPC getPlacements；M-N(2) 的离线对应物 = placements.json 回放表)——锁
// ZoomToSelectedElements 的三段：①placement 装载（instances60-placements-v1
// 62 行）②世界域计算（选中元素 → Placement 语义八角展开）③取景
// （LookAtVolume 后视域塌缩到元素盒量级）。资产钉值：0x38 球
// origin(-2.270,1.915,0) bbox ±0.531 angles(0,0,90)——世界域 ≈
// origin ± 0.531（Y90 旋转下 xy 对调不变并集）。
TEST(DtaToolsWiring, ZoomToSelectedFramesSelectedElement)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard(900, 640);

    dta::DumpOpenPackage pkg;
    std::string const dumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";
    pkg.imodelRoot = dumpRoot + "/instances60-placements-v1";
    pkg.tileRoots = {dumpRoot + "/instances60-placements-v1"};
    auto opened = dta::openDumpIModel(*guard.view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open failed: " << pkg.imodelRoot;

    // ① placement 装载（placements.json 随 open——62 行）。
    auto* conn = opened->connection.Get();
    ASSERT_NE(conn, nullptr);
    ASSERT_EQ(conn->getPlacements().size(), 62u)
        << "placements.json not loaded alongside imodel.json";
    auto const* p38 = conn->findPlacement(dqBase::DqId::FromString("0x38"));
    ASSERT_NE(p38, nullptr);
    EXPECT_NEAR(p38->origin[0], -2.270142702156284, 1e-9);
    EXPECT_NEAR(p38->bboxHigh[0], 0.5309601873536304, 1e-9);
    EXPECT_NEAR(p38->angles[2], 90.0, 1e-9);

    // ② 世界域计算（zoomToPlacements 的 placements→volume 步——静态直驱）。
    auto volume = Gui::ZoomToSelectedElementsTool::computeSelectedVolume(
        *conn, {0x38u});
    ASSERT_TRUE(volume.has_value());
    // Y90 旋转下球盒对称——世界域 = origin ± 0.531（八角并集对对称盒
    // 旋转不变）。LookAtVolume 的 x1.04 膨胀不在此步（volume 是纯盒）。
    EXPECT_NEAR(volume->low.x, -2.270142702156284 - 0.5309601873536304, 1e-6);
    EXPECT_NEAR(volume->high.x, -2.270142702156284 + 0.5309601873536304, 1e-6);
    EXPECT_NEAR(volume->low.y, 1.9146355987602088 - 0.5309601873536304, 1e-6);
    EXPECT_NEAR(volume->high.y, 1.9146355987602088 + 0.5309601873536304, 1e-6);
    // z：origin.z=0，盒 ±0.531（Y90 不触 z）。
    EXPECT_NEAR(volume->high.z, 0.5309601873536304, 1e-6);

    // 多元素并集（两个球——域为两盒并集，跨度 > 单盒）。
    auto volume2 = Gui::ZoomToSelectedElementsTool::computeSelectedVolume(
        *conn, {0x38u, 0x39u});
    ASSERT_TRUE(volume2.has_value());
    double const span1 = volume->high.x - volume->low.x;
    double const span2 = volume2->high.x - volume2->low.x;
    EXPECT_GT(span2, span1);

    // 空选集 → 空域（Viewer.ts:43-45 的 0 < elems.size 门）。
    auto empty = Gui::ZoomToSelectedElementsTool::computeSelectedVolume(*conn, {});
    EXPECT_FALSE(empty.has_value());

    guard.view->close();
}

// Authored: no reference test (DTA ships none) — pins SyncViewportFrustaTool 的
// frusta-only 语义（SyncViewportsTool.ts:91-99 → synchronizeViewportFrusta
// ViewportSync.ts:114-122 的 savePose/applyPose）。
TEST(DtaToolsWiring, SyncFrustaAppliesPoseAcrossViews)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw a(400, 300);
    ViewGuardDtw b(400, 300);

    Gui::SyncViewportFrustaTool tool;
    ASSERT_TRUE(tool.parseAndRun({"all"}));

    auto changed = a.vp()->GetView()->Clone();
    dqGeom::Range3d const far{-2000.0, -3000.0, -400.0, 2000.0, 3000.0, 400.0};
    changed->AsViewState3d()->LookAtVolume(far);
    a.vp()->ChangeView(std::move(changed));
    qApp->processEvents();

    // frusta 同步把 A 的姿态应用到 B 的现有视图（origin/extents 对齐）。
    auto const* viewA = a.vp()->GetView();
    auto const* viewB = b.vp()->GetView();
    EXPECT_NEAR(viewB->GetExtents().x, viewA->GetExtents().x, 1e-6);
    EXPECT_NEAR(viewB->GetExtents().y, viewA->GetExtents().y, 1e-6);
}

// ---------------------------------------------------------------------------
// ToolAssistance 提示链（ViewTool.ts:628-655 的 host 半边——M-O(2) 3i 事件驱动）
// ---------------------------------------------------------------------------

// Authored: no reference test (DTA ships none) — pins the prompt strings from
// core/frontend/src/public/locales/en/CoreTools.json（tools.View.*.Prompts.
// FirstPoint + tools.ElementSet.Inputs.{AcceptPoint,Exit}）与 ViewTool.ts:630-641
// 的鼠标签节结构。M-O(2) 3i：payload 来自引擎 provideToolAssistance 实装
//（OnToolAssistance 事件）——不再用 install-time toolId 表。
TEST(DtaToolsWiring, ToolAssistanceHintsMatchReferencePrompts)
{
    ensureEngineReady();
    ViewGuardDtw guard;
    auto* vp = guard.vp();
    ASSERT_NE(vp, nullptr);

    // 引擎事件 payload（PanViewTool.provideToolAssistance 实装面）。
    std::optional<dqApp::ToolAssistanceInstructions> payload;
    auto disconnect = dqApp::Application::Get().GetNotificationManager()
                          .OnToolAssistance.AddListener(
                              [&payload](dqApp::ToolAssistanceInstructions const& i) {
                                  payload = i;
                              });
    dqApp::PanViewTool pan(vp);
    pan.provideToolAssistance("Pan.Prompts.FirstPoint");
    disconnect();
    ASSERT_TRUE(payload.has_value());

    auto panHints = Gui::toolAssistanceHintsFor(*payload);
    ASSERT_EQ(panHints.size(), 3u);
    EXPECT_EQ(panHints.front().message, QStringLiteral("Define point to pan from"));
    // 第二/三条 = 鼠标 Accept/Exit（ViewTool.ts:636-641）。
    auto it = std::next(panHints.begin());
    EXPECT_EQ(it->message, QStringLiteral("%1 Accept point"));
    EXPECT_EQ(it->sequences.size(), 1u);
    EXPECT_EQ(it->sequences.front().keys.front(), Gui::InputHint::UserInput::MouseLeft);
    it = std::next(it);
    EXPECT_EQ(it->message, QStringLiteral("%1 Exit"));
    EXPECT_EQ(it->sequences.front().keys.front(), Gui::InputHint::UserInput::MouseRight);

    // Rotate 主指令（同引擎面）。
    payload.reset();
    disconnect = dqApp::Application::Get().GetNotificationManager()
                     .OnToolAssistance.AddListener(
                         [&payload](dqApp::ToolAssistanceInstructions const& i) {
                             payload = i;
                         });
    dqApp::RotateViewTool rotate(vp);
    rotate.provideToolAssistance("Rotate.Prompts.FirstPoint");
    disconnect();
    ASSERT_TRUE(payload.has_value());
    auto rotateHints = Gui::toolAssistanceHintsFor(*payload);
    ASSERT_EQ(rotateHints.size(), 3u);
    EXPECT_EQ(rotateHints.front().message,
              QStringLiteral("Identify point on element to rotate about"));
}

// M-O(1) I5：grid 设置 keyin（Grid.ts:11-94 ChangeGridSettingsTool——
// s/r/g/o/l 参数面 + 逐参写 + invalidateScene）。
// Authored: no reference test exists in display-test-app for the grid tool
//           (test app ships no tests); scenario transcribes Grid.ts:21-34/:46-93
//           verbatim（s=2.5 → spacing x=y；r=2 → y=x*2；g=12；o=2=WorldYZ；
//           l=true → ToolAdmin.gridLock）。
TEST(DtaToolsWiring, GridSettingsKeyinChangesViewDetails)
{
    ensureAppStubReady();
    ensureEngineReady();
    ViewGuardDtw guard;
    auto* vp = guard.vp();
    ASSERT_NE(vp, nullptr);
    auto* v3 = vp->GetView()->AsViewState3d();
    ASSERT_NE(v3, nullptr);

    Gui::MainWindow mw;
    Gui::setupDtaStatusBar(&mw);
    auto* keyin = mw.statusBar()->findChild<Gui::KeyinField*>();
    ASSERT_NE(keyin, nullptr);

    // 基线（ViewDetails.ts:24-31 默认：WorldXY/10/{1,1}）。
    EXPECT_EQ(v3->getGridOrientation(), dqCommon::GridOrientationType::WorldXY);
    EXPECT_EQ(v3->getGridsPerRef(), 10);

    // 参考声明 maxArgs=4（Grid.ts:14）——l 与 s/r/g/o 分两段键入（5 参超限
    // 会被 ToolRegistry 的 maxArgs 门拒绝——契约同 ToolRegistryKeyin 锁）。
    keyin->setText(QStringLiteral("dta grid settings s=2.5 r=2 g=12 o=2"));
    QKeyEvent submit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(keyin, &submit);

    auto const spacing = v3->getGridSpacing();
    EXPECT_NEAR(spacing.x, 2.5, 1e-12) << "s= 未生效";
    EXPECT_NEAR(spacing.y, 5.0, 1e-12) << "r= 未生效（y 应为 x*ratio）";
    EXPECT_EQ(v3->getGridsPerRef(), 12) << "g= 未生效";
    EXPECT_EQ(v3->getGridOrientation(), dqCommon::GridOrientationType::WorldYZ) << "o=2 未映射 WorldYZ";

    keyin->setText(QStringLiteral("dta grid settings l=1"));
    QApplication::sendEvent(keyin, &submit);
    EXPECT_TRUE(dqApp::Application::Get().GetToolAdmin().isGridLocked()) << "l= 未生效";
}

// M-O(1) R2：Models/Categories 工具栏按钮 toggle dock 面板（Viewer.ts:274-293
// picker 位——面板宿主为 dock 的 EQUIVALENCE 见 DtaToolBars.cpp 注）。
TEST(DtaToolsWiring, PanelToggleButtonsSwitchDockPanels)
{
    ensureAppStubReady();
    ensureEngineReady();

    Gui::MainWindow mw;
    mw.show();
    qApp->processEvents();
    Gui::setupModelsPanel();
    Gui::setupCategoriesPanel();

    Gui::DtaToolBarSet toolbars(&mw);
    qApp->processEvents();

    auto* modelsAction = mw.findChild<QAction*>(QStringLiteral("DTA.PanelToggle.Models"));
    ASSERT_NE(modelsAction, nullptr) << "Models 面板开关按钮未注册";
    auto* categoriesAction = mw.findChild<QAction*>(QStringLiteral("DTA.PanelToggle.Categories"));
    ASSERT_NE(categoriesAction, nullptr);

    auto findDock = [&mw](QString const& name) -> QDockWidget* {
        for (auto* d : mw.findChildren<QDockWidget*>())
            if (d->windowTitle() == name)
                return d;
        return nullptr;
    };
    QDockWidget* modelsDock = findDock(QStringLiteral("Models"));
    ASSERT_NE(modelsDock, nullptr) << "Models dock 面板未装配";
    ASSERT_NE(findDock(QStringLiteral("Categories")), nullptr);

    // toggle 双向语义（确定性前置：面板隐藏 + action 未勾选 → 触发=勾选=显示；
    // 再触发=取消勾选=隐藏）。
    modelsDock->setVisible(false);
    modelsAction->setChecked(false);
    modelsAction->trigger();
    qApp->processEvents();
    EXPECT_TRUE(modelsDock->isVisibleTo(&mw)) << "勾选触发未显示面板";
    modelsAction->trigger();
    qApp->processEvents();
    EXPECT_FALSE(modelsDock->isVisibleTo(&mw)) << "取消勾选未隐藏面板";
}

// M-O(1) I7：DTA 快捷键族 MDI 子集注册（Surface.ts:240-258——Ctrl+[ ]/\|；
// 行为半边由 MainWindow::activate*Window 与 MDIChromeTest/CommandWindowTest 锁）。
TEST(DtaToolsWiring, DtaShortcutsRegistered)
{
    ensureAppStubReady();
    Gui::MainWindow mw;
    Gui::setupDtaShortcuts(&mw);

    for (char const* name : {"FocusPrev", "FocusNext", "CloneView", "CloseView"}) {
        auto* sc = mw.findChild<QShortcut*>(QStringLiteral("DTA.Shortcut.") + name);
        ASSERT_NE(sc, nullptr) << name << " 快捷键未注册";
        EXPECT_EQ(sc->parent(), &mw);
    }
}

// ---------------------------------------------------------------------------
// M-O(2) 3f——glTF 实例化开关（GltfDecoration.ts:103-228）。
// ---------------------------------------------------------------------------

// Authored: no reference test exists in display-test-app for the glTF
//           decoration tool (test app ships no tests); scenario transcribes
//           GltfDecoration.ts:58-66（scale 域 [0.25,2.5] / 关闭时恒等）与
//           :41-53（位置域 ±maxExtent 逐分量随机）。
TEST(DtaToolsWiring, GltfDecorationToolCreateTransformDomains)
{
    ensureEngineReady();
    constexpr double kMaxExtent = 100.0;
    // 关闭两开关：matrix 恒等（:60 scaleFactor=1 / :63 zAngle=0）。
    {
        auto const tf = Gui::GltfDecorationTool::createGltfInstanceTransform(
            kMaxExtent, /*wantScale=*/false, /*wantRotate=*/false);
        auto const& identity = dqGeom::Matrix3d::CreateIdentity();
        for (int i = 0; i < 9; ++i)
            EXPECT_NEAR(identity.coffs[static_cast<size_t>(i)],
                        tf.matrix.coffs[static_cast<size_t>(i)], 1.0e-9)
                << "coffs[" << i << "]";
    }
    // 全开：合成 origin = t + P − sR·P（参考 :56-66 的乘积序
    // translation×scale×rotation——scale/rotation 均固定于随机点 P），
    // |origin| ≤ |t|+|P|+s|P| ≤ (2+s)·maxExtent ≤ 4.5·maxExtent；缩放域
    // [0.25, 2.5]（行范数——uniform scale 三行同范数，:58-59）。
    for (int trial = 0; trial < 25; ++trial) {
        auto const tf = Gui::GltfDecorationTool::createGltfInstanceTransform(
            kMaxExtent, /*wantScale=*/true, /*wantRotate=*/true);
        EXPECT_LE(std::abs(tf.origin.x), kMaxExtent * 4.5 + 1.0e-6);
        EXPECT_LE(std::abs(tf.origin.y), kMaxExtent * 4.5 + 1.0e-6);
        EXPECT_LE(std::abs(tf.origin.z), kMaxExtent * 4.5 + 1.0e-6);
        double const rowNorm = std::sqrt(
            tf.matrix.coffs[0] * tf.matrix.coffs[0] +
            tf.matrix.coffs[1] * tf.matrix.coffs[1] +
            tf.matrix.coffs[2] * tf.matrix.coffs[2]);
        EXPECT_GE(rowNorm, 0.25 - 1.0e-6);
        EXPECT_LE(rowNorm, 2.5 + 1.0e-6);
    }
}

// Authored: no reference test exists in display-test-app for the glTF
//           decoration tool; scenario transcribes GltfDecoration.ts:76-88
//           （七色循环）与 :87-94 的逐实例装配面（N 份 mesh + 烘后恒等）。
TEST(DtaToolsWiring, GltfDecorationToolBuildInstancedSceneCyclesColors)
{
    ensureEngineReady();
    // 单 mesh 源（BoxTexturedDots——树内不对称标记资产）。
    auto src = dqRender::GltfReader::LoadFromFile(
        DANQING_GLTF_ASSETS_DIR "/BoxTexturedDots/BoxTextured.gltf");
    ASSERT_NE(src, nullptr);
    ASSERT_FALSE(src->meshes.empty());
    size_t const srcMeshCount = src->meshes.size();
    float const srcAlpha = src->meshes[0].baseColorFactor[3];

    int const kNumInstances = 5;
    auto instanced = Gui::GltfDecorationTool::buildInstancedScene(
        *src, kNumInstances, /*maxExtent=*/3.0,
        /*wantScale=*/false, /*wantColor=*/true, /*wantRotate=*/false);
    ASSERT_NE(instanced, nullptr);
    ASSERT_EQ(kNumInstances * srcMeshCount, instanced->meshes.size());

    // 七色循环（:77-84 序：green/blue/red/white/yellow/orange/black——
    // 前 5 实例 = green/blue/red/white/yellow）；alpha 透传。
    static float const kExpected[5][3] = {
        {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f},
    };
    for (int i = 0; i < kNumInstances; ++i) {
        auto const& mesh = instanced->meshes[static_cast<size_t>(i) * srcMeshCount];
        for (int c = 0; c < 3; ++c)
            EXPECT_NEAR(kExpected[i][c], mesh.baseColorFactor[c], 1.0e-6f)
                << "instance " << i << " channel " << c;
        EXPECT_FLOAT_EQ(srcAlpha, mesh.baseColorFactor[3]);
        // 烘后 mesh transform 恒等（buildInstancedScene 的烘变换契约）。
        auto const& identity = dqGeom::Matrix3d::CreateIdentity();
        for (int e = 0; e < 9; ++e)
            EXPECT_NEAR(identity.coffs[static_cast<size_t>(e)],
                        mesh.transform.matrix.coffs[static_cast<size_t>(e)], 1.0e-9);
    }

    // 位置展开：5 实例的原点两两距离 > 0（随机位置——几乎必然不重合）。
    for (size_t a = 0; a < instanced->meshes.size(); a += srcMeshCount) {
        for (size_t b = a + srcMeshCount; b < instanced->meshes.size(); b += srcMeshCount) {
            auto const& pa = instanced->meshes[a].polyface->Data().points[0];
            auto const& pb = instanced->meshes[b].polyface->Data().points[0];
            double const dist = std::sqrt(
                (pa.x - pb.x) * (pa.x - pb.x) + (pa.y - pb.y) * (pa.y - pb.y) +
                (pa.z - pb.z) * (pa.z - pb.z));
            EXPECT_GT(dist, 1.0e-6) << "instances " << a << "/" << b;
        }
    }
}

// 像素锁（§11.11 WHERE 断言）：5 实例散布 ±maxExtent 后上屏——内容在
// 水平/垂直两轴的多簇展开（单实例立方体会聚为单一中央块）。
// Authored: no reference test exists in display-test-app for the glTF
//           decoration tool; scenario = GltfDecoration.ts:180-204 装配 +
//           :41-53 的散布域（maxExtent=3.0 的受控数据面——机制即 :74-75 的
//           projectExtents 对角线最小分量，值随数据面）。
TEST(DtaToolsWiring, GltfDecorationInstancesRenderSeparatedClusters)
{
    ensureEngineReady();
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < 400)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    auto src = dqRender::GltfReader::LoadFromFile(
        DANQING_GLTF_ASSETS_DIR "/BoxTexturedDots/BoxTextured.gltf");
    ASSERT_NE(src, nullptr);
    // 固定种子 = 确定性布局（生产 seed=0 随机——参考 Math.random）。
    auto instanced = Gui::GltfDecorationTool::buildInstancedScene(
        *src, /*numInstances=*/5, /*maxExtent=*/3.0,
        /*wantScale=*/false, /*wantColor=*/false, /*wantRotate=*/false,
        /*seed=*/42);
    ASSERT_NE(instanced, nullptr);

    auto decoration = dqApp::InstallGltfDecoration(
        *view.getUeViewport(), std::move(instanced), "BoxTexturedDots-x5");
    ASSERT_NE(decoration, nullptr);
    // InstallGltfDecoration 的 fit 是即时的（InvalidateController 非 synchWithView
    // 动画——GltfImport.cpp:44-49 登记）；渲染一帧后取帧。
    {
        qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
        while (QDateTime::currentMSecsSinceEpoch() - t0 < 300)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    view.getUeViewport()->RenderFrame();

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(view.getUeViewport()->ReadFrameForTest(frame, w, h));

    auto const px = [&](uint32_t x, uint32_t y) -> size_t {
        return (static_cast<size_t>(y) * w + x) * 4;
    };
    // 逐行背景 = 该行中位色（空白连接的天空渐变竖直单向——行内恒定；实例
    // 像素为离群）。内容 = 与行中位差的 L1 > 120。
    auto const rowContentGroups = [&](uint32_t y, int& groups,
                                      int& contentCols) {
        std::vector<uint8_t> rs, gs, bs;
        rs.reserve(w); gs.reserve(w); bs.reserve(w);
        for (uint32_t x = 0; x < w; x += 3) {
            rs.push_back(frame[px(x, y)]);
            gs.push_back(frame[px(x, y) + 1]);
            bs.push_back(frame[px(x, y) + 2]);
        }
        std::sort(rs.begin(), rs.end());
        std::sort(gs.begin(), gs.end());
        std::sort(bs.begin(), bs.end());
        double const mr = rs[rs.size() / 2], mg = gs[gs.size() / 2],
                     mb = bs[bs.size() / 2];
        groups = 0;
        contentCols = 0;
        bool inGroup = false;
        for (uint32_t x = 0; x < w; ++x) {
            bool const content =
                std::abs(frame[px(x, y)] - mr) +
                    std::abs(frame[px(x, y) + 1] - mg) +
                    std::abs(frame[px(x, y) + 2] - mb) > 120.0;
            if (content) {
                ++contentCols;
                if (!inGroup) {
                    ++groups;
                    inGroup = true;
                }
            } else {
                inGroup = false;
            }
        }
    };

    // 扫描行带（h/16 步）取列组峰值——5 实例散布后至少一行切过 ≥2 个水平
    // 分离的实例剪影（单实例立方体在任意行至多 1 组）。
    int maxGroups = 0, maxGroupsRow = 0, maxRowContentCols = 0;
    for (uint32_t y = h / 16; y < h - h / 16; y += h / 16) {
        int groups = 0, contentCols = 0;
        rowContentGroups(y, groups, contentCols);
        if (groups > maxGroups) {
            maxGroups = groups;
            maxGroupsRow = static_cast<int>(y);
            maxRowContentCols = contentCols;
        }
    }
    printf("[GLTFINST] frame=%ux%u maxGroups=%d@row=%d rowContentCols=%d\n",
           w, h, maxGroups, maxGroupsRow, maxRowContentCols);

    // WHERE 断言：水平多簇分离（≥2 列组——散布实例剪影）。
    EXPECT_GE(maxGroups, 2);

    // 清理（ViewManager 非拥有指针——销毁前 Drop）。
    dqApp::Application::Get().GetViewManager().DropDecorator(decoration.get());
}
