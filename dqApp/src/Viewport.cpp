// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Viewport implementation
// Ported from: itwinjs-core core/frontend/src/Viewport.ts (ScreenViewport)
//              FreeCAD src/Gui/View3DInventorViewer.cpp
//
// Bridges Qt's QWidget with dqRender's Swapchain + Driver.
// The native window handle is obtained from QWidget::winId(), and
// the Swapchain manages the GL context binding. Rendering goes through
// the RHI pipeline (no raw GL calls).
//
// The renderFrame() method implements itwinjs-core's 17-step pipeline:
//   1. Capture changeFlags  2. Animate  3. Redraw check  4. Resize
//   5. Controller sync  6. Selection set  7. Analysis fraction
//   8. Time point  9. Feature overrides  10. Scene creation
//   11. Render plan  12. Decorations  13. Flash  14. Pre-render
//   15. Draw  16. Events  17. Continuous rendering
#include "dqApp/Viewport.h"
#include "dqApp/Application.h"
#include "dqApp/DecorateContext.h"
#include "dqApp/IModelConnection.h"
#include "dqApp/ViewManager.h"
#include "dqApp/ViewTool.h"
#include "dqApp/Decorator.h"
#include "BackgroundMapGeometry.h"   // Viewport::backgroundMapGeometry + pickDepthPoint 求交分支

#include <dqRender/GraphicBuilder.h>
#include <dqRender/PlanarGridProps.h>
#include <cstdio>
#include <cstdlib>
#include <dqRender/RenderPipeline.h>
#include <dqRender/rhi/Driver.h>   // collectTextureStatistics virtual
#include <dqRender/RenderPlan.h>
#include <dqRender/RenderTarget.h>
#include <dqRender/RenderSystem.h>
#include <dqRender/RenderSkyBoxParams.h>  // RenderSkyGradientParams (gradient sky)
#include <dqRender/Swapchain.h>
#include <dqRender/GltfReader.h>
#include <dqRender/tile/TileAdmin.h>
#include "dqApp/tile/SceneContext.h"
#include "dqApp/tile/SimpleTileTreeReference.h"
#include "dqApp/tile/TiledGraphicsProvider.h"
#include <dqRender/tile/TileTree.h>
#include <dqRender/tile/TileDrawArgs.h>
#include <dqCommon/FlashSettings.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/PolyfaceData.h>
#include <QCursor>
#include <QPixmap>

#include <map>

#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Ray3d.h>

#include <cmath>

#ifndef DANQING_CURSOR_ASSETS_DIR
#define DANQING_CURSOR_ASSETS_DIR "."
#endif
#include <dqGeom/Transform.h>

#include <QShowEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QDateTime>
#include <cmath>
#include <chrono>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace dqApp {

// ---------------------------------------------------------------------------
// Task 14: Qt → ToolAdmin.addEvent bridge — file-local field-extraction helpers.
//
// Ported from: itwinjs-core EventController.ts (core/frontend/src/tools/EventController.ts)
//              DOM listener → ToolAdmin.addEvent (ToolAdmin.ts:792). The DOM
//              listeners extract button/modifiers from the DOM Event; DanQing's
//              Qt port does the same from a QMouseEvent/QWheelEvent/QKeyEvent
//              (§3.4 adaptation — DOM/Qt types collapse to the ToolEvent POD).
//
// Authored: no single itwinjs source for these helpers — EventController reads
//           DOM `event.button` / `event.shiftKey|ctrlKey|altKey`; the Qt→BeButton
//           / Qt→BeModifierKeys mapping is the faithful C++ equivalent.
// ---------------------------------------------------------------------------

// Map a Qt::MouseButton to its faithful BeButton equivalent.
// Qt::LeftButton → BeButton::Data  (primary data button)
// Qt::RightButton → BeButton::Reset (contextual/reset)
// Qt::MiddleButton → BeButton::Middle
// (Matches itwinjs Tool.ts:36 BeButton order: Data = 0, Reset = 1, Middle = 2.)
static BeButton qtToBeButton(Qt::MouseButton b) noexcept
{
    switch (b) {
        case Qt::LeftButton:   return BeButton::Data;
        case Qt::RightButton:  return BeButton::Reset;
        case Qt::MiddleButton: return BeButton::Middle;
        default:               return BeButton::Data;  // unknown → Data (safe default)
    }
}

// Map Qt::KeyboardModifiers to the faithful BeModifierKeys bitmask.
// (Control/Shift/Alt are the three modifiers Tool.ts:81 enumerates.)
static BeModifierKeys qtToModifiers(Qt::KeyboardModifiers m) noexcept
{
    BeModifierKeys k = BeModifierKeys::None;
    if (m & Qt::ControlModifier) k = k | BeModifierKeys::Control;
    if (m & Qt::ShiftModifier)   k = k | BeModifierKeys::Shift;
    if (m & Qt::AltModifier)     k = k | BeModifierKeys::Alt;
    return k;
}

// ---------------------------------------------------------------------------
// WindowArea/Look W4 — 2D canvas decorations（CanvasDecoration）后端说明。
//
// 第一版后端（QPainterCanvasContext + CanvasOverlayWidget——QPainter alien 子控件
// 覆盖在原生 WGL 视口上）在视口显示时导致 GPU 驱动挂死（2x LiveKernelEvent 141
// TDR：GDI 在 GL swap HWND 上合成与 WGL 交换链争用）。按预批准兜底（原 spec
// 2026-09-11-windowarea-look-design §2.4，已随 2026-09-24 历史文档清理删除）移除，
// 2D 装饰改为在 GL 帧内栅格化——同一 CanvasContext API 与 drawDecoration 体不变，
// 仅换栅格化后端（见 dqRender GLCanvasContext + RenderTarget::drawCanvasDecorations，
// RenderFrame Step 15 调用点）。此举亦更贴近参考：itwinjs 在每个绘制帧内重栅格化
// overlay 装饰（Target.drawFrame → drawOverlayDecorations，Target.ts:546-554）。
// ---------------------------------------------------------------------------

int Viewport::sNextViewportId = 1;

dqBase::DqDuration Viewport::s_undoDelay = dqBase::DqDuration::FromSeconds(0.5);  // Viewport.ts:521（公有可变）

Viewport::Viewport(QWidget* parent, dqBase::RefPtr<ViewState> view)
    : QWidget(parent)
    , m_view(std::move(view))
    , m_viewportId(sNextViewportId++)
{
    // Set minimum size
    setMinimumSize(100, 100);

    // Enable mouse tracking for smooth interaction
    setMouseTracking(true);

    // Ensure the widget creates a native window (NSView on macOS, HWND on Windows).
    // This is required for the Swapchain to bind the GL context to the window surface.
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    // WA_OpaquePaintEvent：声明本控件完全自绘（GL 帧），Qt 的 backing store 不
    // 对本区域做 raster flush——最大化后父窗口（View3DInventor/QMainWindow）的
    // QWindowsBackingStore::flush 会尝试 GetDC(GL 子窗口 HWND) 失败并循环重试
    // （QWindowsBackingStore::flush: GetDC failed 刷屏，主线程 91% CPU 卡死）。
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Register as tile user (← itwinjs-core: IModelApp.tileAdmin.registerUser(this))
    dqRender::TileAdmin::instance().registerUser(*this);

    // Tile content landing → scene invalidation.
    // ← itwinjs-core: Viewport.onRequestStateChanged = invalidateScene
    //   (Viewport.ts:3082-3084); TileAdmin raises onTileLoad on setContent
    //   (TileAdmin.ts:759-765 → 581-585 invalidateAllScenes).
    m_onTileLoadDisconnect = dqRender::TileAdmin::instance().onTileLoad.AddListener(
        [this](dqRender::Tile&) { InvalidateScene(); });

    // Attach to view events (DisplayStyle, ViewState changes)
    AttachToView();
}

Viewport::~Viewport()
{
    // Detach the tile-load subscription before the widget dies
    // (the event outlives this viewport — TileAdmin is global).
    if (m_onTileLoadDisconnect)
        m_onTileLoadDisconnect();

    // Detach from view events before destruction
    DetachFromView();

    // Unregister from tile admin (← itwinjs-core: IModelApp.tileAdmin.forgetUser(this))
    dqRender::TileAdmin::instance().forgetUser(*this);

    // Release the GL pipeline (idempotent: a prior closeEvent / ShutdownAll may
    // already have done this). Done here too so a viewport destroyed without a
    // closeEvent (e.g. QApplication teardown of a child widget) still releases
    // its GL resources while the native window is still valid.
    Shutdown();
}

void Viewport::Shutdown()
{
    // Idempotent: a viewport may be shut down by closeEvent and again by
    // ViewManager::ShutdownAll / ~Viewport.
    if (m_isShuttingDown) return;
    m_isShuttingDown = true;

    // 绑定本视口的视图工具一并退出（视口先死、工具持有裸指针悬空——下一视口的
    // 手势会路由进死视口（startHandleDrag → viewport 死指针 UB；2026-09-19 中键
    // 平移测试复现）。exitViewTool 同时经 SuspendedToolState 恢复光标/光圈状态。
    // 须在渲染管线拆除前（恢复路径读 target）。
    {
        auto& toolAdmin = Application::Get().GetToolAdmin();
        if (auto* vt = toolAdmin.GetViewTool(); vt && vt->viewport == this)
            toolAdmin.exitViewTool();
    }

    // Release this viewport's tile contents BEFORE the GL pipeline dies —
    // tile graphics (PolyfaceGraphic GPU objects) must be freed while the
    // driver that created them is still alive (reference: viewport dispose
    // drops its tile contents via TileAdmin per-user cleanup). Without this,
    // a tree outliving the viewport crashes in ~PolyfaceGraphic (SEH
    // 0xc0000005, 2026-09-21 LOD test teardown).
    for (auto* tree : m_tileTrees)
        if (tree) tree->freeContents();
    m_tileTrees.clear();  // raw pointers to caller-owned trees — never touch again

    // Clean up render pipeline (Swapchain owns GL context binding, Driver owns
    // resources). The Swapchain is destroyed before the Driver in
    // RenderPipeline::shutdown(). Releasing here — before Qt destroys the
    // native NSView — prevents RenderFrame -> acquire ->
    // [ctx setView:<freed NSView>] (crash-on-close use-after-free).
    delete m_renderTarget;
    m_renderTarget = nullptr;
    // 装饰缓存持有网格等 GraphicOwner（其析构经 driver 释放 GL 资源）——必须在
    // m_pipeline 销毁前清空（GL 上下文/驱动尚存活，同 AcsTriadDecorator 的
    // dispose 生命周期约束）。
    m_decorationCache.clear();
    delete m_skyGraphic;
    m_skyGraphic = nullptr;
    delete m_gridGraphic;   // 同 m_skyGraphic 的 GL 生命周期约束（pipeline 前）
    m_gridGraphic = nullptr;
    m_pipeline.reset();
}

Viewport* Viewport::Create(QWidget* parent, dqBase::RefPtr<ViewState> view)
{
    // ← ScreenViewport.create(parentDiv, view)
    if (!view) return nullptr;

    auto* vp = new Viewport(parent, std::move(view));
    // ScreenViewport.create populates the viewing transform immediately (itwinjs
    // calls setupFromView), so a freshly-created Viewport has a current
    // ViewingSpace — required now that Viewport::getFrustum delegates to it
    // (the adjusted-frustum path). Without this, getFrustum would read the
    // default identity maps.
    vp->SetupFromView();

    // ← ScreenViewport.create → vp.changeView(view) (Viewport.ts:3196-3204)：参考经
    // ScreenViewport.changeView → saveViewUndo（Viewport.ts:3623）以初始视图建立撤销
    // 基线。DanQing 的构造器直接接收视图，故在 Create 里建立基线（净效果与参考一致：
    // 新视口有基线、撤销栈为空）。
    vp->saveViewUndo();

    return vp;
}

IModelConnection* Viewport::GetIModel() const
{
    // ← Viewport._iModel (accessed through ViewState)
    return m_view ? m_view->GetIModel() : nullptr;
}

// Ported from: itwinjs-core Viewport.setCursor (Viewport.ts:3580-3582 —
// `this.canvas.style.cursor = cursor`). The reference's cursor family is
// `url(<name>.cur), <cssFallback>` (ViewManager.ts:585-592): the browser shows
// the bitmap, falling back to a standard CSS shape when the URL fails.
// DanQing mirrors the two-tier resolve:
//   1. bitmap tier — <name>.png + hotspot converted from the reference's
//      public/cursors/*.cur (third_party/itwinjs-cursors/); hotspots recorded
//      at conversion time (32x32 images).
//   2. fallback tier — the reference's CSS fallback keyword resolved to the
//      equivalent Qt shape (crosshair→Cross, move→SizeAll, auto→Arrow).
// Unknown names fall back to Arrow (the CSS default keyword's shape).
void Viewport::setCursor(std::string const& cursor)
{
    static std::map<std::string, QCursor> const kBitmapCursors = [] {
        std::map<std::string, QCursor> out;
        struct Entry { char const* name; int hotX, hotY; };
        // Hotspots from the .cur directory entries (32x32 frames):
        //   crosshair(15,15) dynamics(15,15) openHand(15,15) closedHand(8,10)
        //   rotate(16,15) look(16,15) walk(16,15) zoom(16,15)
        Entry const entries[] = {
            {"crosshair", 15, 15}, {"dynamics", 15, 15}, {"grab", 15, 15},
            {"grabbing", 8, 10},   {"walk", 16, 15},     {"rotate", 16, 15},
            {"look", 16, 15},      {"zoom", 16, 15},
        };
        for (auto const& e : entries) {
            QPixmap pm(QString(DANQING_CURSOR_ASSETS_DIR "/%1.png").arg(e.name));
            if (!pm.isNull())
                out.emplace(e.name, QCursor(pm, e.hotX, e.hotY));
        }
        return out;
    }();

    auto const bmp = kBitmapCursors.find(cursor);
    if (bmp != kBitmapCursors.end()) {
        QWidget::setCursor(bmp->second);
        return;
    }

    Qt::CursorShape shape = Qt::ArrowCursor;
    if (cursor == "crosshair")        shape = Qt::CrossCursor;
    else if (cursor == "move")        shape = Qt::SizeAllCursor;
    else if (cursor == "not-allowed") shape = Qt::ForbiddenCursor;
    else if (cursor == "pointer")     shape = Qt::PointingHandCursor;
    else if (cursor == "grab")        shape = Qt::OpenHandCursor;
    else if (cursor == "grabbing")    shape = Qt::ClosedHandCursor;
    else if (cursor == "wait")        shape = Qt::WaitCursor;
    else if (cursor == "text")        shape = Qt::IBeamCursor;
    QWidget::setCursor(QCursor(shape));
}

void Viewport::collectTextureStatistics(dqRender::RenderMemory::Statistics& stats) const
{
    // MemoryTracker calcMem slice (MemoryTracker.ts:52-57): the textures owned
    // by this viewport's GL driver. Routed via the Driver virtual (§9 -fno-rtti
    // — no RTTI casts); backends without a statistics hook report zero.
    if (!m_pipeline)
        return;
    m_pipeline->getDriver().collectTextureStatistics(stats);
}

void Viewport::collectStatistics(dqRender::RenderMemory::Statistics& stats)
{
    // Ported from: Viewport.collectStatistics (Viewport.ts:2948-2954):
    //   discloseTileTrees → for each tree: tree.collectStatistics(stats)
    //   → view.collectNonTileTreeStatistics(stats)（背景图等非树源——DanQing 的
    //   ViewState 尚无该走查，见头文件 NOTE）。
    std::vector<dqRender::TileTree*> trees;
    discloseTileTrees(trees);
    for (auto* tree : trees) {
        if (tree)
            tree->collectStatistics(stats);
    }
}

dqRender::RenderGraphic* Viewport::createGraphicFromPolyface(
    dqGeom::IndexedPolyface const* polyface, uint32_t defaultColor, uint32_t featureId,
    dqRender::rhi::TextureHandle texture, dqRender::rhi::TextureHandle normalMapTexture,
    float normalMapScale, bool textureExternal, bool normalMapTextureExternal)
{
    // Delegate to this viewport's RenderPipeline (the real GL driver). The global
    // RenderSystem::get() is a no-op stub here, so driver-bound graphics must route
    // through the per-viewport pipeline.
    return m_pipeline
        ? m_pipeline->createGraphicFromPolyface(polyface, defaultColor, featureId, texture,
                                                normalMapTexture, normalMapScale,
                                                textureExternal, normalMapTextureExternal)
        : nullptr;
}

// ← itwinjs-core RenderSystem.createTexture (per-viewport driver routing).
dqRender::rhi::TextureHandle Viewport::createTexture(dqRender::CreateTextureArgs const& args)
{
    return m_pipeline ? m_pipeline->createTexture(args) : dqRender::rhi::TextureHandle{};
}

dqRender::rhi::Driver* Viewport::getDriver()
{
    return m_pipeline ? &m_pipeline->getDriver() : nullptr;
}

dqRender::RenderGraphic* Viewport::createGraphicList(std::vector<dqRender::RenderGraphic*> graphics)
{
    return m_pipeline ? m_pipeline->createGraphicList(std::move(graphics)) : nullptr;
}

dqRender::RenderGraphicOwner* Viewport::createGraphicOwner(dqRender::RenderGraphic* owned)
{
    return m_pipeline ? m_pipeline->createGraphicOwner(owned) : nullptr;
}

// Ported from: itwinjs-core viewport.target.renderSystem.createPlanarGrid
//              (ViewContext.drawStandardGrid 调用点，ViewContext.ts:348)。
// 缓存复用（m_skyGraphic 同款生命周期）：首次 createPlanarGrid 建图，后续帧
// updatePlanarGridFrustum 原地更新 buffer（GPU 句柄复用）。无缓存时缩放动画
// 每帧 InvalidateScene → CollectDecorations → new PlanarGridGraphic（每组新
// GL buffer/秒 60 次）——打开视图卡、resize 卡、bump arena 持续增长的三重
// 根因。参考 createPlanarGrid 是轻量 shader quad（itwinjs 重建便宜），DanQing
// 的多 buffer 图形用 update 路径等价。
dqRender::RenderGraphic* Viewport::createPlanarGrid(dqCommon::Frustum const& frustum,
                                                    dqRender::PlanarGridProps const& props)
{
    if (!m_pipeline || !m_pipeline->renderSystem())
        return nullptr;
    auto* sys = m_pipeline->renderSystem();
    if (m_gridGraphic) {
        sys->updatePlanarGridFrustum(m_gridGraphic, frustum, props);
        return m_gridGraphic;
    }
    m_gridGraphic = sys->createPlanarGrid(frustum, props);
    return m_gridGraphic;
}

std::unique_ptr<dqRender::GraphicBuilder> Viewport::createGraphicBuilder(
    dqRender::GraphicBuilderOptions const& options)
{
    // Delegate to this viewport's RenderSystem (owned by the RenderPipeline) —
    // same per-viewport-driver routing as createGraphicFromPolyface/List/Owner.
    // Ported from: itwinjs-core RenderSystem.createGraphicBuilder.
    if (!m_pipeline)
        return nullptr;
    auto* sys = m_pipeline->renderSystem();
    return sys ? sys->createGraphicBuilder(options) : nullptr;
}

void Viewport::ChangeView(dqBase::RefPtr<ViewState> view, ViewChangeOptions const* opts)
{
    if (!view) return;

    // Ported from: itwinjs-core ScreenViewport.changeView (Viewport.ts:3610-3611)
    // — nothing to do
    if (view.Get() == m_view.Get())
        return;

    // Viewport.ts:3613 — this.setAnimator(undefined)：切换前清活动动画器
    // （make sure we clear any active animators before we change views）。
    m_animator.reset();

    // :3615 — opts = opts ?? { animationTime: ScreenViewport.animation.time.slow.milliseconds }
    ViewChangeOptions options;
    if (opts)
        options = *opts;
    else
        options.animationTime = animation().time.slow;  // 1.25s

    // :3618 — determined whether we can animate this ViewState change:
    //   doAnimate = this.view && this.view.hasSameCoordinates(view) && false !== opts.animateFrustumChange
    // （undefined ≠ false ⇒ 坐标一致即动画）。
    bool const doAnimate = m_view && m_view->hasSameCoordinates(*view)
                        && options.animateFrustumChange != false;

    // Capture previous view for onChangeView event.
    // Ported from: itwinjs-core Viewport.ts changeView() (line 1840-1845)
    dqBase::RefPtr<ViewState> prevView = m_view;

    // :3619-3620 — if we can animate, don't throw out view undo.
    if (!doAnimate)
        clearViewUndo();

    // 1. Detach from old view (unsubscribe all listeners)
    DetachFromView();

    // 2. Assign new view
    m_view = std::move(view);

    // 3. Attach to new view (subscribe to events)
    AttachToView();

    // 4. Rebuild transform chain
    SetupFromView();

    // 6. invalidate all rendering state
    InvalidateController();

    // 7. Notify
    // Ported from: itwinjs-core Viewport.ts changeView() (line 1845-1846)
    if (prevView && prevView.Get() != m_view.Get()) {
        OnChangeView.Raise(prevView.Get());
        m_changeFlags.SetViewState();
    }
    emit ViewChanged();
    // Viewport.ts:3623 — 切换后保存新基线。
    saveViewUndo();
    // :3626 — if (doAnimate) this.animateFrustumChange(opts);
    if (doAnimate)
        animateFrustumChange(options.ToAnimationOptions());
    update();  // trigger repaint
}

// ---------------------------------------------------------------------------
// 视图撤销/重做栈
// Ported from: itwinjs-core Viewport.ts:3634-3718
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Viewport.saveViewUndo (Viewport.ts:3649-3676)。
void Viewport::saveViewUndo()
{
    if (m_inViewChangedEvent) return;                       // :3650-3651 echo 早退
    auto* view3d = m_view ? m_view->AsViewState3d() : nullptr;
    if (!view3d) return;                                    // DanQing 无 2D 视图——无姿态可存

    if (!m_currentBaseline)
        m_currentBaseline = view3d->savePose();             // :3654-3655 首调用建基线
    if (m_currentBaseline->equalState(*m_view))
        return;                                             // :3657-3658 无变化

    if (static_cast<int>(m_backStack.size()) >= maxUndoSteps)
        m_backStack.erase(m_backStack.begin());             // :3661-3662 满 20 shift 最老

    // :3664-3673 防抖：0.5s 窗口内的快速连续保存不追加新条目。
    // :3669 undoDelay.isZero 为置零关防抖的参考逃生口（s_undoDelay 公有可变）。
    auto const now = dqBase::DqTimePoint::Now();
    if (s_undoDelay.IsZero() || m_backStack.empty() || (m_backStack.back()->undoTime + s_undoDelay) < now) {
        m_currentBaseline->undoTime = now;                  // :3670
        m_backStack.push_back(std::move(m_currentBaseline)); // :3671 存前一个状态
        m_forwardStack.clear();                             // :3672 不可再 redo
    }
    m_currentBaseline = view3d->savePose();                 // :3675 新基线
}

// Ported from: itwinjs-core Viewport.doUndo (Viewport.ts:3679-3690)。
void Viewport::doUndo()
{
    if (m_backStack.empty() || !m_currentBaseline) return;
    m_forwardStack.push_back(std::move(m_currentBaseline));  // :3683
    m_currentBaseline = std::move(m_backStack.back());       // :3686
    m_backStack.pop_back();
    m_view->AsViewState3d()->applyPose(*m_currentBaseline);  // :3687 view.applyPose
    finishUndoRedo();                                        // :3688
    OnViewUndoRedo.Raise(this, ViewUndoEvent::Undo);         // :3689
}

// Ported from: itwinjs-core Viewport.doRedo (Viewport.ts:3693-3704)。
void Viewport::doRedo()
{
    if (m_forwardStack.empty() || !m_currentBaseline) return;
    m_backStack.push_back(std::move(m_currentBaseline));     // :3697
    m_currentBaseline = std::move(m_forwardStack.back());    // :3700
    m_forwardStack.pop_back();
    m_view->AsViewState3d()->applyPose(*m_currentBaseline);  // :3701
    finishUndoRedo();                                        // :3702
    OnViewUndoRedo.Raise(this, ViewUndoEvent::Redo);         // :3703
}

// Ported from: itwinjs-core Viewport.finishUndoRedo (Viewport.ts:3706-3712)。
// updateChangeFlags 未移植（undo 只动姿态——displayStyle/categories 标志不变，
// 参考的 updateChangeFlags 对姿态类撤销无可观测效应，TODO 随完整 changeView 对照补）。
void Viewport::finishUndoRedo()
{
    SetupFromView();          // :3709
    InvalidateController();   // 参考级联失效的 DanQing 等价（doSetupFromView:2052-2053 链）
}

// Ported from: itwinjs-core Viewport.clearViewUndo/resetUndo (Viewport.ts:3641-3646, 3715-3718)。
void Viewport::clearViewUndo()
{
    m_currentBaseline.reset();
    m_forwardStack.clear();
    m_backStack.clear();
    m_lastPose.reset();                       // :3645
}

void Viewport::resetUndo()
{
    clearViewUndo();
    saveViewUndo();   // :3717 建立新基线
}

// Ported from: itwinjs-core ScreenViewport.synchWithView (Viewport.ts:3585-3597)。
void Viewport::synchWithView(ViewChangeOptions const& options)
{
    SetupFromView(options.skipAspectFix);  // super.synchWithView() + skipAspectFix 门控
    if (!options.noSaveInUndo)
        saveViewUndo();                       // :3593-3594
    InvalidateController();                   // 参考 setupFromView 内部级联的等价（见 .h 注释）
    if (options.animateFrustumChange == true)  // :3595-3596 — `true === options.animateFrustumChange`
        animateFrustumChange(options.ToAnimationOptions());
}

// Ported from: itwinjs-core Viewport.animateFrustumChange (Viewport.ts:3519-3522)。
void Viewport::animateFrustumChange(AnimationOptions const& options)
{
    if (!m_lastPose || !m_currentBaseline)
        return;                               // :3520 — 两姿态缺一不动画
    auto* view3d = m_view ? m_view->AsViewState3d() : nullptr;
    if (!view3d)
        return;
    auto endPose = view3d->savePose();        // :3521 — this.view.savePose()
    if (!endPose || endPose->getType() != ViewPoseType::View3d ||
        m_lastPose->getType() != ViewPoseType::View3d)
        return;
    setAnimator(std::make_unique<FrustumAnimator>(options, this,
                static_cast<ViewPose3d const&>(*m_lastPose),
                static_cast<ViewPose3d const&>(*endPose)));
}

void Viewport::AttachToView()
{
    if (!m_view) return;

    // Register DisplayStyle event listeners
    // ← itwinjs-core: registerDisplayStyleListeners()
    auto& style = m_view->GetDisplayStyle();

    // Core display style events
    m_viewEventScope.add(style.OnViewFlagsChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onViewFlagsChanged
        // If background map changed, invalidate controller; otherwise render plan
        InvalidateRenderPlan();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnBackgroundColorChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onBackgroundColorChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnEnvironmentChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onEnvironmentChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnLightSettingsChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onLightsChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    // Additional display style events
    m_viewEventScope.add(style.OnSubCategoryOverridesChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onSubCategoryOverridesChanged
        m_changeFlags.SetDisplayStyle();
        m_changeFlags.SetFeatureOverridesDirty();
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(style.OnModelAppearanceOverrideChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onModelAppearanceOverrideChanged
        m_changeFlags.SetDisplayStyle();
        m_changeFlags.SetFeatureOverridesDirty();
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(style.OnMonochromeColorChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onMonochromeColorChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnWhiteOnWhiteReversalChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onWhiteOnWhiteReversalChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnBackgroundMapChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onBackgroundMapChanged
        InvalidateController();
    }));

    m_viewEventScope.add(style.OnClipStyleChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onClipStyleChanged
        m_changeFlags.SetDisplayStyle();
        m_changeFlags.SetFeatureOverridesDirty();
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(style.OnExcludedElementsChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onExcludedElementsChanged
        m_changeFlags.SetDisplayStyle();
        InvalidateScene();
        m_changeFlags.SetFeatureOverridesDirty();
    }));

    m_viewEventScope.add(style.OnThematicChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onThematicChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnHiddenLineChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onHiddenLineSettingsChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnAmbientOcclusionChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onAmbientOcclusionSettingsChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnSolarShadowsChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onSolarShadowsChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnPlanProjectionChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onPlanProjectionSettingsChanged
        m_changeFlags.SetDisplayStyle();
        RequestRedraw();
    }));

    m_viewEventScope.add(style.OnAnalysisStyleChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onAnalysisStyleChanged
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(style.OnAnalysisFractionChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onAnalysisFractionChanged
        m_analysisFractionValid = false;
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(style.OnRenderTimelineChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onRenderTimelineChanged
        InvalidateScene();
    }));

    m_viewEventScope.add(style.OnScheduleScriptChanged.AddListener([this]() {
        // ← itwinjs-core: style.onScheduleScriptChanged
        m_timePointValid = false;
        InvalidateScene();
    }));

    m_viewEventScope.add(style.OnTimePointChanged.AddListener([this]() {
        // ← itwinjs-core: settings.onTimePointChanged
        m_timePointValid = false;
        InvalidateScene();
    }));

    // Register ViewState event listeners
    // ← itwinjs-core: registerViewListeners()
    m_viewEventScope.add(m_view->OnDisplayStyleChanged.AddListener([this]() {
        // ← itwinjs-core: view.onDisplayStyleChanged
        m_changeFlags.SetDisplayStyle();
        m_changeFlags.SetFeatureOverridesDirty();
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(m_view->OnViewedCategoriesChanged.AddListener([this]() {
        // ← itwinjs-core: view.onViewedCategoriesChanged
        m_changeFlags.SetViewedCategories();
        InvalidateScene();
    }));

    m_viewEventScope.add(m_view->OnViewedModelsChanged.AddListener([this]() {
        // ← itwinjs-core: view.onViewedModelsChanged
        m_changeFlags.SetViewedModels();
        InvalidateScene();
    }));

    m_viewEventScope.add(m_view->OnClipVectorChanged.AddListener([this]() {
        // ← itwinjs-core: view.details.onClipVectorChanged
        InvalidateRenderPlan();
    }));

    m_viewEventScope.add(m_view->OnCategorySelectorChanged.AddListener([this]() {
        // ← itwinjs-core: view.onCategorySelectorChanged
        m_changeFlags.SetViewedCategories();
        InvalidateScene();
    }));

    m_viewEventScope.add(m_view->OnModelSelectorChanged.AddListener([this]() {
        // ← itwinjs-core: view.onModelSelectorChanged
        m_changeFlags.SetViewedModels();
        InvalidateScene();
    }));

    m_viewEventScope.add(m_view->OnRenderModeChanged.AddListener([this]() {
        // ← itwinjs-core: view.onRenderModeChanged
        RequestRedraw();
    }));

    m_viewEventScope.add(m_view->OnDetailsChanged.AddListener([this]() {
        // ← itwinjs-core: view.onDetailsChanged
        RequestRedraw();
    }));

    m_viewEventScope.add(m_view->OnGridOrientationChanged.AddListener([this]() {
        // ← itwinjs-core: view.onGridOrientationChanged (ViewState3d)
        RequestRedraw();
    }));

    // Bidirectional binding: ViewState knows about this Viewport
    // ← itwinjs-core: view.attachToViewport(this)
    m_view->AttachToViewport(this);
}

void Viewport::DetachFromView()
{
    // Unsubscribe all event listeners
    m_viewEventScope.DisconnectAll();

    // Remove bidirectional binding
    if (m_view) {
        m_view->DetachFromViewport(this);
    }
}

void Viewport::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // Deferred initialization: create RenderPipeline + Swapchain on first show.
    // winId() returns the native window handle (NSView on macOS, HWND on Windows).
    // The Swapchain binds the GL context to this window surface.
    if (m_needsInit) {
        Init();
        m_needsInit = false;
    }
}

// WinIdChange — Qt 销毁重建本控件原生窗口（WA_NativeWindow 子控件在 MDI 重排/
// 最大化等状态跃迁时）后送达。交换链仍绑在已死的旧 HWND 上：present 静默停止
// 上屏（SwapBuffers 成功但到达不了 DWM 表面）——屏幕冻结在旧帧，即"最大化后
// Grid 消失/黑色大方块"。此处把当前句柄交给 GL 层重绑表面（Swapchain::rebind，
// 纯图形 API；窗口生命周期归应用层——Qt 只在本层使用）。
bool Viewport::event(QEvent* event)
{
    // TEMP-DIAG（黑方块真机取证）：窗口系统事件轨迹。WinIdChange/Show/Hide/Moved
    // 全打印（env DANQING_GL_TRACE=1 门控）。
    static bool const s_trace = std::getenv("DANQING_GL_TRACE") != nullptr;
    if (s_trace) {
        switch (event->type()) {
        case QEvent::WinIdChange:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::ParentChange:
        case QEvent::WindowStateChange:
        case QEvent::PlatformSurface:
            fprintf(stderr, "[VPEVT] vp=%d type=%d winIdNow=%p initHwnd=%p geom=(%d,%d %dx%d)\n",
                    m_viewportId, static_cast<int>(event->type()),
                    reinterpret_cast<void*>(winId()), m_dbgInitHwnd,
                    x(), y(), width(), height());
            break;
        default:
            break;
        }
    }
    if (event->type() == QEvent::WinIdChange && m_pipeline && !m_isShuttingDown) {
        if (auto* swapchain = m_pipeline->getSwapchain()) {
            if (s_trace)
                fprintf(stderr, "[WINID] rebind: old=%p new=%p\n", m_dbgInitHwnd,
                        reinterpret_cast<void*>(winId()));
            swapchain->rebind(reinterpret_cast<void*>(winId()));
            m_dbgInitHwnd = reinterpret_cast<void*>(winId());  // TEMP-DIAG
            InvalidateController();  // 旧表面上的帧已不可见——重绘一帧上屏
        }
    }
    // 最小化→恢复黑屏修复（2026-09-14 用户报告，三段）：
    // ① Show→rebind 重建表面（表面层：同 §10.4 resize=表面过期家族）。实测
    //    rebind 后交换链呈现能力完好（显式帧立即上屏）。
    // ② QEvent::Paint→RequestRedraw（触发层）：Windows 恢复/揭开窗口经
    //    WM_PAINT 送达"现在需要内容"（浏览器类比：rAF 只在页面可见后恢复）。
    //    但 Paint 是单次触发语义——若唯一一次落在原生窗口完成映射之前，
    //    帧进虚空且 Qt 已 validate 窗口、不再补 WM_PAINT（3 循环复现第 3
    //    次仍黑的实测形态）。
    // ③ Show 时安排延迟补帧（150ms/400ms，覆盖 Win11 恢复动画窗口期）：
    //    保证至少一帧落在窗口完全可见之后。事件驱动、有界（各一次），
    //    若早帧已上屏则只是多画一帧（无副作用）。
    if (event->type() == QEvent::Show && m_pipeline && m_initialized && !m_isShuttingDown) {
        if (auto* swapchain = m_pipeline->getSwapchain()) {
            if (s_trace)
                fprintf(stderr, "[SURFHEAL] Show after minimize: rebuild surface\n");
            swapchain->rebind(reinterpret_cast<void*>(winId()));
            InvalidateController();
        }
        QTimer::singleShot(150, this, [this]() {
            if (!m_isShuttingDown && m_initialized) {
                if (std::getenv("DANQING_GL_TRACE"))
                    fprintf(stderr, "[SURFHEAL] settle redraw (+150ms)\n");
                RequestRedraw();
            }
        });
        QTimer::singleShot(400, this, [this]() {
            if (!m_isShuttingDown && m_initialized) {
                if (std::getenv("DANQING_GL_TRACE"))
                    fprintf(stderr, "[SURFHEAL] settle redraw (+400ms)\n");
                RequestRedraw();
            }
        });
    }
    if (event->type() == QEvent::Paint && m_pipeline && m_initialized && !m_isShuttingDown) {
        if (s_trace)
            fprintf(stderr, "[SURFHEAL] Paint: request redraw\n");
        RequestRedraw();
    }
    return QWidget::event(event);
}

void Viewport::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Crash-on-close guard: don't recreate the render target into a pipeline
    // that Shutdown() has released (a resize can be delivered during teardown).
    if (m_isShuttingDown) return;

    // RenderFrame 重入守卫：drawFrame 的 swapchain->acquire()（wglMakeCurrent）
    // 内部可能泵 Windows 消息，resizeEvent 若在 RenderFrame 中途重入，delete
    // m_renderTarget 会让 drawFrame 持有的 m_target 引用悬空（m_target.getRenderTarget()
    // 返回已 deallocate 的 handle → beginRenderPass 早退 → endRenderPass 断言，
    // 最大化崩溃根因）。标记重入，延至帧尾处理。
    if (m_inRenderFrame) {
        m_pendingResize = true;
        return;
    }

    // Update Swapchain dimensions and recreate render target.
    // On Retina displays, the framebuffer is 2x the logical widget size.
    int dpr = static_cast<int>(devicePixelRatio());
    int fbWidth = width() * dpr;
    int fbHeight = height() * dpr;

    // 最大化/最小化动画期间 Qt 可能发多个 resizeEvent，中间态 size 可为 0
    // （showMaximized 的 SetWindowPos 序列）。0 尺寸重建 render target 会创建
    // 不完整 FBO（glTexStorage2D(0,0) 失败），beginRenderPass 绑定后 GL 状态
    // 错乱（最大化崩溃根因之一）。0 尺寸跳过重建，保留旧 target（下一有效
    // resize 会重建）。
    if (fbWidth <= 0 || fbHeight <= 0) {
        InvalidateController();
        emit ViewportResized();
        return;
    }

    if (m_pipeline && m_pipeline->isInitialized()) {
        auto* swapchain = m_pipeline->getSwapchain();
        if (swapchain) {
            swapchain->resize(static_cast<uint32_t>(fbWidth),
                              static_cast<uint32_t>(fbHeight));
        }

        // ← itwinjs-core renderFrame step 7（Viewport.ts:2604-2608）：
        //   resized = target.updateViewRect(); if (resized) { target.onResized();
        //   this.invalidateController(); }
        // Target 对象不销毁——setViewRect = updateViewRect+onResized 合并语义
        // （更新 renderRect + 尺寸变化时 disposeFbo 销毁自身合成 FBO，Target.ts:
        // 1330-1336/1441-1443/315-330）；下一帧 drawFrame 入口 assignDC 惰性
        // 重建（Target.ts:751-766/290-313），compositor 纹理由其 preDraw 自行
        // 重建。此前的 delete+createRenderTarget 整只销毁路径销毁
        // CompositorFrameBuffers 触发 80-110ms 的 glDelete 驱动同步等待
        // （resize 卡顿根因），且偏离参考机制（Target 生命周期跨 resize）。
        m_renderTarget->setViewRect(dqRender::ViewRect(
            0, 0, static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight)));
    }
    InvalidateController();
    emit ViewportResized();
}

// ---------------------------------------------------------------------------
// Init — create dqRender pipeline from native window handle
// ---------------------------------------------------------------------------
void Viewport::Init()
{
    // Get the native window handle (NSView on macOS, HWND on Windows).
    // winId() forces Qt to create the native window if not already done.
    WId nativeHandle = winId();
    if (!nativeHandle) return;

    // Compute Retina-aware framebuffer dimensions
    int dpr = static_cast<int>(devicePixelRatio());
    uint32_t fbWidth = static_cast<uint32_t>(width() * dpr);
    uint32_t fbHeight = static_cast<uint32_t>(height() * dpr);

    // Create RenderPipeline with native window handle.
    // This creates the RHI Driver, Swapchain (binds GL context to window),
    // RenderSystem, and all shader techniques.
    m_pipeline = std::make_unique<dqRender::RenderPipeline>();
    m_dbgInitHwnd = reinterpret_cast<void*>(nativeHandle);  // TEMP-DIAG
    if (!m_pipeline->initialize(reinterpret_cast<void*>(nativeHandle), fbWidth, fbHeight)) {
        m_pipeline.reset();
        return;
    }

    // NOTE: the procedural ground grid is NO LONGER built here. It is rebuilt each
    // time the scene is invalidated (i.e. whenever the camera frustum changes) in
    // CreateScene() from the live view frustum — faithful to itwinjs-core
    // ViewContext.drawStandardGrid (ViewContext.ts:348), which calls
    // createPlanarGrid(vp.getFrustum(), props) so the infinite grid plane tracks
    // pan/zoom. Building it once at init from a synthetic frustum left the grid
    // frozen; the per-frame-frustum rebuild fixes that.

    // Create the RenderTarget (bridges Viewport → TargetImpl → SceneCompositor).
    // ← itwinjs-core: IModelApp.renderSystem.createTarget(canvas)
    m_renderTarget = m_pipeline->createRenderTarget(fbWidth, fbHeight).release();

    // Feed the real DPR to the target — the canvas-decoration flush scales
    // view-px to device-px by it (ImGuiHelper io.DisplayFramebufferScale
    // equivalent; default 1.0 was the "circle stuck in the top-left quarter"
    // root cause on a 2× display).
    if (m_renderTarget)
        m_renderTarget->setDevicePixelRatio(devicePixelRatioF());

    m_initialized = true;
}

// ---------------------------------------------------------------------------
// Invalidation cascade (← itwinjs-core Viewport.ts invalidation chain)
//
// invalidateController → invalidateRenderPlan → invalidateScene → invalidateDecorations
// invalidateScene → invalidateDecorations
// invalidateDecorations → requestNextAnimation
// requestRedraw → requestNextAnimation (no invalidation)
// ---------------------------------------------------------------------------

void Viewport::InvalidateController()
{
    m_controllerValid = false;
    InvalidateRenderPlan();  // cascade
}

void Viewport::InvalidateRenderPlan()
{
    m_renderPlanValid = false;
    InvalidateScene();  // cascade
}

void Viewport::InvalidateScene()
{
    m_sceneValid = false;
    // 缓存图形随 cache.clear() 被 delete——先把引用它们的帧装饰列表一并清空，
    // 否则列表悬空直到下一帧 CollectDecorations：此窗口内的任何 pick 回读
    // （readPixels → RenderCommands::initForReadPixels 遍历 dec->normal）会对已
    // 释放图形做 isPickable 虚调用（SEH 0xc0000005——2026-09-19 滚轮缩放崩溃
    // 复现：doZoom 的 InvalidateController 级联触发本路径，随后的 updateDynamics
    // → onMouseMotion → pickDepthPoint 正中悬空图形）。
    m_decorations.clear();
    m_decorationCache.clear();  // ← itwinjs-core: clear decoration cache on scene invalidation
    InvalidateDecorations();  // cascade
    emit SceneInvalidated();
}

void Viewport::InvalidateDecorations()
{
    m_decorationsValid = false;
    RequestRedraw();
}

void Viewport::RequestRedraw()
{
    m_redrawPending = true;
    Application::Get().RequestNextAnimation();
}

// ---------------------------------------------------------------------------
// Highlight management
// Ported from: itwinjs-core Viewport.renderFrame step 6
//               (Viewport.ts:2613-2617: if (_selectionSetDirty)
//                { target.setHiliteSet(view.iModel.hilited); ... })
// ---------------------------------------------------------------------------
void Viewport::SetHilitedFeature(uint32_t featureId)
{
    if (featureId == m_hiliteFeatureId)
        return;

    // DanQing-side state mirror (GetHilitedFeature accessor). The DRAW path
    // follows the reference: the iModel's HiliteSet is pushed to the target
    // from renderFrame step 6 when the selection set is dirty — the LUTs live
    // on the target's batches (§8.4), not here.
    m_hiliteFeatureId = featureId;
    SetSelectionSetDirty();

    // Hilite color travels with any hilite draw (compositor-owned default).
    if (m_renderTarget) {
        m_renderTarget->setHiliteColor(m_hiliteColor[0], m_hiliteColor[1], m_hiliteColor[2]);
    }

    // Trigger a redraw
    InvalidateDecorations();
}

void Viewport::setHiliteColor(float r, float g, float b)
{
    m_hiliteColor[0] = r;
    m_hiliteColor[1] = g;
    m_hiliteColor[2] = b;
}

dqRender::RenderSystem* Viewport::renderSystem() const
{
    // ← itwinjs-core: viewport.target.renderSystem — DanQing's render system is
    // held by the per-viewport RenderPipeline (createPlanarGrid 同款路由).
    return m_pipeline ? m_pipeline->renderSystem() : nullptr;
}

// ---------------------------------------------------------------------------
// Per-model category visibility integration
// Ported from: itwinjs-core Viewport.ts addModelSubCategoryVisibilityOverrides (line 1519)
// ---------------------------------------------------------------------------
void Viewport::addModelSubCategoryVisibilityOverrides(dqCommon::FeatureOverrides& fs)
{
    // Iterate per-model category overrides and populate model subcategory overrides.
    // For each (model, category) override, we invert the subcategory visibility
    // for that model. This requires knowing which subcategories belong to each
    // category — resolved via the ViewState's category selector.
    //
    // Ported from: itwinjs-core PerModelCategoryVisibility.Overrides.addOverrides()
    //              (PerModelCategoryVisibility.ts lines 231-267)
    for (auto const& entry : m_perModelCategoryVisibility.getEntries()) {
        if (entry.override == dqCommon::PerModelCategoryOverride::None)
            continue;

        // Resolve category → subcategories via the ViewState.
        // In Phase 1 (blank connection), categories have no subcategories,
        // so we use the category ID itself as the subcategory key.
        // This matches itwinjs-core's behavior when no subcategory cache is available.
        uint64_t modelId = entry.modelId;
        uint64_t categoryId = entry.categoryId;

        // The override inverts visibility: if the per-model override says Hide
        // and the global visibility says Show, the subcategory becomes hidden
        // for that model (and vice versa). The m_modelSubCategoryOverrides set
        // records which subcategories should have inverted visibility.
        // Ported from: itwinjs-core PerModelCategoryVisibility.ts line 258-260
        bool shouldShow = (entry.override == dqCommon::PerModelCategoryOverride::Show);
        bool isGloballyVisible = fs.isSubCategoryVisible(
            static_cast<uint32_t>(categoryId & 0xFFFFFFFF),
            static_cast<uint32_t>(categoryId >> 32));

        // Only add override if it differs from global visibility (would cause inversion)
        if (shouldShow != isGloballyVisible) {
            fs.addModelSubCategoryOverride(modelId, categoryId);
        }
    }
}

// ---------------------------------------------------------------------------
// Flash management
// Ported from: itwinjs-core Viewport.ts lines 2485-2507
// ---------------------------------------------------------------------------
void Viewport::SetFlashedId(uint32_t id)
{
    if (m_flashedId == id) return;

    uint32_t previous = m_flashedId;
    m_flashedId = id;

    // 强度/计时器的复位在 processFlash 的 id-change 分支（参考 :2524-2529）。
    // flash 不是装饰（LUT Flashed 位 + 逐帧 u_flash_intensity uniform），不
    // InvalidateDecorations——只需一帧来爬升（参考经 requestNextAnimation）。
    RequestRedraw();

    emit OnFlashedIdChanged(previous, id);
}

bool Viewport::processFlash()
{
    // Ported from: itwinjs-core Viewport.ts:2521-2543（逐行对照）。默认
    // flashSettings：duration 0.25s、maxIntensity 1.0（FlashSettings.ts:72-81）。
    constexpr qint64 kFlashDurationMs = 250;
    constexpr float kMaxIntensity = 1.0f;

    bool needsFlashUpdate = false;

    if (m_flashedId != m_lastFlashedElem) {
        m_flashIntensity = 0.0f;
        m_flashUpdateTimeMs = QDateTime::currentMSecsSinceEpoch();
        m_lastFlashedElem = m_flashedId;  // flashing has begun; this is now the previous flash
        needsFlashUpdate = (m_flashedId == 0);  // flash 关闭也要通知渲染一帧（参考 :2528）
    }

    if (m_flashedId != 0 && m_flashIntensity < kMaxIntensity) {
        qint64 const flashElapsed = QDateTime::currentMSecsSinceEpoch() - m_flashUpdateTimeMs;
        float const intensity = static_cast<float>(
            std::min<qint64>(flashElapsed, kFlashDurationMs))
            / static_cast<float>(kFlashDurationMs);
        m_flashIntensity = std::min(intensity, kMaxIntensity);

        needsFlashUpdate = true;
    }

    return needsFlashUpdate;
}

// ---------------------------------------------------------------------------
// Animation management
// Ported from: itwinjs-core Viewport.ts lines 2537-2539
// ---------------------------------------------------------------------------
void Viewport::setAnimator(std::unique_ptr<Animator> animator)
{
    if (m_animator) {
        m_animator->interrupt();
    }
    // 单槽互斥：拥有型设置时清非拥有引用（参考单 _animator 槽语义）。
    if (m_animatorRef) {
        m_animatorRef->interrupt();
        m_animatorRef = nullptr;
    }
    m_animator = std::move(animator);
    if (m_animator) {
        RequestRedraw();
    }

    // Ported from: itwinjs-core Viewport.setAnimator (Viewport.ts:2347-2355)。
    // :2354 — this.animate()：立即推进新动画器一次（set up the initial frustum，
    // 参考注释：TwoWayViewportSync 防首帧闪到终态）。DanQing 缺失时，首 tick 落在
    // 下一帧（~16ms 后），且 FrustumAnimator 首 tick 才以此刻为 tween 起点
    // （m_startTimeValid 惰性初始化）——连滚动画器重启链的起点被系统性滞后
    // 一帧，净缩放比 DTA 慢。立即 animate 与参考 1:1；animate() 返回 true
    // （零时长动画器）时即丢弃（Viewport.ts:2573-2575 同一契约）。
    if (m_animator && m_animator->animate()) {
        m_animator.reset();
    }
}

// 非拥有 animator 注册（← 参考 vp.setAnimator(this) 的 GC 语义适配，见 .h 注释）。
void Viewport::setAnimatorRef(Animator* animator)
{
    if (m_animatorRef) {
        m_animatorRef->interrupt();
    }
    // 单槽互斥：非拥有型设置时清拥有型（参考单 _animator 槽语义）。
    if (m_animator) {
        m_animator->interrupt();
        m_animator.reset();
    }
    m_animatorRef = animator;
    if (m_animatorRef) {
        RequestRedraw();
        // Viewport.ts:2354 同款立即 tick——animate() 返回 true（已完成）即清除。
        if (m_animatorRef->animate())
            m_animatorRef = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Frustum / view wrappers (Task 4)
// Thin delegates over the Task-3 ViewState3d frustum API.
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Viewport.getFrustum (Viewport.ts:2113)
//   return this._viewingSpace.getFrustum(sys, adjustedBox, box);
dqCommon::Frustum Viewport::getFrustum(bool world, bool adjustedBox) const
{
    dqCommon::Frustum out;
    if (m_view && m_view->AsViewState3d() != nullptr) {
        m_viewingSpace.getFrustum(
            out,
            world ? dqCommon::CoordSystem::World : dqCommon::CoordSystem::View,
            adjustedBox);
    }
    return out;
}

// Ported from: itwinjs-core Viewport.getWorldFrustum (Viewport.ts:2116)
//   getFrustum(CoordSystem.World, false) —— **未扩展**盒（视图工具工作视锥）。
dqCommon::Frustum Viewport::getWorldFrustum() const
{
    return getFrustum(true, /*adjustedBox=*/false);
}

// Ported from: itwinjs-core Viewport.setupViewFromFrustum (Viewport.ts:2289)
bool Viewport::setupViewFromFrustum(dqCommon::Frustum const& inFrustum)
{
    // ← itwinjs: const validSize = this.view.setupFromFrustum(inFrustum);
    //            return (ViewStatus.Success === this.setupFromView() && ...);
    // Note: always call SetupFromView, even if SetupFromFrustum failed.
    // SetupFromFrustum lives on ViewState3d; 2D/null views report invalid.
    auto* v3 = m_view ? m_view->AsViewState3d() : nullptr;
    bool const valid = v3 ? v3->SetupFromFrustum(inFrustum) : false;
    SetupFromView();
    InvalidateController();
    RequestRedraw();
    return valid;
}

// Ported from: itwinjs-core Viewport.scroll (Viewport.ts:2121-2141)
void Viewport::scroll(dqGeom::Vector3d const& dist)
{
    // ← itwinjs orthographic branch (lines 2133-2138):
    //   const pts = [new Point3d(), dist];
    //   this.viewToWorldArray(pts);
    //   const dist = pts[1].minus(pts[0]);
    //   view.setOrigin(view.getOrigin().plus(dist));
    //
    // Step 4 simplification: treat `dist` as already in world coordinates
    // (the itwinjs wrapper converts screen-pixel distance to world distance
    // via viewToWorldArray before translating the origin; that conversion
    // needs the full ViewingSpace viewToWorld path and is deferred).
    // TODO: full screen→world conversion — Task 10 (ViewScroll handle).
    if (!m_view) return;

    auto const origin = m_view->GetOrigin();
    m_view->SetOrigin({origin.x + dist.x, origin.y + dist.y, origin.z + dist.z});

    // ← itwinjs: this.synchWithView(options)  (invalidateController + redraw)
    InvalidateController();
    RequestRedraw();
}

// Ported from: itwinjs-core ViewState3d.getRotation (accessed via Viewport
// machinery); the Viewport wrapper is DanQing glue for tool access.
dqGeom::Matrix3d Viewport::getRotation() const
{
    auto* v3 = m_view ? m_view->AsViewState3d() : nullptr;
    return v3 ? v3->getRotation() : dqGeom::Matrix3d::CreateIdentity();
}

// Ported from: itwinjs-core Viewport.getContrastToBackgroundColor
// (Viewport.ts:2480-2483) + _wantInvertBlackAndWhite (:2475-2478).
dqCommon::ColorDef Viewport::getContrastToBackgroundColor() const
{
    if (!m_view)
        return dqCommon::ColorDef::black;
    // Viewport.ts:2476-2477 — a bright background ((r+g+b) > (255*3)/2) inverts to black.
    // For integer r+g+b, (x > (255*3)/2) with C++ integer division (==382) matches the
    // reference's (x > 382.5) exactly.
    auto const bgTbgr = m_view->GetDisplayStyle().getBackgroundColor();
    auto const c = dqCommon::ColorDef::fromTbgr(bgTbgr).getColors();
    bool const wantInvert = (c.r + c.g + c.b) > (255 * 3) / 2;
    return wantInvert ? dqCommon::ColorDef::black : dqCommon::ColorDef::white;
}

// Ported from: itwinjs-core Viewport.isGridOn (Viewport.ts:637).
bool Viewport::isGridOn() const noexcept
{
    return m_view && m_view->getViewFlags().grid();
}

// Ported from: itwinjs-core ScreenViewport.viewRect (Viewport.ts:3505)
ViewRect Viewport::viewRect() const noexcept
{
    // ← itwinjs: return new ViewRect(0, 0, this.canvasSize.width, this.canvasSize.height);
    // QWidget::width()/height() report the current geometry (Qt default-initializes
    // top-level widgets to 640x480; both update synchronously on resize/show).
    ViewRect r;
    r.left = 0;
    r.top = 0;
    r.right = width();
    r.bottom = height();
    return r;
}

// Ported from: itwinjs-core Viewport.isCameraOn (Viewport.ts:1741)
bool Viewport::isCameraOn() const noexcept
{
    // ← itwinjs: public get isCameraOn(): boolean { return undefined !== this.view && this.view.isCameraOn; }
    auto* v3 = m_view ? m_view->AsViewState3d() : nullptr;
    return v3 && v3->IsCameraOn();
}

// Ported from: itwinjs-core Viewport.pickDepthPoint (Viewport.ts:3394)
dqGeom::Point3d Viewport::pickDepthPoint(dqGeom::Point3d pt, double /*pickRadius*/) const
{
    // Step 4 stub: no depth-pick buffer read in Step 3 (geometry not yet
    // renderable end-to-end on macOS — shader-link issue, see memory).
    // TODO: real depth pick (readPixels) — Step 4 with geometry.
    return pt;
}

// ---------------------------------------------------------------------------
// Tile tree management (Phase 2b)
// Ported from: itwinjs-core Viewport.addTileTreeReference()
// ---------------------------------------------------------------------------
void Viewport::AddTileTree(dqRender::TileTree* tree)
{
    if (!tree) return;

    // Check if already added
    for (auto* existing : m_tileTrees) {
        if (existing == tree) return;
    }

    // Inject this viewport's render system into the tree — tile content
    // (readContent → createGraphicFromPolyface) is created with it. The
    // reference passes the system down the readContent chain explicitly
    // (Tile.ts:429); DanQing's real system is per-viewport (RenderPipeline
    // owned), so the tree holds the injected pointer instead.
    tree->setRenderSystem(renderSystem());

    m_tileTrees.push_back(tree);
    // Scaffold bridge: wrap the caller-owned tree in the faithful
    // owner/reference chain so it flows through the same
    // Viewport -> refs/providers -> addToScene path as real references.
    m_scaffoldRefs.push_back(std::make_unique<SimpleTileTreeReference>(tree));
    InvalidateScene();
}

void Viewport::AddTiledGraphicsProvider(TiledGraphicsProvider* provider)
{
    // <- itwinjs-core Viewport.addTiledGraphicsProvider (Viewport.ts:1729-1732).
    if (!provider)
        return;
    for (auto* existing : m_tiledGraphicsProviders)
        if (existing == provider)
            return;
    m_tiledGraphicsProviders.push_back(provider);
    InvalidateScene();
}

void Viewport::DropTiledGraphicsProvider(TiledGraphicsProvider* provider)
{
    // <- itwinjs-core Viewport.dropTiledGraphicsProvider (Viewport.ts:1738-1740).
    auto it = std::find(m_tiledGraphicsProviders.begin(), m_tiledGraphicsProviders.end(), provider);
    if (it != m_tiledGraphicsProviders.end()) {
        m_tiledGraphicsProviders.erase(it);
        InvalidateScene();
    }
}

bool Viewport::HasTiledGraphicsProvider(TiledGraphicsProvider* provider) const
{
    // <- itwinjs-core Viewport.hasTiledGraphicsProvider (Viewport.ts:1743-1745).
    return std::find(m_tiledGraphicsProviders.begin(), m_tiledGraphicsProviders.end(), provider)
        != m_tiledGraphicsProviders.end();
}

void Viewport::RemoveTileTree(dqRender::TileTree* tree)
{
    auto it = std::find(m_tileTrees.begin(), m_tileTrees.end(), tree);
    if (it != m_tileTrees.end()) {
        m_tileTrees.erase(it);
        // Drop the scaffold reference wrapping this tree.
        for (auto rit = m_scaffoldRefs.begin(); rit != m_scaffoldRefs.end(); ++rit) {
            if ((*rit)->getTreeOwner().getTileTree() == tree) {
                m_scaffoldRefs.erase(rit);
                break;
            }
        }
        InvalidateScene();
    }
}

void Viewport::discloseTileTrees(std::vector<dqRender::TileTree*>& trees)
{
    // Ported from: itwinjs-core Viewport.discloseTileTrees()
    // Append all tile trees this viewport is currently displaying
    for (auto* tree : m_tileTrees) {
        if (tree) {
            trees.push_back(tree);
        }
    }
}

size_t Viewport::numRequestedTiles() const
{
    // Ported from: Viewport.ts:1836 numRequestedTiles
    // (tileAdmin.getNumRequestsForUser(this)).
    return dqRender::TileAdmin::instance().getNumRequestsForUser(*this);
}

size_t Viewport::numSelectedTiles() const
{
    // Ported from: Viewport.ts:1844 numSelectedTiles
    // (tiles.selected.size + tiles.external.selected).
    dqRender::TileAdmin::SelectedAndReadyTiles tiles;
    if (!dqRender::TileAdmin::instance().getTilesForUser(*this, tiles))
        return 0;
    return (tiles.selected ? tiles.selected->size() : 0) + tiles.external.selected;
}

size_t Viewport::numReadyTiles() const
{
    // Ported from: Viewport.ts:1855 numReadyTiles
    // (tiles.ready.size + tiles.external.ready).
    dqRender::TileAdmin::SelectedAndReadyTiles tiles;
    if (!dqRender::TileAdmin::instance().getTilesForUser(*this, tiles))
        return 0;
    return (tiles.ready ? tiles.ready->size() : 0) + tiles.external.ready;
}

// ---------------------------------------------------------------------------
// RenderFrame — 17-step rendering pipeline
// Ported from: itwinjs-core Viewport.ts:2546 renderFrame()
// ---------------------------------------------------------------------------
void Viewport::RenderFrame()
{
    // Crash-on-close guard: once Shutdown() has released the pipeline (or is
    // tearing it down), never touch the GL surface / native window.
    if (m_isShuttingDown) return;
    if (!m_pipeline || !m_pipeline->isInitialized()) return;
    if (!m_initialized) return;

    // RenderFrame 进行中标记——resizeEvent 经 wglMakeCurrent 的消息泵重入时
    // 延至帧尾（见 resizeEvent 的 m_inRenderFrame 守卫）。
    struct RenderFrameGuard {
        Viewport& vp;
        RenderFrameGuard(Viewport& v) : vp(v) { vp.m_inRenderFrame = true; }
        ~RenderFrameGuard() {
            vp.m_inRenderFrame = false;
            // 帧边界：清 ToolAdmin 的 deferred ViewTool 删除队列（TD-11——
            // 自退出工具 ViewUndoTool::onPostInstall→exitTool 在 run() 调用链
            // 内，同步 delete this 是 UB；参考 async run() 的微任务恢复语义
            // （ViewTool.ts:100-112）在此帧边界等价执行）。
            Application::Get().GetToolAdmin().flushDeferredViewToolDeletes();
            if (vp.m_pendingResize) {
                vp.m_pendingResize = false;
                // 帧尾补处理 resize（delete 旧 target 在 RenderFrame 外，安全）。
                vp.resizeEvent(nullptr);
            }
        }
    } guard(*this);

    // Acquire the Swapchain image (binds GL context to window surface).
    // This must happen before any rendering commands.
    auto* swapchain = m_pipeline->getSwapchain();
    if (!swapchain || !swapchain->isValid()) return;
    swapchain->acquire();

    // Step 1: Capture and clear change flags
    ChangeFlags changeFlags = m_changeFlags;
    if (changeFlags.HasChanges())
        m_changeFlags.clear();

    // Step 2: Animation
    // Ported from: itwinjs-core Viewport.ts step 2 (lines 2537-2539)
    bool animatorRan = false;
    if (m_animator) {
        animatorRan = true;
        if (m_animator->animate()) {
            // Animation complete — remove animator
            m_animator.reset();
        }
    } else if (m_animatorRef) {
        // 非拥有槽（ViewHandleArray 持有的惯性/滚动句柄，ViewTool.ts:1072 的
        // vp.setAnimator(this) 等价）。Viewport.ts:2573-2575：didFinish → 清除。
        animatorRan = true;
        if (m_animatorRef->animate()) {
            m_animatorRef = nullptr;
        }
    }

    // Step 3: Redraw check
    // ← Viewport.ts:2601 `let isRedrawNeeded = this._redrawPending || this._doContinuousRendering`
    // —— continuousRendering 时每一帧都是重绘帧（FpsTracker/GPU profiler 依赖帧
    // 持续产出；此前丢 `|| m_doContinuousRendering` 导致只首帧绘制、计时结果永
    // 不落地——2026-09-21 GPU profiler 取证实锤）。
    bool isRedrawNeeded = m_redrawPending || m_doContinuousRendering;
    m_redrawPending = false;
    // 动画器激活时强制重绘：animate() 每帧改 view（FrustumAnimator.onUpdate 的
    // SetExtents/SetRotation/setCenter），参考 renderFrame 的 isRedrawNeeded 依赖
    // _redrawPending（setAnimator 只设一次），后续帧靠 SetupFromView 的连锁失效
    // （invalidateController → scene/plan 重建）置位。DanQing 的 SetupFromView 故意
    // 不失效（RenderFrame 控制器同步路径依赖，.h 注释），动画器的 onUpdate 调用
    // 不失效 → m_controllerValid 恒 true → Step 10/11 不置位 → 不画 = 无渐进感。
    // 与参考 1:1：动画器每帧改 view 必须每帧重绘。
    //
    // 2026-09-16（视图旋转不进渲染链 saga）：仅置 isRedrawNeeded 不够——那只重画
    // **旧的 RenderPlan/scene**（plan.frustum 仍是加载初始盒），标准视图工具/相机
    // 旋转后内容不随 rotation 更新。参考侧 plan 每帧经 createRenderPlanFromViewport
    // 从 viewingSpace 重建（m_renderPlanValid 门是 DanQing 优化偏差）；最小等效 =
    // 动画器跑过的帧强制 controller/plan/scene 重建（Step 5/10/11 会消费这些门）。
    if (animatorRan) {
        isRedrawNeeded = true;
        InvalidateController();  // 级联 m_renderPlanValid/m_sceneValid=false
    }

    // Step 4: View rect resize detection
    // (Qt handles resize via resizeGL(), which calls InvalidateController)

    // Step 5: Controller synchronization
    if (!m_controllerValid) {
        SetupFromView();
        m_controllerValid = true;
    }

    // Step 6: Selection set — push the iModel's hilite set to the target and
    // request a redraw. The per-batch feature-override LUTs pick the new set up
    // lazily at draw time (PushBatch → Batch::updateHilite, the reference's
    // FeatureOverrides.update).
    // Ported from: itwinjs-core Viewport.ts:2613-2617
    //   if (this._selectionSetDirty) {
    //     target.setHiliteSet(view.iModel.hilited);
    //     this._selectionSetDirty = false;
    //     isRedrawNeeded = true;
    //   }
    if (m_selectionSetDirty) {
        m_selectionSetDirty = false;
        if (m_renderTarget) {
            // 注意不可用三元把临时 QSet 绑到 const 引用（分号即悬垂）——早帧/
            // 拆除帧 GetIModel()==null 时走临时分支 → UB/AV（拾取 saga 套内
            // 崩塌的根因）。
            std::vector<uint32_t> ids;
            if (auto* imodel = GetIModel()) {
                auto const& elements = imodel->GetHiliteSet().GetElements();
                ids.assign(elements.begin(), elements.end());
            }
            m_renderTarget->setHiliteSet(ids.data(), ids.size());
        }
        isRedrawNeeded = true;
    }

    // Step 7: Analysis fraction
    // Ported from: itwinjs-core Viewport.ts step 7 (lines 2585-2588)
    if (!m_analysisFractionValid) {
        m_analysisFractionValid = true;
        isRedrawNeeded = true;
        // The analysis fraction is already read in ValidateRenderPlan() (line 977)
        // and stored in the RenderPlan. This step just ensures the render plan
        // is re-validated when the fraction changes.
        InvalidateRenderPlan();
    }

    // Step 8: Time point / schedule script
    // Ported from: itwinjs-core Viewport.ts step 8 (lines 2590-2603)
    if (!m_timePointValid) {
        m_timePointValid = true;
        isRedrawNeeded = true;
        // The time point is read in ValidateRenderPlan() and stored in the RenderPlan.
        // Schedule script evaluation (AnimationBranchStates) is deferred until the
        // full schedule script API is exposed through DisplayStyle public interface.
    }

    // Step 9: Feature symbology overrides
    // Ported from: itwinjs-core Viewport.ts step 9
    if (m_featureOverridesDirty) {
        m_featureOverridesDirty = false;
        m_featureOverrides.clearModelSubCategoryOverrides();
        m_featureOverrides.clear();

        // Collect feature overrides from all providers
        // ← itwinjs-core: addFeatureOverrides()
        auto& viewMgr = Application::Get().GetViewManager();
        for (auto* provider : viewMgr.GetFeatureOverrideProviders()) {
            if (provider) {
                provider->addFeatureOverrides(m_featureOverrides, this);
            }
        }

        // Apply per-model category visibility overrides.
        // Ported from: itwinjs-core Viewport.ts addModelSubCategoryVisibilityOverrides (line 1519)
        if (!m_perModelCategoryVisibility.isEmpty()) {
            addModelSubCategoryVisibilityOverrides(m_featureOverrides);
        }

        isRedrawNeeded = true;
    }

    // Step 10: Scene creation
    if (!m_sceneValid && !m_freezeScene) {
        CreateScene();
        if (m_renderTarget) {
            m_renderTarget->changeScene(m_scene);
        }
        isRedrawNeeded = true;
    }
    m_sceneValid = true;

    // Step 11: Render plan validation
    if (!m_renderPlanValid) {
        ValidateRenderPlan();
        isRedrawNeeded = true;
    }
    // Step 12: Decorations collection
    // Ported from: itwinjs-core Viewport.ts line 2637-2645
    if (!m_decorationsValid) {
        CollectDecorations();
        if (m_renderTarget) {
            m_renderTarget->changeDecorations(m_decorations);
        }
        m_decorationsValid = true;  // ← itwinjs-core sets this at line 2642
        isRedrawNeeded = true;
        // 2D canvas 装饰无独立重绘调度：isRedrawNeeded=true ⇒ Step 15 绘制帧，
        // drawCanvasDecorations 在该帧内重栅格化（Target.ts:552 语义——每绘制帧
        // 无条件重绘 2D 层；GL 帧整体重绘，无参考 _2dCanvas.needsClear 的对应物）。
    }

    // Step 13: Flash processing
    // Ported from: itwinjs-core Viewport.ts:2682-2687（逐行对照）:
    //   if (this.processFlash()) {
    //     target.setFlashed(flashedId ?? Id64.invalid, this._flashIntensity);
    //     isRedrawNeeded = true;
    //     requestNextAnimation = undefined !== this.flashedId;
    //   }
    // hover locate（EQUIVALENCE——见 Viewport.h m_hoverDirty 注释）：参考在
    // 事件层（AccuSnap.onMotion）locate，Chromium 每帧合并；DanQing 在 renderFrame
    // 内对最新 hover 点做一次 PickAtPoint（SelectionTool 同款 doLocate 等价物）。
    if (m_hoverDirty) {
        m_hoverDirty = false;
        uint32_t const hit = PickAtPoint(m_hoverCssX, m_hoverCssY);
        SetFlashedId(hit);
        // Snap cross（AccuSnap.onMotion → getSnap → cross.activate 的帧合并等价，
        // EQUIVALENCE 登记于 AccuSnap.cpp activateCrossAt）：命中元素时在光标处
        // 显示 snap 十字 sprite（AccuSnap.ts:484-490），未命中即清除（:355 miss →
        // clear）。
        auto& accuSnap = Application::Get().GetAccuSnap();
        if (hit != 0 && accuSnap.isSnapEnabled())
            accuSnap.activateCrossAt(*this, m_hoverCssX, m_hoverCssY);
        else
            accuSnap.clearCross();
    }
    if (processFlash()) {
        isRedrawNeeded = true;
        if (m_renderTarget) {
            m_renderTarget->setFlashed(m_flashedId, m_flashIntensity);
        }
        if (m_flashedId > 0) {
            RequestRedraw();  // 强度爬升需要连续帧（参考 requestNextAnimation）
        }
    }

    // Step 14: Pre-render hook
    // Ported from: itwinjs-core Viewport.ts step 14 (lines 2654-2658)
    if (m_renderTarget) {
        m_renderTarget->onBeforeRender.Raise();
    }

    // Set the viewport transform on the render target.
    // ← itwinjs-core: the viewport's viewing transform reaches the target via
    // changeRenderPlan(plan) → changeFrustum (RenderPlan.frustum) — the branch
    // stack's MV/MVP pair is derived there (see OpenGLRenderTarget::
    // setViewportTransform). The arguments below are legacy inputs, no longer
    // consumed for the projection (worldToNdc as projection inverted depth).
    if (m_renderTarget && m_view) {
        auto const& mv = m_viewingSpace.GetViewMatrix();
        auto const& mvp = m_viewingSpace.GetWorldToNdcMatrix();
        m_renderTarget->setViewportTransform(mv.data(), mvp.data());
    }

    // Step 15: Actual draw
    // ← itwinjs-core: target.drawFrame(elapsedMs)
    //
    // The compositor renders the full scene through multi-pass pipeline:
    //   clearOpaque → background → skybox → backgroundMap →
    //   opaque (with pick data) → translucent (OIT) → hilite → composite
    //
    // The grid is part of the scene graph (added in CreateScene()),
    // rendered as OpaquePlanar pass alongside other geometry.
    if (isRedrawNeeded && m_renderTarget) {
        if (std::getenv("DANQING_DP_TRACE"))
            fprintf(stderr, "[DP] drawFrame (decorations canvDecs=%zu)\n",
                    GetDecorations().canvasDecorations.size());
        m_renderTarget->drawFrame();

        // 2D canvas decorations — rasterized INSIDE the GL frame, after the scene
        // draw and before present. The reference's Target.drawFrame calls
        // drawOverlayDecorations() unconditionally after paintScene
        // (internal/render/webgl/Target.ts:546-554, the call at :552), so the
        // overlay re-rasterizes on every drawn frame; DanQing mirrors that here
        // (RenderTarget::drawCanvasDecorations — GL backend, APPROVED DEVIATION
        // per spec §2.4: reference rasterizes to an HTML 2D canvas).
        // An empty list draws nothing — GL frames are redrawn wholesale each
        // drawn frame (the scene pass clears), so no explicit clear is needed.
        m_renderTarget->drawCanvasDecorations(GetDecorations().canvasDecorations);

        // Present the Swapchain (swap buffers to display on screen).
        // This must happen after all rendering commands are complete.
        if (swapchain) {
            swapchain->present();
            // TEMP-DIAG：present 计数（"屏幕黑块但 FBO 正确"复现期是否持续 present）。
            // DANQING_OIT_DUMP=1 时打印尺寸轨迹（常态零开销只累加）。
            ++m_dbgPresentCount;
            if (std::getenv("DANQING_OIT_DUMP"))
                fprintf(stderr, "[PRES] #%d %ux%u\n", m_dbgPresentCount,
                        static_cast<unsigned>(width() * devicePixelRatioF()),
                        static_cast<unsigned>(height() * devicePixelRatioF()));
        }
    }

    // Step 16: Post-frame events
    // Ported from: itwinjs-core Viewport.ts renderFrame() event dispatch (lines 2669-2698)
    //
    // itwinjs-core dispatches events in a specific nested order:
    //   1. ViewportChanged (always if any changes)
    //   2. DisplayStyleChanged
    //   3. ViewedModelsChanged
    //   4. FeatureOverridesChanged (if any overrides dirty)
    //      5. AlwaysDrawnChanged (nested inside overrides)
    //      6. NeverDrawnChanged (nested inside overrides)
    //      7. ViewedCategoriesChanged (nested inside overrides)
    //      8. ViewedCategoriesPerModelChanged (nested inside overrides)
    //      9. FeatureOverrideProviderChanged (nested inside overrides)
    if (changeFlags.HasChanges()) {
        emit ViewportChanged(changeFlags);
        if (changeFlags.DisplayStyle())
            emit DisplayStyleChanged();
        if (changeFlags.ViewedModels())
            emit ViewedModelsChanged();
        if (changeFlags.AreFeatureOverridesDirty()) {
            emit FeatureOverridesChanged();
            if (changeFlags.alwaysDrawn())
                emit AlwaysDrawnChanged();
            if (changeFlags.neverDrawn())
                emit NeverDrawnChanged();
            if (changeFlags.ViewedCategories())
                emit ViewedCategoriesChanged();
            if (changeFlags.ViewedCategoriesPerModel())
                emit ViewedCategoriesPerModelChanged();
            if (changeFlags.FeatureOverrideProvider())
                emit FeatureOverrideProviderChanged();
        }
    }

    // Step 17: Continuous rendering
    // Ported from: itwinjs-core Viewport.ts step 17 (line 2701)
    // Request next animation if redraw needed, flash active, or animator active.
    // continuousRendering (Viewport.ts:1455-1461): every tick of the render
    // loop renders a new frame (FpsTracker turns this on while tracking).
    if (isRedrawNeeded || m_flashedId != 0 || m_animator || m_animatorRef ||
        m_doContinuousRendering) {
        Application::Get().RequestNextAnimation();
    }
}

void Viewport::setContinuousRendering(bool contRend)
{
    // Ported from: Viewport.ts:1456-1461 — turning on requests the next frame
    // immediately (the renderFrame tail keeps the loop alive while on).
    if (m_doContinuousRendering == contRend)
        return;
    m_doContinuousRendering = contRend;
    if (contRend)
        RequestRedraw();
}

// ---------------------------------------------------------------------------
// SetupFromView — synchronize viewport state with ViewState
// Ported from: itwinjs-core Viewport.ts setupFromView()
// ---------------------------------------------------------------------------
void Viewport::SetupFromView()
{
    SetupFromView(false);
}

void Viewport::SetupFromView(bool skipAspectFix)
{
    if (!m_view) return;

    // Rebuild the ViewingSpace transform chain from current ViewState.
    // Ported from: itwinjs-core Viewport.doSetupFromView() (line 2001-2022)
    auto* view3d = m_view->AsViewState3d();
    if (view3d) {
        // Fix aspect ratio to match viewport dimensions.
        // Ported from: itwinjs-core Viewport.ts line 2006
        // if (!this.isAspectRatioLocked)
        //   view.fixAspectRatio(this.viewRect.aspect);
        // skipAspectFix=true 时跳过 FixAspectRatio——滚轮缩放正交分支
        // （Viewport.ts:2211-2225 vp.zoom）不走 FixAspectRatio 的 1:1 对齐。
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        if (w > 0.0f && h > 0.0f && !skipAspectFix) {
            view3d->FixAspectRatio(w / h);
        }

        ViewRect rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = width();
        rect.bottom = height();
        m_viewingSpace.update(*view3d, rect);
    }
}

// Ported from: itwinjs-core Viewport.setupFromView(pose?) (Viewport.ts:2062-2066)：
// pose 非空 → view.applyPose(pose) 后 doSetupFromView。
void Viewport::SetupFromView(ViewPose const* pose)
{
    if (pose) {
        auto* view3d = m_view ? m_view->AsViewState3d() : nullptr;
        if (view3d)
            view3d->applyPose(*pose);          // :2064
    }
    SetupFromView();                           // :2065 doSetupFromView
}

// ---------------------------------------------------------------------------
// CreateScene — build the scene graph
// Ported from: itwinjs-core Viewport.ts createScene()
// ---------------------------------------------------------------------------
void Viewport::CreateScene()
{
    m_scene.clear();

    // 注：网格不在此处构建。参考机制（ViewState.ts:641-645/678-680 → GridDecorator
    // → ViewContext.drawStandardGrid）把网格作为 **WorldDecoration** 在
    // CollectDecorations（管线步骤 12）里经 ViewState::Decorate → GridDecorator
    // → DecorateContext::DrawStandardGrid 产出；此处（步骤 10 createScene）只装
    // 场景图形（参考 createScene 同样不含网格）。

    // Tile trees via the faithful reference chain.
    // <- itwinjs-core Viewport.renderFrame (Viewport.ts:2647-2658):
    //    createSceneContext() -> createScene(context) -> view.getModelTreeRefs()
    //    -> ref.addToScene -> context.requestMissingTiles() -> changeScene.
    if (m_view) {
        // Reset this user's tile sets before re-selecting.
        // <- Viewport.ts:2650-2651 clearTilesForUser/clearUsageForUser.
        dqRender::TileAdmin::instance().clearTilesForUser(*this);

        // Viewport-level TileDrawArgs template (eye/pixelSize/frustumPlanes -
        // the context part of the reference TileDrawArgs ctor,
        // TileTreeReference.ts:156-176).
        dqRender::TileDrawArgs viewportArgs;

        // Camera position: the viewing space's eye point (camera on) - the
        // reference TileDrawArgs eye comes from the viewport frustum
        // (TileDrawArgs.ts:34-79). Orthographic views have no meaningful eye;
        // the frustum origin stands in (ViewingSpace keeps {0,0,0} there).
        // TODO(perspective SSE): the orthographic pixelSize below is uniform;
        // camera views should use computePixelSizeInMetersAtClosestPoint
        // (TileDrawArgs.ts:190-206) once a camera-on test exercises it.
        auto const eye = m_viewingSpace.getEyePoint();
        viewportArgs.eyePos[0] = static_cast<float>(eye.x);
        viewportArgs.eyePos[1] = static_cast<float>(eye.y);
        viewportArgs.eyePos[2] = static_cast<float>(eye.z);

        // Pixel size = world units per pixel (frustum height / viewport pixel
        // height) - the reference getPixelSize (TileDrawArgs.ts:138).
        auto const& extents = m_view->GetExtents();
        float const viewHeightPx = static_cast<float>(height());
        if (extents.y > 0.0 && viewHeightPx > 0.0f) {
            viewportArgs.pixelSizeRatio = static_cast<float>(extents.y) / viewHeightPx;
        }

        // World-space frustum planes for tile culling (TileDrawArgs.ts:93-96).
        viewportArgs.frustumPlanes = dqCommon::FrustumPlanes::fromFrustum(getFrustum(/*world=*/true));

        // Perspective pixel-size inputs (camera on) —
        // computePixelSizeInMetersAtClosestPoint inputs (TileDrawArgs.ts:190-206).
        if (auto* view3dCam = m_view->AsViewState3d(); view3dCam && view3dCam->IsCameraOn()) {
            viewportArgs.cameraOn = true;
            auto const& cam = view3dCam->GetCamera();
            viewportArgs.cameraEye[0] = static_cast<float>(cam.eye.x);
            viewportArgs.cameraEye[1] = static_cast<float>(cam.eye.y);
            viewportArgs.cameraEye[2] = static_cast<float>(cam.eye.z);
            viewportArgs.perspectiveScale =
                static_cast<float>(2.0 * std::tan(cam.lensRadians * 0.5) / viewHeightPx);
        }

        SceneContext context(*this, std::move(viewportArgs));

        // 1) The view's spatial tile tree references (factory seam -
        //    SpatialTileTreeReferences.create, overridable frontend-tiles
        //    style; the default set is empty until per-model production
        //    lands).
        if (auto* spatial3d = m_view->AsViewState3d()) if (auto* spatial = spatial3d->AsSpatialViewState()) {
            spatial->ForEachModelTreeRef([&context](dqApp::TileTreeReference& ref) {
                ref.addToScene(context);
            });
        }

        // 2) Application-injected providers.
        // <- Viewport.ts:2660-2662 (TiledGraphicsProvider.addToScene).
        for (auto* provider : m_tiledGraphicsProviders) {
            if (provider)
                TiledGraphicsProviders::addToScene(*provider, context);
        }

        // 3) The AddTileTree scaffold bridge (caller-owned trees wrapped as
        //    references - same chain as everything else).
        for (auto& ref : m_scaffoldRefs)
            ref->addToScene(context);

        // Report the frame's selection (batched equivalence of the per-tree
        // addTilesForUser + frame-tail requestMissingTiles,
        // TileTree.ts:136-142 + Viewport.ts:2656; registered 2026-09-21).
        dqRender::TileAdmin::instance().addTilesForUser(
            *this, /*selected=*/context.selectedTiles(),
            /*ready=*/{}, /*touched=*/{});

        // Collected graphics -> scene foreground.
        for (auto* graphic : context.graphics()) {
            if (graphic)
                m_scene.foreground.push_back(graphic);
        }

        if (std::getenv("DANQING_TILE_TRACE"))
            std::fprintf(stderr,
                         "[TILE] CreateScene trees=%zu selected=%zu missing=%zu graphics=%zu\n",
                         m_tileTrees.size(), context.selectedTiles().size(),
                         context.missingTiles().size(), context.graphics().size());
    }
}

// ---------------------------------------------------------------------------
// ValidateRenderPlan — validate the render plan
// Ported from: itwinjs-core Viewport.ts validateRenderPlan()
// ---------------------------------------------------------------------------
void Viewport::ValidateRenderPlan()
{
    // Build render plan from current ViewState
    // ← itwinjs-core: createRenderPlanFromViewport(this)
    dqRender::RenderPlan newPlan;

    if (m_view) {
        auto const& style = m_view->GetDisplayStyle();
        newPlan.viewFlags = style.getViewFlags();
        newPlan.backgroundColor = style.getBackgroundColor();

        // ← itwinjs-core RenderPlan.ts:119: RenderPlan.lights = vp.lightSettings
        // Typed dqCommon::LightSettings consumed by TargetUniforms.updateRenderPlan.
        newPlan.lights = style.GetLightSettings();

        newPlan.analysisFraction = style.getAnalysisFraction();
        newPlan.timePoint = style.getTimePoint();
        newPlan.monochromeMode = style.getViewFlags().monochrome();
        newPlan.monochromeColor = style.getMonochromeColor();
        newPlan.whiteOnWhiteReversal = style.getWhiteOnWhiteReversal();

        // View volume — feeds changeRenderPlan → FrustumUniforms.changeFrustum
        // (the reference's ONLY u_proj/u_mv source; lookIn + ortho(0,depth)).
        // Ported from: itwinjs-core createRenderPlanFromViewport
        // (RenderPlan.ts:103 is3d, :110 frustum, :111 fraction):
        //   const is3d = view.is3d();
        //   const frustum = vp.viewingSpace.getFrustum();      // World coords
        //   const fraction = vp.viewingSpace.frustFraction;
        // is3d(): 参考 ViewState 基类虚函数（ViewState.ts:1505 在 3d 子类）；
        // DanQing 的 is3d() 只在 ViewState3d 上 —— 经 AsViewState3d() 判别。
        newPlan.is3d = m_view->AsViewState3d() != nullptr;
        m_viewingSpace.getFrustum(newPlan.frustum, dqCommon::CoordSystem::World);
        newPlan.fraction = m_viewingSpace.getFrustFraction();
    }

    // Attach feature overrides
    newPlan.featureOverrides = &m_featureOverrides;

    // Check if plan changed
    if (newPlan != m_currentRenderPlan) {
        m_currentRenderPlan = newPlan;

        // Pass to render target if available
        // ← itwinjs-core: target.changeRenderPlan(plan)
        if (m_renderTarget) {
            m_renderTarget->changeRenderPlan(m_currentRenderPlan);
        }
    }

    // Viewport.ts:3602 — this._lastPose = this.view.savePose()：逐帧保存渲染姿态，
    // 作为下一次 frustum-change 动画的起点（animateFrustumChange :3520-3521）。
    if (auto* view3d = m_view ? m_view->AsViewState3d() : nullptr)
        m_lastPose = view3d->savePose();

    m_renderPlanValid = true;
}

// ---------------------------------------------------------------------------
// CollectDecorations — collect decorations from all decorators
// Ported from: itwinjs-core ScreenViewport.addDecorations() (Viewport.ts:3511)
// ---------------------------------------------------------------------------
void Viewport::CollectDecorations()
{
    if (getenv("DANQING_DP_TRACE"))
        fprintf(stderr, "[DP] CollectDecorations run\n");
    m_decorations.clear();

    // Create DecorateContext for collecting decorations（ViewContext.ts:182-186：
    // ctor(vp, decorations, cache)）
    DecorateContext context(*this, m_decorations, m_decorationCache);

    // 1. View's own decorations (← context.addFromDecorator(this.view)，
    //    Viewport.ts:3565)。ViewState::Decorate → drawGrid → GridDecorator
    //    （ViewState.ts:641-645/678-680）——网格经此作为 WorldDecoration 产出。
    if (m_view) {
        context.AddFromDecorator(m_view.Get());
    }

    // 2. Tile tree references (← for (ref of getTileTreeRefs()))
    // Phase 2: tile trees contribute decorations

    // 3. Registered decorators (← for (decorator of IModelApp.viewManager.decorators))
    auto& viewMgr = Application::Get().GetViewManager();
    for (auto* decorator : viewMgr.GetDecorators()) {
        if (decorator) {
            context.AddFromDecorator(decorator);
        }
    }

    // 4. Sky box (← itwinjs-core EnvironmentDecorations.decorate: when
    //    env.displaySky, renderSystem.createSkyBox(sky.params) → context.setSkyBox).
    //    Authored: itwinjs attaches EnvironmentDecorations via view.attachToViewport;
    //    DanQing inlines the sky decoration here until that decorator is ported.
    if (m_view && m_renderTarget) {
        auto const& env = m_view->GetDisplayStyle().getEnvironment();
        if (env.displaySky) {
            // Pass the full SkyGradient (4 colors + twoColor + exponents) so the
            // SkySphereViewportQuadGeometry ctor branches 2- vs 4-color exactly like
            // the reference (CachedGeometry.ts:686-716). Earlier only zenith/nadir
            // were packed into topColor/bottomColor, forcing the 2-color path always
            // (audit D9.1).
            dqRender::RenderSkyGradientParams params;
            params.gradient = env.sky.gradient;
            // EnvironmentDecorations.ts:278 — zOffset: this._view.iModel.globalOrigin.z.
            // (0 for the DTA blank connection — its globalOrigin defaults to {0,0,0}.)
            auto* iModel = m_view->GetIModel();
            params.zOffset = iModel ? static_cast<float>(iModel->GetGlobalOrigin().z) : 0.0f;
            // SkySphere.ts:145-180 - backgroundMapOn replaces the map-facing
            // (ground/nadir) sky stops with skyColor (light blue), so the top-down
            // blank-connection sky reads flat light-blue instead of nadir green.
            params.backgroundMapOn = m_view->getViewFlags().backgroundMap();
            // Create-once: itwinjs creates the sky sphere once (System.createSkyBox)
            // and re-derives a_worldPos/u_worldEye per frame (initWorldPos). DanQing
            // previously called createSkyBox EVERY frame, leaking a SkySphere graphic
            // (VBOs + VAO + primitive) each frame → GL resource exhaustion →
            // glDrawElements crash on the null buffer. Cache in m_skyGraphic (mirrors
            // the grid's m_gridGraphic lifecycle) and only updateSkySphere per frame.
            //
            // 参数指纹失效：参考 EnvironmentDecorations.decorate 每帧以当下环境
            // createSkyBox（EnvironmentDecorations.ts:90-95）；DanQing 的缓存图形把
            // 颜色烘在 ctor（CachedGeometry.ts:686-716），环境变更（如 Decoration
            // Geometry Example 的 twoColor sky）必须销毁重建，否则首视图的默认
            // 渐变残留。
            bool const skyParamsChanged =
                !m_skyGradientCache.has_value() ||
                !m_skyGradientCache->equals(params.gradient) ||
                m_skyZOffsetCache != params.zOffset ||
                m_skyBackgroundMapOnCache != params.backgroundMapOn;
            if (skyParamsChanged && m_skyGraphic) {
                delete m_skyGraphic;
                m_skyGraphic = nullptr;
            }
            if (!m_skyGraphic) {
                m_skyGraphic = m_renderTarget->renderSystem().createSkyBox(&params);
                m_skyGradientCache = params.gradient;
                m_skyZOffsetCache = params.zOffset;
                m_skyBackgroundMapOnCache = params.backgroundMapOn;
            }
            if (m_skyGraphic) {
                // RenderPlan.ts:106/132-141/108-109 — 计划的 globe-mode 状态
                // （isGlobeMode3D = GlobeMode.Ellipsoid === view.globeMode；
                //   upVector = view.getUpVector(视锥中心)；frustum type + planFraction）。
                dqRender::SkySphereGlobeParams globeParams;
                dqCommon::Frustum const worldFrustum = getFrustum(true);
                if (auto* view3d = m_view->AsViewState3d()) {
                    // settings.backgroundMap.globeMode 未接入样式图（同
                    // BackgroundMapGeometry.cpp:202 登记）——参考默认即 Ellipsoid。
                    globeParams.isGlobeMode3D = true;
                    // RenderPlan.ts:133-136 — 视锥中心 = 中深 LBRear→LBFront 与
                    // RTRear→RTFront 的中点链。
                    auto const lbMid = dqGeom::Point3d::FromInterpolate(
                        worldFrustum.getCorner(0), 0.5, worldFrustum.getCorner(4));
                    auto const rtMid = dqGeom::Point3d::FromInterpolate(
                        worldFrustum.getCorner(3), 0.5, worldFrustum.getCorner(7));
                    auto const frustumCenter = dqGeom::Point3d::FromInterpolate(lbMid, 0.5, rtMid);
                    globeParams.upVector = view3d->getUpVector(frustumCenter);
                    globeParams.perspective = view3d->IsCameraOn();
                    globeParams.planFraction = m_viewingSpace.getFrustFraction();
                }
                m_renderTarget->renderSystem().updateSkySphere(m_skyGraphic, worldFrustum, globeParams);
                context.SetSkyBox(m_skyGraphic);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// PickAtPoint — read feature ID at pixel coordinate from pick buffer
// ---------------------------------------------------------------------------
#ifdef DANQING_TESTING
// DIAG(grid-app): 应用路径合成帧回读。
bool Viewport::ReadFrameForTest(std::vector<uint8_t>& rgbaOut)
{
    uint32_t w = 0, h = 0;
    return ReadFrameForTest(rgbaOut, w, h);
}

bool Viewport::ReadFrameForTest(std::vector<uint8_t>& rgbaOut, uint32_t& wOut, uint32_t& hOut)
{
    if (!m_renderTarget) return false;
    // 读整个渲染目标（设备像素 = CSS 尺寸 × devicePixelRatio，itwinjs
    // readPixels 同语义）——高 DPR 下 FBO 大于 QWidget 的 width()/height()，
    // 只读 CSS 尺寸会只拿到左下象限（ACS 中心等 FBO 中心内容被裁掉）。
    auto const rect = m_renderTarget->viewRect();
    wOut = rect.width();
    hOut = rect.height();
    rgbaOut.resize(static_cast<size_t>(rect.width()) * rect.height() * 4);
    return m_renderTarget->readPixels(0, 0, rect.width(), rect.height(), rgbaOut);
}
#endif

uint32_t Viewport::PickAtPoint(int32_t x, int32_t y)
{
#ifdef DANQING_TESTING
    // Test-only override: short-circuit the GL path so SelectionTool's pick
    // chain can be driven in headless GoogleTest (no live render target).
    // Authored: test affordance — no reference equivalent (itwinjs tests use
    //           a live WebGL context). See SetPickResultForTest. Guarded by
    //           DANQING_TESTING so production builds have no seam (§8.4).
    if (m_pickResultOverride != 0u)
        return m_pickResultOverride;
#endif  // DANQING_TESTING

    // Real pick path: render the pick view on demand (simplified view flags,
    // translucent-as-opaque, non-pickable decorations excluded) and read back
    // the element id at the pixel. Previously this read the RGBA COLOR buffer
    // and memcpy'd its first 4 bytes as a feature id — garbage (the pick
    // featureId lives in the R32UI pick attachment, written by the Pick shader
    // variant).
    // Ported from: itwinjs-core Viewport.readPixels → target.readPixels
    //               (Viewport.ts:2778-2785 → Target.ts:768-825)
    if (m_renderTarget) {
        // CSS (Qt top-left origin, event.viewPoint 语义) → device pixels →
        // GL bottom-up origin（参考 cssPixelsToDevicePixels + readFrameBuffer
        // 的 bottom = fullHeight - rect.bottom，SceneCompositor.ts:1641-1642）。
        auto const rect = m_renderTarget->viewRect();
        double const cssW = width() > 0 ? static_cast<double>(width()) : 1.0;
        double const dpr = rect.width() / cssW;
        int32_t const devX = static_cast<int32_t>(std::lround(x * dpr));
        int32_t const devY = static_cast<int32_t>(std::lround(y * dpr));
        int32_t const glY = static_cast<int32_t>(rect.height()) - 1 - devY;
        if (devX < 0 || devY < 0 || devX >= static_cast<int32_t>(rect.width())
            || devY >= static_cast<int32_t>(rect.height()))
            return 0;
        uint32_t elementId = 0;
        m_renderTarget->readPickData(devX, glY, 1, 1, &elementId, 1);
        return elementId;
    }

    // Legacy path: use RenderPipeline::pickQuery()
    if (!m_pipeline || !m_pipeline->isInitialized()) return 0;
    return m_pipeline->pickQuery(x, y);
}

// ---------------------------------------------------------------------------
// pickDepthPoint — 光标圆域内的深度锚定（featureId + depthAndOrder 双附件回读）
// Ported from: itwinjs-core Viewport.pickDepthPoint (Viewport.ts:3435-3503)。
//
// DanQing 通道适配（EQUIVALENCE）：参考经 ElementPicker.doPick 圆域拾取 + 命中
// 优先级/距离排序取首（HitList）；DanQing 读 (2r+1)² 矩形，取"离圆心最近的非零
// featureId 像素"（同优先序的顶点结果）。发散=多优先级/多 iModel/瓦片来源分辨
// 未实现（无场景）；验证法=Deco 盒面命中点落回盒面、天空落 ACS 面（z=0）。
// ---------------------------------------------------------------------------
Viewport::DepthPointResult Viewport::pickDepthPoint(dqGeom::Point3d const& pickPoint,
                                                    double radiusPixels)
{
    DepthPointResult result;

    // :3436-3438 —— 2D 视图返回 ACS 面；DanQing 仅有 3D 视图（2D TODO）。
    auto* view = GetView();
    auto* view3d = view ? view->AsViewState3d() : nullptr;

    // 像素圆域中心：世界点 → CSS px（WorldToView 为 CSS 语义）。
    dqGeom::Point3d const cssPt = WorldToView(pickPoint);
    int const cssX = static_cast<int>(std::lround(cssPt.x));
    int const cssY = static_cast<int>(std::lround(cssPt.y));

    // CSS → device px + GL 行翻转（PickAtPoint 同款换算，Viewport.cpp 上文）。
    if (m_renderTarget && view3d) {
        auto const rect = m_renderTarget->viewRect();
        double const cssW = width() > 0 ? static_cast<double>(width()) : 1.0;
        double const dpr = rect.width() / cssW;
        int32_t const devX = static_cast<int32_t>(std::lround(cssX * dpr));
        int32_t const devY = static_cast<int32_t>(std::lround(cssY * dpr));
        int32_t const devR = std::max(1, static_cast<int>(std::ceil(radiusPixels * dpr)));
        uint32_t const devSide = static_cast<uint32_t>(2 * devR + 1);
        std::vector<uint32_t> ids(devSide * devSide, 0);
        std::vector<float> depths(devSide * devSide, 0.0f);
        int32_t const left = devX - devR;
        int32_t const topGL = static_cast<int32_t>(rect.height()) - 1 - devY - devR;
        bool const okIds = m_renderTarget->readPickData(left, topGL, devSide, devSide,
                                                        ids.data(), static_cast<uint32_t>(ids.size()));
        bool const okDepth = m_renderTarget->readPickDepth(left, topGL, devSide, devSide,
                                                           depths.data(), static_cast<uint32_t>(depths.size()));
        if (okIds && okDepth) {
            // 离圆心最近的非零 id 像素（同距取深度更近者）。
            int best = -1;
            double bestDist2 = 1.0e30;
            float bestDepth = 2.0f;
            for (uint32_t j = 0; j < devSide; ++j) {
                for (uint32_t i = 0; i < devSide; ++i) {
                    uint32_t const idx = j * devSide + i;
                    if (ids[idx] == 0) continue;
                    double const dx = static_cast<double>(i) - devR;
                    double const dy = static_cast<double>(j) - devR;
                    double const d2 = dx * dx + dy * dy;
                    if (d2 < bestDist2 - 1.0e-9 || (std::abs(d2 - bestDist2) < 1.0e-9 && depths[idx] < bestDepth)) {
                        bestDist2 = d2;
                        bestDepth = depths[idx];
                        best = static_cast<int>(idx);
                    }
                }
            }
            if (best >= 0) {
                // getPixelDataNpcPoint（Viewport.ts:2892-2909）：设备像素 → npc，
                // 透视时按 frustFraction 修正 z，再 npcToWorld。
                double const fx = static_cast<double>(best % devSide);
                double const fy = static_cast<double>(best / devSide);
                double const px = left + fx;
                double const pyGL = topGL + fy;
                double const npcY = (pyGL + 0.5) / rect.height();   // npc y=0=底（GL 底上行坐标即 npc 序，不翻）
                double const npcX = (px + 0.5) / rect.width();
                double z = bestDepth;
                double const ff = m_viewingSpace.getFrustFraction();
                if (ff < 1.0)
                    z = z * ff / (1.0 + z * (ff - 1.0));   // 相机开时修正到 npc
                result.origin = m_viewingSpace.NpcToWorld(dqGeom::Point3d::From(npcX, npcY, z));
                result.normal = view3d->GetZVec();
                result.source = DepthPointSource::Geometry;
                result.sourceId = ids[static_cast<size_t>(best)];   // ← hitDetail.sourceId
                return result;
            }
        }
    }

    // ---- 未命中回退链（:3463-3503） ----
    // 眼点方向：参考从 worldToViewMap.transform1.columnZ 取齐次眼点再归一；
    // 正交视图取 -z 视线方向。Ray3d 从 pickPoint 沿离眼方向。
    dqGeom::Vector3d dir;
    if (view3d && view3d->IsCameraOn()) {
        dqGeom::Point3d const eye = view3d->getEyePoint();
        dir = dqGeom::Vector3d::From(pickPoint.x - eye.x, pickPoint.y - eye.y, pickPoint.z - eye.z);
    } else {
        dir = dqGeom::Vector3d::From(0.0, 0.0, -1.0);
        if (view3d)
            dir = view3d->GetZVec();   // GetZVec = 视线反方向（RowZ）→ 朝场景为 -RowZ？见下
        // 参考 direction.scaleToLength(-1.0)：单位化并**反向**——从眼指向 pickPoint。
        dir = dqGeom::Vector3d::From(-dir.x, -dir.y, -dir.z);
    }
    if (dir.Normalize() == 0.0)
        dir = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
    dqGeom::Ray3d const ray(pickPoint, dir);

    // :3475-3484 — 背景图几何求交：先于 groundPlane/ACS——命中即 BackgroundMap
    // 源（参考的有效性白名单成员），法线取求交结果（cartesian 平面修正后恒 (0,0,1)）。
    if (auto const* bmg = backgroundMapGeometry()) {
        auto const intersect = bmg->getRayIntersection(ray, false);
        if (intersect.has_value()) {
            dqGeom::Point3d const npc = m_viewingSpace.WorldToNpc(intersect->origin);
            if (npc.z < 1.0) {  // :3481 — only if in front of eye
                result.origin = intersect->origin;
                result.normal = intersect->direction;
                result.source = DepthPointSource::BackgroundMap;
                return result;
            }
        }
    }

    auto boresiteIntersect = [&](dqGeom::Plane3dByOriginAndUnitNormal const& plane,
                                 dqGeom::Point3d& out) {
        auto const dist = ray.intersectionWithPlane(plane, &out);
        if (!dist.has_value())
            return false;
        // 仅在眼前方有效（npc z < 1——参考 :3472-3478 语义）
        dqGeom::Point3d const npc = m_viewingSpace.WorldToNpc(out);
        return npc.z < 1.0;
    };

    // backgroundMapGeometry 分支（:3480-3488）未移植：DanQing 无背景图射线求交子系统
    // （blank connection 无真实背景图）。TODO 登记。
    // groundPlane 分支（:3490-3494）：displayGround 默认关（blank connection）。
    {
        auto const& env = view ? view->GetDisplayStyle().getEnvironment() : dqCommon::Environment::defaults();
        if (env.displayGround) {
            // TODO: getGroundElevation 未移植（ViewState 地面高度）；参考地面层
            // 在 Deco/blank 视图恒不启用——分支保留位。
        }
    }

    // ACS 平面（:3496-3500）：DanQing 无 AuxCoordSystemState——blank connection 恒为
    // 默认 ACS（原点 (0,0,0)、单位旋转，即 z=0 平面）。TODO：AuxCoordSystemState
    // 移植后改为读视图 ACS。
    {
        dqGeom::Plane3dByOriginAndUnitNormal const acsPlane(
            dqGeom::Point3d::From(0.0, 0.0, 0.0), dqGeom::Vector3d::From(0.0, 0.0, 1.0));
        dqGeom::Point3d hit;
        if (boresiteIntersect(acsPlane, hit)) {
            result.origin = hit;
            result.normal = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
            // isGridOn + GridOrientationType.AuxCoord → Grid；DanQing 网格朝向枚举未
            // 移植，取 ACS（参考默认网格朝向即 AuxCoord 时方报 Grid——展示差异无）。
            result.source = DepthPointSource::ACS;
            return result;
        }
    }

    // TargetPoint 回退（:3502-3507）：光标点按视图目标点的 npc 深度投影。
    {
        dqGeom::Point3d targetNpc = m_viewingSpace.WorldToNpc(view3d ? view3d->GetTargetPoint()
                                                                     : dqGeom::Point3d::From(0, 0, 0));
        if (targetNpc.z < 0.0 || targetNpc.z > 1.0)
            targetNpc.z = 0.5;
        dqGeom::Point3d npc = m_viewingSpace.WorldToNpc(pickPoint);
        npc.z = targetNpc.z;
        result.origin = m_viewingSpace.NpcToWorld(npc);
        result.normal = view3d ? view3d->GetZVec() : dqGeom::Vector3d::From(0.0, 0.0, 1.0);
        result.source = DepthPointSource::TargetPoint;
        return result;
    }
}

// Ported from: itwinjs-core Viewport.backgroundMapGeometry getter (Viewport.ts:1483)。
BackgroundMapGeometry const* Viewport::backgroundMapGeometry() const
{
    // `return this.view.displayStyle.getBackgroundMapGeometry();`
    if (auto* vs = GetView())
        return vs->GetDisplayStyle().getBackgroundMapGeometry();
    return nullptr;
}

// Ported from: itwinjs-core Viewport.pickNearestVisibleGeometry (Viewport.ts:3404-3425)。
std::optional<dqGeom::Point3d> Viewport::pickNearestVisibleGeometry(dqGeom::Point3d const& pickPoint,
                                                                    double radiusPixels)
{
    // :3405 — depthResult = pickDepthPoint(pickPoint, radius, { excludeNonLocatable: !allowNonLocatable })
    // DanQing 无 excludeNonLocatable 通道（Locatable 过滤子系统 TODO）——默认全开，
    // 与参考默认参数 allowNonLocatable=true 一致。
    auto const depthResult = pickDepthPoint(pickPoint, radiusPixels);

    // :3407-3418 — 有效性门
    bool isValidDepth = false;
    switch (depthResult.source) {
        case DepthPointSource::Geometry:
        case DepthPointSource::Model:
        case DepthPointSource::Map:
            isValidDepth = true;
            break;
        default: {
            dqGeom::Point3d const npcPt = m_viewingSpace.WorldToNpc(depthResult.origin);
            isValidDepth = !(npcPt.z < 0.0 || npcPt.z > 1.0);
            break;
        }
    }
    if (!isValidDepth)
        return std::nullopt;
    return depthResult.origin;
}

// ---------------------------------------------------------------------------
// Qt input handlers → ToolAdmin.addEvent bridge (Task 14).
//
// Ported from: itwinjs-core EventController.ts DOM listeners
//               (mousedown/mouseup/mousemove/wheel/keydown/keyup)
//               → ToolAdmin.addEvent (ToolAdmin.ts:792).
//
// The reference registers DOM listeners on the viewport canvas; DanQing's
// equivalents are the Qt QWidget event overrides. Each handler extracts the
// fields the ToolEvent POD carries (Task 6) and enqueues it for processing on
// the next animation frame. The dispatch layer (Tasks 7/8/13) routes the
// events to the active/idle tool.
//
// The previous direct path (PickAtPoint + emit FeaturePicked in
// mousePressEvent; m_cameraController->Mouse*/WheelEvent in all handlers) is
// removed: SelectionTool now owns pick/select/hilite (Task 13), and
// IdleTool/ViewTool own camera manipulation (Tasks 10-12). Redraws are
// requested by the tool system via RequestRedraw — the update() calls are
// removed to mirror the reference (no Qt widget-level redraw trigger).
// Task 15 additionally removed CameraController + its RotateCamera/PanCamera/
// ZoomCamera Viewport wrappers + m_cameraController member: the faithful
// ViewRotate/ViewPan/ViewScroll tools (Tasks 9-11) supersede them.
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core EventController mousedown listener (ToolAdmin.ts:810).
void Viewport::mousePressEvent(QMouseEvent* event)
{
    ToolEvent te;
    te.type = ToolEventType::MouseDown;
    te.vp = this;
    QPointF const pos = event->position();
    te.posx = static_cast<float>(pos.x());
    te.posy = static_cast<float>(pos.y());
    te.button = qtToBeButton(event->button());
    te.modifiers = qtToModifiers(event->modifiers());
    te.isDoubleClick = (event->type() == QEvent::MouseButtonDblClick);
    Application::Get().GetToolAdmin().addEvent(te);
}

// Ported from: itwinjs-core EventController mouseup listener (ToolAdmin.ts:811).
void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
    ToolEvent te;
    te.type = ToolEventType::MouseUp;
    te.vp = this;
    QPointF const pos = event->position();
    te.posx = static_cast<float>(pos.x());
    te.posy = static_cast<float>(pos.y());
    te.button = qtToBeButton(event->button());
    te.modifiers = qtToModifiers(event->modifiers());
    Application::Get().GetToolAdmin().addEvent(te);
}

// Ported from: itwinjs-core EventController mouseout 监听 → ToolAdmin.onMouseLeave
// （EventController.ts:27 + ToolAdmin.ts:841/952-960）——Qt 的 QWidget::leaveEvent
// 即 DOM mouseout 的对应物。同时清 hover 置脏点，避免 renderFrame 对陈旧点再
// locate（参考离开后 cursorView 清空、locate 链整体停摆）。
void Viewport::leaveEvent(QEvent* event)
{
    if (std::getenv("DANQING_CURSOR_TRACE")) {
        fprintf(stderr, "[LEAVE] leaveEvent fired, cursorView=%p\n",
                (void*)Application::Get().GetToolAdmin().cursorView());
        fflush(stderr);  // 文件重定向全缓冲——kill 前不 flush 会丢（§12.9 教训5）
    }
    m_hoverDirty = false;
    Application::Get().GetToolAdmin().onMouseLeave(this);
    QWidget::leaveEvent(event);
}

// Ported from: itwinjs-core EventController mousemove listener (ToolAdmin.ts:812).
void Viewport::mouseMoveEvent(QMouseEvent* event)
{
    ToolEvent te;
    te.type = ToolEventType::MouseMove;
    te.vp = this;
    QPointF const pos = event->position();
    te.posx = static_cast<float>(pos.x());
    te.posy = static_cast<float>(pos.y());
    te.modifiers = qtToModifiers(event->modifiers());
    Application::Get().GetToolAdmin().addEvent(te);

    // Hover locate 输入记录（EQUIVALENCE——见 Viewport.h m_hoverDirty 注释）：
    // 参考经 AccuSnap.onMotion 每帧 locate 并设 vp.flashedId；DanQing 记录最新
    // hover 点，renderFrame step 13 帧合并 PickAtPoint（每 motion 一次 GL pick
    // 在 Qt 逐事件转发下过重——Chromium 合并语义的等价物）。
    m_hoverCssX = static_cast<int32_t>(std::lround(pos.x()));
    m_hoverCssY = static_cast<int32_t>(std::lround(pos.y()));
    m_hoverDirty = true;
    RequestRedraw();
}

// Ported from: itwinjs-core EventController wheel listener (ToolAdmin.ts:816).
void Viewport::wheelEvent(QWheelEvent* event)
{
    ToolEvent te;
    te.type = ToolEventType::Wheel;
    te.vp = this;
    QPointF const pos = event->position();
    te.posx = static_cast<float>(pos.x());
    te.posy = static_cast<float>(pos.y());
    QPoint const angle = event->angleDelta();
    te.wheelDeltaX = static_cast<float>(angle.x());
    te.wheelDeltaY = static_cast<float>(angle.y());
    te.modifiers = qtToModifiers(event->modifiers());
    Application::Get().GetToolAdmin().addEvent(te);
}

// Ported from: itwinjs-core EventController keydown listener (ToolAdmin.ts:817).
void Viewport::keyPressEvent(QKeyEvent* event)
{
    ToolEvent te;
    te.type = ToolEventType::KeyDown;
    te.vp = this;
    te.key = static_cast<uint32_t>(event->key());
    te.modifiers = qtToModifiers(event->modifiers());
    Application::Get().GetToolAdmin().addEvent(te);
}

// Ported from: itwinjs-core EventController keyup listener (ToolAdmin.ts:818).
void Viewport::keyReleaseEvent(QKeyEvent* event)
{
    ToolEvent te;
    te.type = ToolEventType::KeyUp;
    te.vp = this;
    te.key = static_cast<uint32_t>(event->key());
    te.modifiers = qtToModifiers(event->modifiers());
    Application::Get().GetToolAdmin().addEvent(te);
}

// ---------------------------------------------------------------------------
// Coordinate transform methods
// Ported from: itwinjs-core Viewport.worldToView/viewToWorld/worldToNpc/npcToWorld
// ---------------------------------------------------------------------------
dqGeom::Point3d Viewport::WorldToView(dqGeom::Point3d const& pt) const
{
    return m_viewingSpace.WorldToView(pt);
}

dqGeom::Point3d Viewport::ViewToWorld(dqGeom::Point3d const& pt) const
{
    return m_viewingSpace.ViewToWorld(pt);
}

dqGeom::Point3d Viewport::WorldToNpc(dqGeom::Point3d const& pt) const
{
    return m_viewingSpace.WorldToNpc(pt);
}

dqGeom::Point3d Viewport::NpcToWorld(dqGeom::Point3d const& pt) const
{
    return m_viewingSpace.NpcToWorld(pt);
}

dqGeom::Point3d Viewport::NpcToView(dqGeom::Point3d const& pt) const
{
    return m_viewingSpace.NpcToView(pt);
}

dqGeom::Point3d Viewport::ViewToNpc(dqGeom::Point3d const& pt) const
{
    return m_viewingSpace.ViewToNpc(pt);
}

// ---------------------------------------------------------------------------
// Coordinate transform array helpers (WindowArea/Look W4)
// Ported from: itwinjs-core Viewport.worldToViewArray (Viewport.ts:2092) /
//              viewToWorldArray (:2096) / worldToNpcArray (:2088) /
//              npcToWorldArray (:2090) — 参考转发 ViewingSpace 同名方法
//              （ViewingSpace.ts:427-457 逐点 transform0/1）；DanQing 逐点调单点
//              API（同一循环结构，计划 §Global Constraints 第 23 行的既定方案）。
// ---------------------------------------------------------------------------
void Viewport::worldToViewArray(std::vector<dqGeom::Point3d>& pts) const
{
    for (auto& pt : pts)
        pt = WorldToView(pt);
}

void Viewport::viewToWorldArray(std::vector<dqGeom::Point3d>& pts) const
{
    for (auto& pt : pts)
        pt = ViewToWorld(pt);
}

void Viewport::worldToNpcArray(std::vector<dqGeom::Point3d>& pts) const
{
    for (auto& pt : pts)
        pt = WorldToNpc(pt);
}

void Viewport::npcToWorldArray(std::vector<dqGeom::Point3d>& pts) const
{
    for (auto& pt : pts)
        pt = NpcToWorld(pt);
}

// Ported from: itwinjs-core Viewport.viewDelta (Viewport.ts:562 —
//              `get viewDelta(): Vector3d { return this._viewingSpace.viewDelta; }`)。
dqGeom::Vector3d Viewport::viewDelta() const
{
    return m_viewingSpace.viewDelta();
}

// Ported from: itwinjs-core Viewport.pixelsFromInches = inches * pixelsPerInch
//               (Viewport.ts:2099); pixelsPerInch = 96 (Viewport.ts:1441-1444,
//               "apparently unobtainable information in a browser").
double Viewport::PixelsFromInches(double inches) const
{
    return inches * 96.0;
}

}  // namespace dqApp
