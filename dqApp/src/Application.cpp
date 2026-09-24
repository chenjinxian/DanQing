// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Application singleton implementation
// Ported from: itwinjs-core core/frontend/src/IModelApp.ts
#include "dqApp/Application.h"

#include "FileTileFetcher.h"
#include "QtTileRequestFetcher.h"

#include <dqRender/tile/TileAdmin.h>

#include <QCoreApplication>
#include <QTimer>

#include <memory>

namespace dqApp {

Application::Application() = default;
Application::~Application() { shutdown(); }

Application& Application::Get()
{
    static Application instance;
    return instance;
}

bool Application::Startup(Options const& opts)
{
    if (m_initialized) return true;

    // 1. Save configuration (← IModelApp: this._applicationId, this._applicationVersion)
    m_applicationId = opts.applicationId;
    m_applicationVersion = opts.applicationVersion;
    m_renderSystemOptions = opts.renderSystemOptions;

    // 2. All subsystems are already constructed as member variables.
    //    (← IModelApp: this._toolAdmin = opts.toolAdmin ?? new ToolAdmin())
    //    (← IModelApp: this._notifications = opts.notifications ?? new NotificationManager())
    //    (← IModelApp: this._viewManager = opts.viewManager ?? new ViewManager())
    //    (← IModelApp: this._accuDraw = opts.accuDraw ?? new AccuDraw())
    //    (← IModelApp: this._accuSnap = opts.accuSnap ?? new AccuSnap())
    //    (← IModelApp: this._locateManager = opts.locateManager ?? new ElementLocateManager())
    //    (← IModelApp: this._tentativePoint = opts.tentativePoint ?? new TentativePoint())
    //    (← IModelApp: this._quantityFormatter = opts.quantityFormatter ?? new QuantityFormatter())
    //    (← IModelApp: this._uiAdmin = opts.uiAdmin ?? new UiAdmin())

    // 3. Register core entity states.
    //    Ported from: itwinjs-core IModelApp.startup() entity registration
    //                (IModelApp.ts:409-421)
    RegisterCoreEntityStates();

    // 4. Notify ALL subsystems of initialization.
    //    Ported from: itwinjs-core IModelApp.startup() sys.onInitialized()
    //                (IModelApp.ts:448-457)
    //    [this.renderSystem, this.viewManager, this.toolAdmin, this.accuDraw,
    //     this.accuSnap, this.locateManager, this.tentativePoint, this.uiAdmin
    //    ].forEach((sys) => sys.onInitialized());
    m_toolAdmin.OnInitialized();            // registers core tools (default tool starts on first viewport selection — ViewManager.cpp)
    m_accuDraw.onInitialized();
    m_accuSnap.onInitialized();
    m_locateManager.onInitialized();
    m_tentativePoint.onInitialized();
    m_quantityFormatter.onInitialized();
    m_uiAdmin.onInitialized();

    // Ported from: itwinjs-core ViewManager.onInitialized (ViewManager.ts:126-130)：
    // addDecorator(accuSnap) + addDecorator(toolAdmin)（参考还注册 tentativePoint/
    // accuDraw——DanQing 该两件无可 decorate 内容，暂不接；AcsTriadDecorator 已在
    // ViewManager 构造注册）。accuSnap 的 decorate 现承载 snap 十字 sprite
    // (AccuSnap.ts:1208-1220)。
    // 时机说明：ViewManager/ToolAdmin 同为 Application 成员，须待全部构造完成——
    // Startup 接线与参考 onInitialized 时机等价。
    m_viewManager.AddDecorator(&m_accuSnap);
    m_viewManager.AddDecorator(&m_toolAdmin);

    // Inject the application-layer tile fetcher (Qt network backend wrapped by
    // the local-file router for offline tilesets). dqRender's TileAdmin defaults
    // to a Qt-free NullTileFetcher (§8.2); dqApp supplies the real fetcher via
    // DI (setFetcher). FileTileFetcher forwards http(s) to the Qt fetcher and
    // reads local filesystem paths directly (offline tile-sample assets).
    dqRender::TileAdmin::instance().setFetcher(
        std::make_unique<FileTileFetcher>(std::make_unique<QtTileRequestFetcher>()));

    m_initialized = true;
    OnAfterStartup.Raise();
    return true;
}

void Application::shutdown()
{
    if (!m_initialized) return;

    OnBeforeShutdown.Raise();
    m_initialized = false;
}

void Application::RegisterEntityState(const char* classFullName, EntityFactory factory)
{
    m_entityClasses[classFullName] = std::move(factory);
}

std::unique_ptr<EntityState> Application::LookupEntityClass(const char* classFullName,
                                                             const dqBase::DqId& id) const
{
    auto it = m_entityClasses.find(classFullName);
    if (it != m_entityClasses.end())
        return it->second(id);
    return nullptr;
}

void Application::RegisterCoreEntityStates()
{
    // Ported from: itwinjs-core IModelApp.startup() entity registration
    //              (IModelApp.ts:409-421)
    //
    // this.registerEntityState(EntityState.classFullName, EntityState);
    // this.registerEntityState(ElementState.classFullName, ElementState);
    // [modelState, sheetState, viewState, drawingViewState, spatialViewState,
    //  displayStyleState, modelselector, categorySelectorState, auxCoordState]
    // .forEach((module) => this.registerModuleEntities(module));

    // Note: EntityState base class has protected constructor, not directly registrable.
    // Ported from: itwinjs-core IModelApp.ts:409 (EntityState registered via module scan)
    RegisterEntityState("BisCore:Element", [](const dqBase::DqId& id) {
        return std::make_unique<ElementState>(id, dqBase::DqId());
    });
    RegisterEntityState("BisCore:Model", [](const dqBase::DqId& id) {
        return std::make_unique<ModelState>(id, "");
    });
    RegisterEntityState("BisCore:GeometricModel", [](const dqBase::DqId& id) {
        return std::make_unique<GeometricModelState>(id, "");
    });
    RegisterEntityState("BisCore:GeometricModel3d", [](const dqBase::DqId& id) {
        return std::make_unique<GeometricModel3dState>(id, "");
    });
    RegisterEntityState("BisCore:GeometricModel2d", [](const dqBase::DqId& id) {
        return std::make_unique<GeometricModel2dState>(id, "");
    });
    RegisterEntityState("BisCore:SpatialModel", [](const dqBase::DqId& id) {
        return std::make_unique<SpatialModelState>(id, "");
    });
    RegisterEntityState("BisCore:CategorySelector", [](const dqBase::DqId& id) {
        return std::make_unique<CategorySelectorState>(id);
    });
    RegisterEntityState("BisCore:ModelSelector", [](const dqBase::DqId& id) {
        return std::make_unique<ModelSelectorState>(id);
    });
}

void Application::StartEventLoop()
{
    if (m_wantEventLoop) return;
    m_wantEventLoop = true;

    // itwinjs-core relies on the browser's requestAnimationFrame to invoke
    // IModelApp.eventLoop(); Qt's equivalent is a QTimer whose timeout calls
    // Application::EventLoop(). The timer is (re)armed by RequestNextAnimation(),
    // mirroring requestAnimationFrame semantics.
    // Ported from: itwinjs-core IModelApp.startEventLoop + requestAnimationFrame.
    m_animTimer = new QTimer();
    m_animTimer->setTimerType(Qt::PreciseTimer);
    QObject::connect(m_animTimer, &QTimer::timeout, [] { Application::Get().EventLoop(); });

    // Quit safety: when the Qt event loop is exiting (last window closed /
    // Cmd+Q), stop the render timer and tear down every viewport's GL pipeline
    // BEFORE QApplication destroys the native windows. Without this, a viewport
    // whose native NSView Qt has already freed could still be rendered
    // (RenderFrame -> Swapchain::acquire -> [ctx setView:<freed NSView>]) — the
    // crash-on-close use-after-free. (Per-viewport closeEvent handles the
    // single-window-close path; this handles quit-all.)
    if (auto* core = QCoreApplication::instance()) {
        QObject::connect(core, &QCoreApplication::aboutToQuit, [] {
            Application& app = Application::Get();
            app.m_wantEventLoop = false;
            if (app.m_animTimer) app.m_animTimer->stop();
            app.m_viewManager.ShutdownAll();
        });
    }

    RequestNextAnimation();
}

void Application::RequestNextAnimation()
{
    // Qt equivalent of requestAnimationFrame: set the flag and (re)arm the timer
    // so the next EventLoop() tick runs. Application::EventLoop →
    // ViewManager::RenderLoop → Viewport::RenderFrame.
    m_animationRequested = true;
    if (m_animTimer && !m_animTimer->isActive())
        m_animTimer->start(16);  // ~60 fps
}

void Application::EventLoop()
{
    // ← IModelApp.eventLoop()
    m_animationRequested = false;
    if (!m_wantEventLoop) return;

    // Ported from: itwinjs-core IModelApp.eventLoop() (IModelApp.ts:614-622) — the
    // reference order is:
    //   IModelApp.toolAdmin.processEvent();   // :620
    //   IModelApp.viewManager.renderLoop();   // :621  (render BEFORE tile processing)
    //   IModelApp.tileAdmin.process();        // :622

    // Process tool events (drains the static FIFO queue by one event).
    // Ported from: itwinjs-core ToolAdmin.processEvent (ToolAdmin.ts:831-843).
    // 滚轮合并（Chromium 等价层，见 ToolAdmin::coalesceWheelEvents 实现注）：
    // 参考的浏览器在派发前已按帧合并滚轮，DanQing 在帧首补齐同一契约。
    GetToolAdmin().coalesceWheelEvents();
    GetToolAdmin().processEvent();

    // Render all viewports (:621).
    m_viewManager.RenderLoop();

    // Process tile admin (tile request scheduling, :622).
    dqRender::TileAdmin::instance().process();
}

}  // namespace dqApp
