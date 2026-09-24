// View3DInventor — FreeCAD MDI 3D view, modified for DanQing
//
// Original: FreeCAD src/Gui/View3DInventor.cpp
// Modification: Replaces View3DInventorViewer (Coin3D) with dqApp::Viewport (dqRender)
//
// This file implements the MDI window that contains the 3D viewport.
// The FreeCAD UI framework is preserved; only the rendering backend is replaced.

#include "View3DInventor.h"
#include "DecorationGeometryExample.h"   // m_geoDecorator 完整类型（unique_ptr 析构）
#include "MainWindow.h"
#include "MenuManager.h"  // MenuManager::setupContextMenu
#include "Command.h"      // CommandManager::setupContextMenu

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/GltfDecoration.h>
#include <dqApp/GltfImport.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewState.h>
#include <dqApp/ViewPicker.h>

#include <dqRender/GltfReader.h>

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/StandardRotations.ts
// Standard view rotation support
#include <dqApp/StandardView.h>
#include <dqApp/ViewTool.h>  // StandardViewTool（标准视图切换的参考机制）

#include <QAction>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMdiSubWindow>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWindow>

namespace Gui {

// ---------------------------------------------------------------------------
// Construction
// Ported from: FreeCAD View3DInventor.cpp View3DInventor constructor
// Modified: replaced View3DInventorViewer with dqApp::Viewport
// ---------------------------------------------------------------------------
View3DInventor::View3DInventor(Gui::Document* pcDocument, QWidget* parent,
                               dqApp::IModelConnection* connection,
                               Qt::WindowFlags wflags)
    : MDIView(pcDocument, parent, wflags)
    , m_viewport(nullptr)
    , m_connection(connection)
    , m_stack(nullptr)
{
    // Create the QStackedWidget (same pattern as FreeCAD View3DInventor).
    // Ported from: FreeCAD View3DInventor.cpp line 136
    m_stack = new QStackedWidget(this);

    // Create dqApp::Viewport — this replaces View3DInventorViewer (Coin3D).
    // The Viewport is a plain QWidget that creates its own GL context via winId().
    // It can be embedded in any QWidget hierarchy, just like ScreenViewport in
    // display-test-app attaches to a DOM element.
    //
    // Ported from: itwinjs-core Viewer.ts line 231
    //              this.viewport = ScreenViewport.create(this.contentDiv, view)
    //
    // If no connection is provided, create a BlankConnection.
    if (!m_connection) {
        // ← itwinjs-core Surface.ts:185-188 openBlankConnection defaults:
        //   location: Cartographic.fromDegrees({longitude:-75.686694, latitude:40.065757, height:0})  // near Exton, PA
        //   extents:  Range3d(-1000,-1000,-100, 1000,1000,100)
        //   name:     "blank connection test"
        auto props = dqApp::BlankConnectionProps{};
        props.name = "blank connection test";
        props.extents = dqGeom::Range3d(-1000, -1000, -100, 1000, 1000, 100);
        props.locationIsCartographic = true;
        props.cartographicLocation =
            dqCommon::Cartographic::fromDegrees(-75.686694, 40.065757, 0.0);  // near Exton, PA
        auto blank = dqApp::BlankConnection::create(props);
        // Retain via the RefPtr member — `blank` is the only holder; assigning
        // it to m_connection keeps the connection alive for this view's lifetime
        // (it would otherwise be freed when `blank` drops at end-of-scope).
        m_connection = blank;
    }

    // Create ViewState for the viewport.
    // Ported from: itwinjs-core display-test-app Viewer.create (Viewer.ts:183-188):
    //   const views = await ViewList.create(props.iModel, props.defaultViewName);
    //   const view = await views.getDefaultView(props.iModel);
    // ViewList.create → populate (ViewPicker.ts:69-144): a blank connection's
    // getViewList short-circuits to [] (IModelConnection.ts:1490-1491, isClosed always
    // true for blank), so the synthetic "Spatial View" entry is inserted
    // (ViewPicker.ts:139-140) and getView(invalid) → load fails → catch falls back to
    // manufactureSpatialView (ViewPicker.ts:39-44): a blank spatial view framing the
    // project extents (origin=ext.low, extents=diagonal, Top view) with the display
    // overrides (SmoothShade/lighting/backgroundMap/white/sky). getDefaultView returns
    // a CLONE (:49-50) so the cached view keeps its initial persistent state.
    auto views = dqApp::ViewList::create(m_connection.Get());
    auto viewState = views.getDefaultView(m_connection.Get());

    // Create the Viewport widget.
    // Ported from: itwinjs-core ScreenViewport.create(parentDiv, view)
    m_viewport = dqApp::Viewport::Create(this, viewState);

    // Add viewport to the stacked widget.
    // Ported from: FreeCAD View3DInventor.cpp line 137
    //              stack->addWidget(_viewer->getWidget())
    m_stack->addWidget(m_viewport);
    setCentralWidget(m_stack);

    // Register with ViewManager — this makes the viewport "live".
    // Ported from: itwinjs-core Surface.ts line 308
    //              IModelApp.viewManager.addViewport(viewer.viewport)
    dqApp::Application::Get().GetViewManager().AddViewport(m_viewport);

    // Setup viewer-specific toolbar.
    setupToolBar();

    // Notify that viewport is ready.
    Q_EMIT viewportCreated();
}

// ---------------------------------------------------------------------------
// Destruction
// ---------------------------------------------------------------------------
View3DInventor::~View3DInventor()
{
    // ← itwinjs-core GltfDecoration.ts:218-221 onClose -> dropDecorator + disposeGraphic.
    //   Drop before the unique_ptr destructs (ViewManager pointer is non-owning) and
    //   while m_viewport/Application are still valid.
    if (m_gltfDecoration) {
        dqApp::Application::Get().GetViewManager().DropDecorator(m_gltfDecoration.get());
        m_gltfDecoration.reset();
    }
    if (m_geoDecorator) {
        dqApp::Application::Get().GetViewManager().DropDecorator(m_geoDecorator.get());
        m_geoDecorator.reset();
    }

    // Unregister from ViewManager.
    if (m_viewport) {
        dqApp::Application::Get().GetViewManager().DropViewport(m_viewport);
    }
}

// ---------------------------------------------------------------------------
// closeEvent — drop the viewport + release its GL pipeline before Qt destroys
// the native NSView (crash-on-close fix).
// ---------------------------------------------------------------------------
void View3DInventor::closeEvent(QCloseEvent* e)
{
    // Dispose the glTF decoration BEFORE the GL pipeline shutdown below — its
    // RenderGraphic owns driver-bound GL objects (PolyfaceGraphic dtor issues
    // driver.destroyTexture/destroyRenderPrimitive/...). Disposing after
    // Shutdown() is a use-after-teardown (SEH 0xc0000005 in ~View3DInventor,
    // GltfTexturePixelTest teardown).
    // ← itwinjs-core GltfDecoration.ts:218-221 onClose -> dropDecorator +
    //   disposeGraphic (the decoration disposes while the render system lives).
    if (m_gltfDecoration) {
        dqApp::Application::Get().GetViewManager().DropDecorator(m_gltfDecoration.get());
        m_gltfDecoration.reset();
    }

    // Same discipline for the Decoration Geometry Example decorator (the
    // reference's iModel.onClose → dispose, DecorationGeometryExample.ts:49) —
    // its PolyfaceGraphic is driver-bound and must be disposed pre-Shutdown.
    if (m_geoDecorator) {
        dqApp::Application::Get().GetViewManager().DropDecorator(m_geoDecorator.get());
        m_geoDecorator.reset();
    }

    // Synchronous teardown on close: remove the viewport from the render list
    // and release its GL pipeline NOW, while the native NSView is still valid.
    // Without this, the render timer keeps rendering into the viewport after
    // Qt has freed its NSView -> [ctx setView:<freed NSView>] use-after-free.
    // (Idempotent with ~View3DInventor's DropViewport and ~Viewport's Shutdown.)
    if (m_viewport) {
        dqApp::Application::Get().GetViewManager().DropViewport(m_viewport);
        m_viewport->Shutdown();
    }
    MDIView::closeEvent(e);
}

void View3DInventor::setGeoDecorator(std::unique_ptr<Gui::GeometryDecorator> deco)
{
    // 重复打开示例时先摘旧注册再释放（ViewManager 持有非拥有裸指针——
    // 直接 reset 会悬挂；与析构/closeEvent 同一纪律）。
    if (m_geoDecorator)
        dqApp::Application::Get().GetViewManager().DropDecorator(m_geoDecorator.get());
    m_geoDecorator = std::move(deco);
}

// Ported from: itwinjs-core Surface.ts openBlankConnection(:183-192) + ctor 默认
// 参数（Surface.ts:185-188，Exton 位置）——示例自带 extents 的新连接语义。
void View3DInventor::resetBlankConnection(dqGeom::Range3d const& extents, std::string const& name)
{
    if (!m_viewport)
        return;
    auto props = dqApp::BlankConnectionProps{};
    props.name = name;
    props.extents = extents;
    props.locationIsCartographic = true;
    props.cartographicLocation =
        dqCommon::Cartographic::fromDegrees(-75.686694, 40.065757, 0.0);  // near Exton, PA
    m_connection = dqApp::BlankConnection::create(props);

    // ctor 同款默认视图链（ViewList.create → getDefaultView，ViewPicker.ts）。
    auto views = dqApp::ViewList::create(m_connection.Get());
    auto viewState = views.getDefaultView(m_connection.Get());
    dqApp::ViewChangeOptions opts;
    opts.animateFrustumChange = false;   // 参考是新 viewer——无从旧视图的动画过渡
    m_viewport->ChangeView(viewState, &opts);
}

// ---------------------------------------------------------------------------
// clone — create a copy of this view.
// Ported from: FreeCAD View3DInventor::clone()
// ---------------------------------------------------------------------------
View3DInventor* View3DInventor::clone()
{
    auto* view = new View3DInventor(
        _pcDocument, parentWidget(), m_connection.Get());
    return view;
}

// ---------------------------------------------------------------------------
// Message handler
// Ported from: FreeCAD View3DInventor::onMsg()
// Modified: delegates to dqApp::Viewport instead of View3DInventorViewer
// ---------------------------------------------------------------------------
bool View3DInventor::onMsg(const char* pMsg)
{
    if (!m_viewport) return false;

    // 标准视图切换统一走参考机制（StandardViewTool，ViewTool.ts:3503-3525——含
    // animateFrustumChange 相机动画）；tool->run() 失败（未安装）则释放。
    auto runStandardTool = [this](dqApp::StandardViewId id) {
        auto* tool = new dqApp::StandardViewTool(m_viewport, id);
        if (!tool->run())
            delete tool;
    };

    // ViewFit — fit all geometry in view.
    if (strcmp(pMsg, "ViewFit") == 0) {
        viewAll();
        return true;
    }
    // ViewFront — set front view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 4)
    if (strcmp(pMsg, "ViewFront") == 0) {
        runStandardTool(dqApp::StandardViewId::Front);
        return true;
    }
    // ViewTop — set top view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 0)
    if (strcmp(pMsg, "ViewTop") == 0) {
        runStandardTool(dqApp::StandardViewId::Top);
        return true;
    }
    // ViewRight — set right view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 3)
    if (strcmp(pMsg, "ViewRight") == 0) {
        runStandardTool(dqApp::StandardViewId::Right);
        return true;
    }
    // ViewIsometric — set isometric view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 6)
    if (strcmp(pMsg, "ViewIsometric") == 0) {
        runStandardTool(dqApp::StandardViewId::Iso);
        return true;
    }
    // ViewRear — set rear/back view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 5)
    // (FreeCAD "Rear" maps to itwinjs StandardViewId::Back.)
    if (strcmp(pMsg, "ViewRear") == 0) {
        runStandardTool(dqApp::StandardViewId::Back);
        return true;
    }
    // ViewBottom — set bottom view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 1)
    if (strcmp(pMsg, "ViewBottom") == 0) {
        runStandardTool(dqApp::StandardViewId::Bottom);
        return true;
    }
    // ViewLeft — set left view.
    // Ported from: itwinjs-core StandardRotations.ts — IModelApp.tools.run("View.Standard", vp, 2)
    if (strcmp(pMsg, "ViewLeft") == 0) {
        runStandardTool(dqApp::StandardViewId::Left);
        return true;
    }
    // OrthographicCamera — turn perspective camera OFF (orthographic projection).
    // Ported from: itwinjs-core Viewport — camera.on = false ⇒ orthographic projection.
    if (strcmp(pMsg, "OrthographicCamera") == 0) {
        auto* view = m_viewport->GetView();
        if (auto* view3d = view ? view->AsViewState3d() : nullptr) {
            view3d->TurnCameraOff();
            m_viewport->synchWithView();
        }
        return true;
    }
    // PerspectiveCamera — turn perspective camera ON.
    // Ported from: itwinjs-core Viewport — camera.on = true ⇒ perspective projection.
    if (strcmp(pMsg, "PerspectiveCamera") == 0) {
        auto* view = m_viewport->GetView();
        if (auto* view3d = view ? view->AsViewState3d() : nullptr) {
            view3d->EnableCamera();
            m_viewport->synchWithView();
        }
        return true;
    }
    // ViewCreate — clone the current view into a new MDI window.
    // Ported from: FreeCAD StdCmdViewCreate::activated → new view window.
    if (strcmp(pMsg, "ViewCreate") == 0) {
        if (auto* mw = Gui::getMainWindow()) {
            mw->addWindow(clone());
        }
        return true;
    }

    return false;
}

bool View3DInventor::onHasMsg(const char* pMsg) const
{
    if (strcmp(pMsg, "ViewFit") == 0) return true;
    if (strcmp(pMsg, "ViewFront") == 0) return true;
    if (strcmp(pMsg, "ViewTop") == 0) return true;
    if (strcmp(pMsg, "ViewRight") == 0) return true;
    if (strcmp(pMsg, "ViewIsometric") == 0) return true;
    if (strcmp(pMsg, "ViewRear") == 0) return true;
    if (strcmp(pMsg, "ViewBottom") == 0) return true;
    if (strcmp(pMsg, "ViewLeft") == 0) return true;
    if (strcmp(pMsg, "OrthographicCamera") == 0) return true;
    if (strcmp(pMsg, "PerspectiveCamera") == 0) return true;
    if (strcmp(pMsg, "ViewCreate") == 0) return true;
    return false;
}

// ---------------------------------------------------------------------------
// onUpdate — trigger a repaint.
// Ported from: FreeCAD View3DInventor::onUpdate()
// ---------------------------------------------------------------------------
void View3DInventor::onUpdate()
{
    if (m_viewport) {
        m_viewport->update();
    }
}

// ---------------------------------------------------------------------------
// viewAll — fit all geometry in view.
// Ported from: FreeCAD View3DInventor::viewAll()
// Ported from: itwinjs-core Viewer.ts — IModelApp.tools.run("View.Fit", vp, true)
// ---------------------------------------------------------------------------
void View3DInventor::viewAll()
{
    if (!m_viewport) return;

    auto* view = m_viewport->GetView();
    if (!view) return;

    // Ported from: itwinjs-core ViewManip.fitView (ViewTool.ts:823-829)：
    //   const range = this.computeFitRange(viewport);   ← blank connection 空几何
    //     时 computeViewRange 即 projectExtents（clipVolume 关，无相交差异）
    //   const aspect = viewport.viewRect.aspect;
    //   viewport.view.lookAtVolume(range, aspect, options);
    //   viewport.synchWithView({ animateFrustumChange });
    //   viewport.viewCmdTargetCenter = undefined;   ← TODO 已登记（ViewTool.cpp:719）
    double const aspect = m_viewport->viewRect().aspect();
    auto* iModel = m_viewport->GetIModel();
    if (iModel) {
        auto const& extents = iModel->GetProjectExtents();
        if (auto* view3d = view->AsViewState3d()) {
            view3d->LookAtVolume(extents, &aspect, nullptr);
        }
    } else {
        // BlankConnection: use the view's own extents as fallback
        auto const& origin = view->GetOrigin();
        auto const& ext = view->GetExtents();
        dqGeom::Range3d volume;
        volume.low = dqGeom::Point3d::From(
            origin.x - ext.x * 0.5, origin.y - ext.y * 0.5, origin.z - ext.z * 0.5);
        volume.high = dqGeom::Point3d::From(
            origin.x + ext.x * 0.5, origin.y + ext.y * 0.5, origin.z + ext.z * 0.5);
        if (auto* view3d = view->AsViewState3d()) {
            view3d->LookAtVolume(volume, &aspect, nullptr);
        }
    }

    // Ported from: itwinjs-core FitViewTool.doFit (ViewTool.ts:3254-3260)：
    //   synchWithView({ animateFrustumChange: doAnimate })，doAnimate 默认 true
    //   → FrustumAnimator 默认 1000ms/Cubic.Out/cancelOnAbort=false
    //   （ViewAnimation.ts:36-45 + FrustumAnimator.ts:62；DanQing 的
    //   ViewChangeOptions 同默认：animationTime nullopt→time.normal=1000ms、
    //   easingFunction=CubicOut、cancelOnAbort=false）。
    dqApp::ViewChangeOptions opts;
    opts.animateFrustumChange = true;
    m_viewport->synchWithView(opts);
    m_viewport->update();
}

// ---------------------------------------------------------------------------
// loadGltf — load a glTF file and install it as a pickable fitted decoration.
// Ported from: itwinjs-core GltfDecoration.ts:157-208 (queryAsset + readGltfTemplate + addDecorator)
//              + :218-221 (onClose -> disposeGraphic + dropDecorator, here done by ownership)
// ---------------------------------------------------------------------------
bool View3DInventor::loadGltf(QString const& path)
{
    if (!m_viewport)
        return false;

    // ← itwinjs-core GltfDecoration.ts:157 queryAsset (file bytes)
    // toUtf8（非 toStdString——后者 Windows 按 ANSI 代码页，❤♻ 等 Unicode 路径
    // 无法编码 → 字节错乱 → 打不开；LoadFromFile 的宽路径修复消费 UTF-8 字节）。
    auto scene = dqRender::GltfReader::LoadFromFile(path.toUtf8().toStdString());
    if (!scene || scene->meshes.empty()) {
        if (auto* mw = getMainWindow())
            mw->showStatus(0, tr("Import failed: %1").arg(path));
        return false;
    }

    // Drop any previous glTF decoration before installing a new one
    // (ViewManager holds a non-owning pointer — must Drop BEFORE the unique_ptr resets).
    if (m_gltfDecoration) {
        dqApp::Application::Get().GetViewManager().DropDecorator(m_gltfDecoration.get());
        m_gltfDecoration.reset();
    }

    auto name = QFileInfo(path).fileName().toStdString();
    m_gltfDecoration = dqApp::InstallGltfDecoration(*m_viewport, std::move(scene), name);
    if (!m_gltfDecoration)
        return false;

    // Fit the view to the decoration's range so the imported model is visible.
    // ← itwinjs-core GltfDecoration.ts:210-213:
    //   const range = new Range3d(); graphic.unionRange(range);
    //   vp.view.lookAtVolume(range, vp.viewRect.aspect)
    // The blank connection's view is framed on a 1000-unit volume (CreateBlank
    // extents), so a unit-scale glTF (e.g. the 2-unit test cube) renders at ~0.2%
    // of the view — a sub-pixel speck that looks "invisible" even though it draws
    // correctly into the render-target FBO. Fitting the view to the glTF range
    // frames the camera on the model so it fills the viewport.
    // aspect = viewRect.aspect（ViewRect.ts:53 width/height，null→1.0）——参考
    // GltfDecoration.ts:213 传入 lookAtVolume，使 adjustViewDelta 的 aspect 分支
    // 把较短边撑到窗口纵横比（只增不缩）。漏传时 fit 出正方形 delta，随后
    // viewport 的 fixAspectRatio 反向把 y 缩到 x/aspect——立方体满幅贴边且纵深
    // 几何与参考发散（2026-09-15 Front/Back 甩位 saga 的取景层缺口）。
    dqGeom::Range3d range;
    if (m_gltfDecoration->GetGraphicRange(range) && !range.isNull()) {
        auto* view = m_viewport->GetView();
        if (auto* view3d = view ? view->AsViewState3d() : nullptr) {
            float const w = m_viewport->width();
            float const h = m_viewport->height();
            double const aspect = (w > 0.0f && h > 0.0f) ? static_cast<double>(w / h) : 1.0;
            view3d->LookAtVolume(range, &aspect);
        }
        m_viewport->InvalidateController();
        // ← itwinjs-core GltfDecoration.ts:214:
        //   vp.synchWithView({ animateFrustumChange: true });
        // 参考导入后从空白连接的大范围取景渐进缩放到模型 fit（用户在 DTA 观察
        // 到的"超大立方体渐进缩放"动画）；DanQing 此前直接 update() 瞬时跳变。
        dqApp::ViewChangeOptions syncOptions;
        syncOptions.animateFrustumChange = true;
        m_viewport->synchWithView(syncOptions);
    }

    m_viewport->update();  // trigger a repaint
    if (auto* mw = getMainWindow())
        mw->showStatus(0, tr("Imported: %1").arg(path));
    return true;
}

// ---------------------------------------------------------------------------
// getName — return the view name.
// Ported from: FreeCAD View3DInventor::getName()
// ---------------------------------------------------------------------------
const char* View3DInventor::getName() const
{
    return "View3DInventor";
}

// ---------------------------------------------------------------------------
// setCurrentViewMode — handle GL surface reparenting across mode transitions.
// Ported from: FreeCAD src/Gui/View3DInventor.cpp:723-768
// Modified: View3DInventorViewer (Coin3D) → dqApp::Viewport (dqRender).
// FreeCAD operates on Coin3D's getGLWidget(); DanQing operates on m_viewport.
// On return-to-Child the MDIView's own QWindow (acquired when it became a
// top-level widget) is destroyed so it doesn't interfere with QMdiSubWindow
// resize/layout — same call as FreeCAD View3DInventor.cpp:737-740.
// ---------------------------------------------------------------------------
void View3DInventor::setCurrentViewMode(ViewMode mode)
{
    ViewMode oldmode = currentViewMode();
    if (mode == oldmode) {
        return;
    }

    if (mode == Child) {
        // The mdi view got a QWindow when it became a top-level widget and when resetting it to a
        // child widget the QWindow must be deleted because it has an impact on resize events and
        // may break the layout of mdi view inside the QMdiSubWindow. In the second step below the
        // layout must be invalidated after it's again a child widget to make sure the mdi view fits
        // into the QMdiSubWindow.
        // Modified: FreeCAD calls this->windowHandle()->destroy(); the same call applies unchanged
        // in DanQing since `this` (View3DInventor, an MDIView subclass) is the widget that receives
        // the top-level QWindow. m_viewport is a child widget and never holds the top-level QWindow.
        QWindow* winHandle = this->windowHandle();
        if (winHandle) {
            winHandle->destroy();
        }
    }

    MDIView::setCurrentViewMode(mode);

    // This widget becomes the focus proxy of the embedded GL widget if we leave
    // the 'Child' mode. If we reenter 'Child' mode the focus proxy is reset to 0.
    // If we change from 'TopLevel' mode to 'Fullscreen' mode or vice versa nothing
    // happens.
    // Grabbing keyboard when leaving 'Child' mode (as done in a recent version) should
    // be avoided because when two or more windows are either in 'TopLevel' or 'Fullscreen'
    // mode only the last window gets all key event even if it is not the active one.
    //
    // It is important to set the focus proxy to get all key events otherwise we would lose
    // control after redirecting the first key event to the GL widget.
    // Modified: FreeCAD operates on _viewer->getGLWidget(); DanQing operates on m_viewport.
    if (oldmode == Child) {
        if (m_viewport) {
            m_viewport->setFocusProxy(this);
        }
    }
    else if (mode == Child) {
        if (m_viewport) {
            m_viewport->setFocusProxy(nullptr);
        }

        // Step two
        auto mdi = qobject_cast<QMdiSubWindow*>(parentWidget());
        if (mdi && mdi->layout()) {
            mdi->layout()->invalidate();
        }
    }
}

// ---------------------------------------------------------------------------
// setupToolBar — create viewer-specific toolbar.
// Ported from: itwinjs-core Viewer.ts constructor — toolbar setup
// ---------------------------------------------------------------------------
void View3DInventor::setupToolBar()
{
    // TODO: create toolbar with view controls (fit, rotate, etc.)
    // This will be implemented when tools are connected.
}

// ---------------------------------------------------------------------------
// contextMenuEvent — pop up the "View" context menu on right-click.
// Ported from: FreeCAD src/Gui/Navigation/NavigationStyle.cpp:2422-2487
//              (NavigationStyle::openPopupMenu)
// DTA adaptation:
//   * Replaces FreeCAD's NavigationStyle::openPopupMenu (Coin3D viewer entry)
//     with QWidget's contextMenuEvent, since View3DInventor is the QWidget that
//     receives the right-click (no Coin3D NavigationStyle in DTA).
//   * Drops Coin3D SoRayPickAction ray-pick for Clarify Selection
//     (NavigationStyle.cpp:2440-2470) — GL-side pick not yet wired in DTA.
//     TODO: deferred — implement via itwinjs-core; see design 2026-07-16 §5.
//   * Drops the nav-style submenu (spec §4.4 OUT).
//   * Dispatch entry point is CommandManager::setupContextMenu (DTA adaptation
//     of Gui::Application::setupContextMenu); MenuManager::setupContextMenu
//     builds the QMenu (DTA adaptation of MenuManager::getInstance()-
//     >setupContextMenu). Both are reached via MainWindow::getInstance().
// ---------------------------------------------------------------------------
void View3DInventor::contextMenuEvent(QContextMenuEvent* e)
{
    // Build the "View" context-group MenuItem tree via the active workbench.
    // Ported from: FreeCAD NavigationStyle.cpp:2428-2429
    //              MenuItem view; Gui::Application::Instance->setupContextMenu("View", &view);
    MenuItem view;
    MainWindow* mw = getMainWindow();
    if (!mw) {
        return;
    }
    mw->commandManager().setupContextMenu("View", &view);

    // Build the QMenu from the MenuItem tree.
    // Ported from: FreeCAD NavigationStyle.cpp:2431-2433
    //              auto contextMenu = new QMenu(viewer->getGLWidget());
    //              MenuManager::getInstance()->setupContextMenu(&view, *contextMenu);
    QMenu menu(this);
    mw->menuManager().setupContextMenu(&view, menu);

    // Clarify Selection ray-pick block (FreeCAD NavigationStyle.cpp:2435-2470):
    //   SoRayPickAction rp(...); rp.setPoint(position); ...
    //   if (pplist.getLength() > 0) { ... add Clarify Selection action ... }
    // DTA: GL ray-pick not yet wired.
    // TODO: deferred — implement via itwinjs-core; see design 2026-07-16 §5.

    // Pop up the menu at the click position.
    // Ported from: FreeCAD NavigationStyle.cpp:2484 — contextMenu->popup(QCursor::pos());
    // DTA uses exec(globalPos) (Qt's standard contextMenuEvent pattern; blocking
    // — equivalent because the Clarify Selection triggered() lambda is dropped).
    menu.exec(e->globalPos());
}

}  // namespace Gui
