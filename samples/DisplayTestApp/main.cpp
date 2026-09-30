// DisplayTestApp — FreeCAD UI Shell + dqApp backend
//
// Architecture: FreeCAD UI framework (MainWindow, MDIView, QToolBar, QDockWidget)
//               + dqApp backend (Application, Viewport, IModelConnection, Tool)
//               + dqRender (OpenGL 4.1 rendering)
//
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/DisplayTestApp.ts
//              main entry — calls DisplayTestApp.startup() then waits for user input
#include <QApplication>
#include <QFile>
#include <QSurfaceFormat>
#include <QStyleFactory>
#include <QTimer>

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>

#ifdef _WIN32
// TEMP-DIAG（2026-09-19 真实 app 锚定崩溃取证）：vectored SEH 崩溃栈打印——
// 先于一切 __except 帧触发，stderr 落盘前 flush（参考 CursorStateTest 同款）。
// DANQING_CRASH_STACK=1 门控，默认零开销。
#include <windows.h>
#include <dbghelp.h>
static LONG WINAPI dtaCrashPrinter(EXCEPTION_POINTERS* ep)
{
    static HANDLE s_process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    SymInitialize(s_process, nullptr, TRUE);
    fprintf(stderr, "[CRASH] code=0x%08lx addr=%p\n",
            ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    void* frames[32] = {};
    WORD const n = CaptureStackBackTrace(0, 32, frames, nullptr);
    for (WORD i = 0; i < n; ++i) {
        char buf[sizeof(SYMBOL_INFO) + 256] = {};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        if (SymFromAddr(s_process, reinterpret_cast<DWORD64>(frames[i]), &disp, sym))
            fprintf(stderr, "[CRASH] #%u %s+0x%llx\n", i, sym->Name, static_cast<unsigned long long>(disp));
        else
            fprintf(stderr, "[CRASH] #%u %p\n", i, frames[i]);
    }
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif


#include <App/Application.h>

#include <dqApp/NotificationManager.h>
#include <dqApp/Application.h>
#include <dqApp/ViewTool.h>
#include <dqApp/IModelConnection.h>  // DANQING_AUTO_OPEN_DECO refit 用 GetProjectExtents

#include "src/Mod/Start/Gui/StartView.h"
#include "src/Mod/Start/Gui/ReadMeView.h"
#include "src/DumpOpenHelper.h"
#include "src/Gui/MainWindow.h"
#include "src/Gui/Application.h"
#include "src/Gui/DtaToolBars.h"
#include "src/Gui/DtaTools.h"
#include "src/Gui/TileTreePanel.h"
#include "src/Gui/CategoriesPanel.h"
#include "src/Gui/View3DInventor.h"
#include "src/Gui/DecorationGeometryExample.h"
#include "src/Gui/Command.h"
#include "src/Gui/CreateStdCommands.h"
#include "src/Gui/MenuManager.h"
#include "src/Gui/ToolBarManager.h"
#include "src/Gui/Workbench.h"

// Global stub instances
App::Application* App::Application::_pcSingleton = nullptr;
std::map<std::string, std::string> App::Application::m_config;

// === M-H(4)：Start 页双模型打开入口支撑 ===
// assets 根寻址（与 tests 同惯例——RpcDumpRenderTest.cpp:226-228）：编译期
// DANQING_TILE_ASSETS_DIR（DisplayTestApp 目标同 DtaTest 定义，CMakeLists）+
// 运行时 DANQING_RPC_DUMP env 覆写整个 dump 根。
#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

// 模型→包根映射表（一处集中，M-H(4) Step 2）：
//   joeshouse   = imodel joeshouse-v1/imodel.json + tiles [joeshouse-v1]
//                 + fallback [joeshouse-drill-v1（drill 域外键补字节，M-H Task 2）,
//                             joeshouse-drill-v2（M-I(5) P2c——缩远态 depth-1 键补字节）]
//   instances60 = imodel instances60-imodel-v1/imodel.json + tiles [instances60-v1]
//                 + fallback [instances60-drill-v1]
//   housemodel  = imodel housemodel-v1/imodel.json + tiles [housemodel-v1]
//                 + fallback [housemodel-drill-v1]（M-K(1)——House_Model.bim，
//                 首个 cameraOn=true 透视默认视图）
//   baytown     = imodel baytown-v1/imodel.json + tiles [baytown-v1]
//                 + fallback [baytown-drill-v1]（M-K(1)——Baytown.bim）
//   bridge-edit = imodel bridge-edit-v1/imodel.json + tiles [bridge-edit-v1]
//                 + fallback [bridge-edit-drill-v1（M-K(1)——编辑大桥测试.bim，
//                             ASCII 目录名映射；默认视图空域坑 24 → frameToWorldContent）,
//                             bridge-edit-sweep-v1（M-M(5)——全树 sweep 380,738 瓦/
//                             4.96GB[用户解除存储限制]；400k 瓦预算 cap 触顶，
//                             浏览深度界扩至 sweep 域；>100MB 大瓦本地持有）]
// （与 DumpOpenChainTest.cpp 各锁的包定义一致。）
std::optional<dta::DumpOpenPackage> dumpPackageForModel(QString const& modelId)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";
    if (char const* env = std::getenv("DANQING_RPC_DUMP"))
        dumpRoot = env;
    dta::DumpOpenPackage pkg;
    if (modelId == QLatin1String("joeshouse")) {
        pkg.imodelRoot = dumpRoot + "/joeshouse-v1";
        pkg.tileRoots = {dumpRoot + "/joeshouse-v1", dumpRoot + "/joeshouse-drill-v1",
                         dumpRoot + "/joeshouse-drill-v2"};
    }
    else if (modelId == QLatin1String("instances60")) {
        pkg.imodelRoot = dumpRoot + "/instances60-imodel-v1";
        pkg.tileRoots = {dumpRoot + "/instances60-v1", dumpRoot + "/instances60-drill-v1"};
    }
    else if (modelId == QLatin1String("housemodel")) {
        pkg.imodelRoot = dumpRoot + "/housemodel-v1";
        pkg.tileRoots = {dumpRoot + "/housemodel-v1", dumpRoot + "/housemodel-drill-v1"};
    }
    else if (modelId == QLatin1String("baytown")) {
        pkg.imodelRoot = dumpRoot + "/baytown-v1";
        pkg.tileRoots = {dumpRoot + "/baytown-v1", dumpRoot + "/baytown-drill-v1"};
    }
    else if (modelId == QLatin1String("bridge-edit")) {
        pkg.imodelRoot = dumpRoot + "/bridge-edit-v1";
        pkg.tileRoots = {dumpRoot + "/bridge-edit-v1", dumpRoot + "/bridge-edit-drill-v1",
                         dumpRoot + "/bridge-edit-sweep-v1"};
        // 坑 24：默认视图 0x99 指向原点附近空域（不含几何——直接回放=白屏
        // 零请求，README"默认视图空域"节）→ 打开链取景到世界域几何
        // contentRange（DumpOpenHelper 的 zoomToVolume 应用面）。
        pkg.frameToWorldContent = true;
    }
    else {
        return std::nullopt;
    }
    return pkg;
}

// 模型→MDI 窗口标题（卡片/入口的展示名——bridge-edit 卡片标注中文名映射，
// dump 目录名按 §11.11 用 ASCII）。
QString windowTitleForModel(QString const& modelId)
{
    if (modelId == QLatin1String("joeshouse"))
        return QObject::tr("Joe's House");
    if (modelId == QLatin1String("instances60"))
        return QObject::tr("60 Instances (Properties)");
    if (modelId == QLatin1String("housemodel"))
        return QObject::tr("House_Model");
    if (modelId == QLatin1String("baytown"))
        return QObject::tr("Baytown");
    if (modelId == QLatin1String("bridge-edit"))
        return QObject::tr("Bridge Edit (编辑大桥测试)");
    return modelId;
}

// 打开产物生命周期注册表已迁入 DumpOpenHelper（dta::registerOpenedDump/
// forgetOpenedDump/findOpenedDump——M-L(2)：Models/瓦树面板与打开链同源消费）。

}  // namespace


int main(int argc, char** argv)
{
    // 1. OpenGL format
    // Ported from: itwinjs-core DisplayTestApp.ts — RenderSystem.Options
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    // 2. QApplication
    QApplication app(argc, argv);
    app.setApplicationName("DisplayTestApp");
    app.setApplicationVersion("1.0.0");

#ifdef _WIN32
    if (std::getenv("DANQING_CRASH_STACK"))
        AddVectoredExceptionHandler(1, dtaCrashPrinter);
#endif

    // 2a. dqApp application startup — IModelApp.startup equivalent. Registers the
    // core tools (ToolAdmin.OnInitialized) and the always-on decorators
    // (ViewManager.onInitialized: accuSnap + toolAdmin). Without this the
    // SelectionTool is never installed and ToolAdmin::Decorate never runs — the
    // locate circle / snap cross / tool cursor chain is inert.
    // Ported from: itwinjs-core DisplayTestApp.startup() (App.ts).
    dqApp::Application::Get().Startup({});

    // 2a. Set Fusion style.
    // Ported from: FreeCAD src/Gui/Application.cpp:2921 setStyle()
    //              + FreeCADStyle : QProxyStyle("Fusion") (FreeCADStyle.h:40)
    //
    // FreeCAD applies a Fusion-based style at startup. On macOS the default
    // QMacStyle does not honor stylesheet :hover backgrounds for buttons; Fusion
    // (the FreeCAD baseline) fully honors the stylesheet.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // 3. Load FreeCAD stylesheet (default: Light theme)
    QString qssFile = QStringLiteral(":/freecad.qss");
    QFile styleFile(qssFile);
    if (styleFile.exists() && styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    // 4. Initialize App::Application singleton
    //    This bridges to dqApp::Application::Get().Startup() — real initialization.
    //    Ported from: itwinjs-core App.ts DisplayTestApp.startup()
    //                calls IModelApp.startup(opts)
    static App::Application appInstance;
    App::Application::_pcSingleton = &appInstance;

    // 5. Create MainWindow with FreeCAD Part Workbench menus/toolbars
    Gui::MainWindow* mainWindow = new Gui::MainWindow();
    // Ported from: FreeCAD src/Gui/main.cpp — Gui::Application::Instance()->setMainWindow(...).
    // Connect the dispatch facade to the main window so commands can reach the active view.
    Gui::Application::Instance()->setMainWindow(mainWindow);

    // Ported from: itwinjs-core Surface.ts openBlankConnection() — register the real
    // BlankConnection View3DInventor creator as the facade's new-view factory, so
    // StdCmdNew (and any other new-document caller) reaches real dqApp without the
    // command layer depending on View3DInventor directly.
    Gui::Application::Instance()->setNewViewFactory([mainWindow]() -> Gui::MDIView* {
        auto* view = new Gui::View3DInventor(nullptr, mainWindow, nullptr);  // nullptr → BlankConnection
        view->setWindowTitle(QObject::tr("3D View"));
        return view;
    });

    // No resize here — the window's size/position/maximized state is settled by
    // loadWindowSettings() below (FreeCAD StartupProcess.cpp:516-518 →
    // MainWindow.loadWindowSettings). Resizing the already-shown window from
    // the constructor's show was the maximize-state desync root cause.

    // 5a. Register all FreeCAD standard commands + activate StdWorkbench
    //     This builds menus/toolbars from command trees (replaces hand-written menus).
    // Ported from: FreeCAD Application::activateWorkbench -> Workbench::activate
    Gui::createStdCommands(mainWindow->commandManager());

    Gui::StdWorkbench wb;
    wb.setMainWindow(mainWindow);
    // M-L(2)：setManagers 第三参（ToolBarManager）已随 StdWorkbench::setupToolBars
    // 死树删除——main.cpp 传 nullptr 永不构建；工具栏区由下方 Gui::DtaToolBarSet
    // 按 DTA 功能分类重建。
    wb.setManagers(&mainWindow->commandManager(), &mainWindow->menuManager());
    wb.activate();

    // 6. Create StartView and add as tab
    auto* startView = new StartGui::StartView(mainWindow);
    startView->setWindowTitle(QObject::tr("Start"));
    mainWindow->addWindow(startView);

    // 7. Connect StartView signals to dqApp commands
    //    Ported from: itwinjs-core Surface.ts — openBlankConnection/openFileIModel
    //
    //    Architecture:
    //    StartView signal → MainWindow slot → Create View3DInventor (MDI window)
    //    View3DInventor wraps dqApp::Viewport (replaces Coin3D View3DInventorViewer)
    //    Viewport renders via dqRender OpenGL (replaces Coin3D SoGLRenderAction)
    //
    QObject::connect(startView, &StartGui::StartView::requestBlankConnection,
                     mainWindow, [mainWindow]() {
                         // Route through the Gui::Application facade so the blank-connection
                         // construction lives in one place (the factory registered above via
                         // setNewViewFactory). Ported from: itwinjs-core Surface.ts
                         // openBlankConnection() → Surface.createViewer({ iModel }).
                         Gui::Application::Instance()->newDocument();
                         mainWindow->showStatus(0, QObject::tr("Blank Connection opened"));
                     });
    // "Decoration Geometry Example"（Surface.ts:155-165）：新建 blank connection
    // 并装装饰——参考的 openBlankConnection + openDecorationGeometryExample 两步
    // 合一路径。
    QObject::connect(startView, &StartGui::StartView::requestDecorationGeometryExample,
                     mainWindow, [mainWindow]() {
                         auto* mdView = Gui::Application::Instance()->newDocument();
                         if (auto* view3d = qobject_cast<Gui::View3DInventor*>(mdView))
                             Gui::openDecorationGeometryExample(*view3d);
                         mainWindow->showStatus(0, QObject::tr("Decoration Geometry Example opened"));
                     });
    // M-H(4)：双模型打开入口（2026-09-28 用户指令——"点击直接进行渲染视图"）。
    // Start 页模型卡片 → 新建 MDI 视图（newDocument 工厂 = blank connection
    // View3DInventor）→ DumpOpenHelper 打开链（M-H Task 3：imodel.json →
    // saved ViewState → modelSelector 逐 model 树装载 → 多根 fetcher）→
    // saved 视图直接渲染。TileAdmin 全局 fetcher 装/卸按 DumpOpenHelper 既有
    // 语义（后开者替换前者；多视图并存的多 fetcher 路由登记范围外）。
    QObject::connect(startView, &StartGui::StartView::requestOpenDumpModel,
                     mainWindow, [mainWindow](QString modelId) {
                         // M-I(5) 终验计时（DANQING_OPEN_TRACE=1 门控，默认零开销）：
                         // 点击→打开链完成（saved view 已建、graphics 已提交）的耗时
                         // ——M-I(1) 性能清偿（18.7s→2.76s）的真实 app 侧取证。
                         auto const tOpen0 = std::chrono::steady_clock::now();
                         bool const openTrace =
                             std::getenv("DANQING_OPEN_TRACE") != nullptr;
                         if (openTrace) {
                             fprintf(stderr, "[OPEN] %s entry\n",
                                     modelId.toUtf8().constData());
                             fflush(stderr);
                         }
                         auto pkg = dumpPackageForModel(modelId);
                         if (!pkg.has_value()) {
                             mainWindow->showStatus(
                                 1, QObject::tr("Unknown model id: %1").arg(modelId));
                             return;
                         }
                         auto* mdView = Gui::Application::Instance()->newDocument();
                         auto* view3d = qobject_cast<Gui::View3DInventor*>(mdView);
                         if (view3d == nullptr) {
                             mainWindow->showStatus(
                                 1, QObject::tr("Open %1 failed: no 3D view").arg(modelId));
                             return;
                         }
                         auto opened = dta::openDumpIModel(*view3d, *pkg);
                         if (!opened.has_value()) {
                             mainWindow->showStatus(
                                 1, QObject::tr("Open %1 failed: dump package unreadable "
                                                "(assets root: %2)")
                                        .arg(modelId)
                                        .arg(QString::fromStdString(pkg->imodelRoot)));
                             return;
                         }
                         if (openTrace) {
                             auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - tOpen0).count();
                             fprintf(stderr, "[OPEN] %s chain-done ms=%lld trees=%zu\n",
                                     modelId.toUtf8().constData(),
                                     static_cast<long long>(ms), opened->treeLoadLog.size());
                             fflush(stderr);
                         }
                         view3d->setWindowTitle(windowTitleForModel(modelId));
                         QString const treeSummary = QString::fromStdString(
                             std::to_string(opened->treeLoadLog.size()));
                         dta::registerOpenedDump(view3d,
                             std::make_unique<dta::DumpOpenResult>(std::move(*opened)));
                         QObject::connect(view3d, &QObject::destroyed, mainWindow,
                                          [view3d]() { dta::forgetOpenedDump(view3d); });
                         mainWindow->showStatus(
                             0, QObject::tr("Opened %1 (%2 tile trees) — saved view rendered")
                                    .arg(modelId, treeSummary));
                     });

    // M-L(3) Task B：ReadMe 展示页入口（Start 页第三分组卡片 → 滚动只读页）。
    // 新建 MDI 视口（addWindow 与 StartView 同位——TabbedView 顶栏新 tab）。
    QObject::connect(startView, &StartGui::StartView::requestReadMe, mainWindow,
                     [mainWindow]() {
                         auto* readme = new StartGui::ReadMeView(mainWindow);
                         readme->setWindowTitle(QObject::tr("ReadMe"));
                         readme->resize(720, 640);
                         mainWindow->addWindow(readme);
                         mainWindow->showStatus(0, QObject::tr("ReadMe opened"));
                     });

    // DTA 功能分类工具栏区（替代原 FreeCAD 6 条 + 临时 2 条）。
    // Ported from: itwinjs-core display-test-app Surface.ts + Viewer.ts 工具栏组织。
    new Gui::DtaToolBarSet(mainWindow);  // QObject 挂在 mainWindow 上，随其析构

    // M-L(3)：DTA 工具注册（App.ts:393-458 SVTTools 扫描——keyin 可达的 app 工具）
    // + 状态栏装配（index.html status-bar div：keyin-entry / fps-container /
    // tileLoadIndicatorContainer——Surface.ts:52-60）+ ToolAssistance 提示接线
    //（ViewTool.ts:628-655 → InputHintWidget）。
    Gui::registerDtaTools();
    Gui::setupDtaStatusBar(mainWindow);
    Gui::setupToolAssistanceHints();

    // Models/瓦树停靠面板（M-L(2) 裁决档：原 FreeCAD ComboView 的
    // TreePanel+PropertyView 无文档后端恒空，改造为已打开 iModel 的
    // models/tile trees 陈列——数据源 = 打开产物注册表）。
    // 必须先于 loadWindowSettings（restoreWindowState 的 dock 布局要能命中它）。
    Gui::setupModelsPanel();

    // Categories 停靠面板（M-N(1)：per-category 可见性——ViewState
    // categorySelector 陈列 + 引擎 subCategory 可见性通道）。
    Gui::setupCategoriesPanel();

    // The ONE show: geometry + dock state, then (deferred) maximize — see
    // MainWindow::loadWindowSettings for the Windows DPI presentation note.
    // (Ported from FreeCAD StartupProcess.cpp:516-518 — loadWindowSettings is
    // called after the workbench/toolbars are active.)
    mainWindow->loadWindowSettings();

    // TEMP-DIAG（取证辅助，默认关）：DANQING_AUTO_OPEN_DECO=1 → 启动后自动打开
    // Decoration Geometry Example；=2 → 再自动激活 Rotate 工具（深度预览取证）。
    // 用途：真实 app 的桌面注入取证（合成点击在 Start 页卡片上不生效——
    // 2026-09-19 leave 取证 saga），绕开卡片点击直接进入用户复现场景。
    if (std::getenv("DANQING_AUTO_OPEN_DECO")) {
        int const mode = std::atoi(std::getenv("DANQING_AUTO_OPEN_DECO"));
        QTimer::singleShot(3500, mainWindow, [mainWindow, mode]() {
            auto* mdView = Gui::Application::Instance()->newDocument();
            if (auto* view3d = qobject_cast<Gui::View3DInventor*>(mdView)) {
                Gui::openDecorationGeometryExample(*view3d);
                // MDI 子窗口首帧是 100×100（VPEVT geom 轨迹），随后才最大化到
                // 全尺寸——open 时的 LookAtVolume 在退化视口上算错取景；待 MDI
                // 落定后补一次 refit（取证辅助，不影响默认路径）。
                QTimer::singleShot(2000, view3d, [view3d]() {
                    if (auto* vp = view3d->getUeViewport()) {
                        if (auto* vs = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr) {
                            if (auto* imodel = vp->GetIModel())
                                vs->LookAtVolume(imodel->GetProjectExtents());
                        }
                    }
                });
                if (mode >= 2) {
                    QTimer::singleShot(3200, view3d, [view3d]() {
                        if (auto* vp = view3d->getUeViewport())
                            (new dqApp::RotateViewTool(vp, /*oneShot=*/false))->run();
                    });
                }
            }
        });
    }

    // 8. Connect NotificationManager to MainWindow status bar
    //    Ported from: itwinjs-core display-test-app Notifications.ts
    //                outputMessage() → showStatus() / showError()
    //
    //    Map OutputMessagePriority to MainWindow StatusType:
    //      Error → Err(1), Warning → Wrn(2), others → Msg(4)
    dqApp::Application::Get().GetNotificationManager().OnMessageOutput.AddListener(
        [mainWindow](dqApp::NotifyMessageDetails const& details) {
            int statusType = 4;  // Msg
            if (details.priority == dqApp::OutputMessagePriority::Error ||
                details.priority == dqApp::OutputMessagePriority::Fatal) {
                statusType = 1;  // Err
            } else if (details.priority == dqApp::OutputMessagePriority::Warning) {
                statusType = 2;  // Wrn
            }
            mainWindow->showStatus(statusType, QString::fromStdString(details.briefMessage));
        });

    // 9. Event loop
    //    Ported from: itwinjs-core DisplayTestApp.ts — app.exec() / IModelApp.eventLoop()
    //    dqApp::Application::EventLoop() is driven by QTimer internally,
    //    triggered by Viewport::RequestRedraw() → Application::RequestNextAnimation().
    return app.exec();
}
