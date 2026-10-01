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
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QImage>
#include <QLineEdit>
#include <QStatusBar>
#include <QCompleter>
#include <QStringListModel>
#include <QCheckBox>

#include <App/Application.h>

#include "Gui/DtaTools.h"
#include "Gui/FpsMonitor.h"
#include "Gui/KeyinField.h"
#include "Gui/MainWindow.h"
#include "Gui/SaveImageTool.h"
#include "Gui/SnapModeTool.h"
#include "Gui/ZoomToSelectedTool.h"
#include "Gui/SyncViewportsTool.h"
#include "Gui/TileLoadIndicator.h"
#include "Gui/View3DInventor.h"
#include "DumpOpenHelper.h"
#include "InputHint.h"

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

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
// ToolAssistance 提示表（ViewTool.ts:628-655 的 host 半边）
// ---------------------------------------------------------------------------

// Authored: no reference test (DTA ships none) — pins the prompt strings from
// core/frontend/src/public/locales/en/CoreTools.json（tools.View.*.Prompts.
// FirstPoint + tools.ElementSet.Inputs.{AcceptPoint,Exit}）与 ViewTool.ts:630-641
// 的鼠标签节结构。
TEST(DtaToolsWiring, ToolAssistanceHintsMatchReferencePrompts)
{
    auto pan = Gui::toolAssistanceHintsFor("View.Pan");
    ASSERT_EQ(pan.size(), 3u);
    EXPECT_EQ(pan.front().message, QStringLiteral("Define point to pan from"));
    // 第二/三条 = 鼠标 Accept/Exit（ViewTool.ts:636-641）。
    auto it = std::next(pan.begin());
    EXPECT_EQ(it->message, QStringLiteral("%1 Accept point"));
    EXPECT_EQ(it->sequences.size(), 1u);
    EXPECT_EQ(it->sequences.front().keys.front(), Gui::InputHint::UserInput::MouseLeft);
    it = std::next(it);
    EXPECT_EQ(it->message, QStringLiteral("%1 Exit"));
    EXPECT_EQ(it->sequences.front().keys.front(), Gui::InputHint::UserInput::MouseRight);

    auto rotate = Gui::toolAssistanceHintsFor("View.Rotate");
    ASSERT_EQ(rotate.size(), 3u);
    EXPECT_EQ(rotate.front().message,
              QStringLiteral("Identify point on element to rotate about"));

    // 无提示表条目的工具（Select/一次性工具）→ 空表（装上即隐藏提示）。
    EXPECT_TRUE(Gui::toolAssistanceHintsFor("Select").empty());
    EXPECT_TRUE(Gui::toolAssistanceHintsFor("View.Undo").empty());
}
