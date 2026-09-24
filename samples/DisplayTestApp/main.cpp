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
#include "src/Gui/MainWindow.h"
#include "src/Gui/Application.h"
#include "src/Gui/DtaToolBars.h"
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
    // 第三参 nullptr：activate() 的 `if (m_tm)` 守卫跳过 FreeCAD 工具栏构建（菜单照常），
    // 工具栏区由下方 Gui::DtaToolBarSet 按 DTA 功能分类重建。
    wb.setManagers(&mainWindow->commandManager(), &mainWindow->menuManager(), nullptr);
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
    QObject::connect(startView, &StartGui::StartView::requestOpenFile,
                     mainWindow, [mainWindow]() {
                         // TODO Step 3/4: File dialog + open IModelConnection
                         mainWindow->showStatus(0, QObject::tr("Open File — not yet implemented"));
                     });
    QObject::connect(startView, &StartGui::StartView::requestNewFile,
                     mainWindow, [mainWindow]() {
                         // TODO Step 4: File dialog + glTF import
                         mainWindow->showStatus(0, QObject::tr("New File — not yet implemented"));
                     });

    // DTA 功能分类工具栏区（替代原 FreeCAD 6 条 + 临时 2 条）。
    // Ported from: itwinjs-core display-test-app Surface.ts + Viewer.ts 工具栏组织。
    new Gui::DtaToolBarSet(mainWindow);  // QObject 挂在 mainWindow 上，随其析构

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
