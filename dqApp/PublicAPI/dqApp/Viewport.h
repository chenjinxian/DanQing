// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Viewport (native window + Swapchain rendering surface)
// Ported from: itwinjs-core core/frontend/src/Viewport.ts (ScreenViewport)
//              FreeCAD src/Gui/View3DInventorViewer.h
//
// The Viewport is a QWidget that displays a ViewState. It owns
// a dqRender RenderTarget and Swapchain, driving the rendering loop
// via RenderFrame().
//
// Architecture:
//   Viewport : QWidget
//     └─ native window handle (NSView / HWND)
//     └─ RenderPipeline (owns Swapchain + Driver + RenderSystem)
//          └─ Swapchain.acquire() → render to FBO 0
//          └─ Swapchain.present() → display on screen
//
// The rendering pipeline matches itwinjs-core's Viewport.renderFrame() with
// 17 steps: change flags → animation → resize → controller → scene →
// render plan → decorations → flash → draw → events.
#pragma once

#include "Export.h"
#include "Animator.h"
#include "ChangeFlags.h"
#include "DecorationsCache.h"
#include "ViewPose.h"
#include "ViewState.h"
#include "ViewingSpace.h"

#include <dqBase/DqTime.h>
#include <dqBase/DqEvent.h>  // onTileLoad 订阅令牌（m_onTileLoadDisconnect）

#include <dqCommon/SkyBox.h>   // m_skyGradientCache（SkyGradient）

#include <optional>            // std::optional（m_skyGradientCache 等）

#include <dqRender/CreateTextureArgs.h>
#include <dqRender/RenderMemory.h>
#include <dqRender/Decorations.h>
#include <dqRender/PlanarGridProps.h>
#include <dqRender/RenderGraphic.h>
#include <dqRender/RenderPlan.h>
#include <dqRender/Scene.h>
#include <dqRender/rhi/Handle.h>
#include <dqRender/tile/TileAdmin.h>

#include <dqCommon/FeatureOverrides.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/PerModelCategoryVisibility.h>

#include <QWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace dqRender {
class RenderPipeline;
class RenderSystem;
class RenderTarget;
class Swapchain;
struct GltfScene;
class TileTree;
class GraphicBuilder;
struct GraphicBuilderOptions;
namespace rhi { class Driver; }
}  // namespace dqRender

namespace dqGeom { class IndexedPolyface; }

namespace dqApp {

class IModelConnection;
class BackgroundMapGeometry;
class IDecorator;

// Ported from: itwinjs-core Viewport.ts — ViewUndoEvent 枚举（undo/redo 事件载荷，
// Viewport.ts:168）。
enum class ViewUndoEvent : uint8_t { Undo, Redo };

// Ported from: itwinjs-core DepthPointSource (Viewport.ts:99-117) —— pickDepthPoint
// 命中来源。（参考属 Viewport.ts 所有；此前寄居 ViewTool.h，现迁回本源——
// ViewTool.h include 本头。）
enum class DepthPointSource : uint8_t {
    Geometry      = 0,  // Depth point from geometry within radius of pick point
    Model         = 1,  // Depth point from reality model
    BackgroundMap = 2,  // Depth point from ray projection to background map plane
    GroundPlane   = 3,  // Depth point from ray projection to ground plane
    Grid          = 4,  // Depth point from ray projection to grid plane
    ACS           = 5,  // Depth point from ray projection to ACS plane
    TargetPoint   = 6,  // Depth point from plane through view target point
    Map           = 7,  // Depth point from map/terrain within radius
};

// Ported from: itwinjs-core ViewAnimation.ts ViewChangeOptions
// (:90-97 extends OnViewExtentsError, ViewAnimationOptions —— 压平为单层；
// noSaveInUndo 在前保持既有聚合初始化 `{true}` 兼容）。
struct ViewChangeOptions {
    bool noSaveInUndo = false;         // :91 — Default is to save in undo.
    // :93 — animateFrustumChange?: boolean。两处参考语义不同，故保留三态：
    //   synchWithView（Viewport.ts:3595）：`true === options.animateFrustumChange`
    //     —— 显式 true 才动画（默认不动画）。
    //   changeView（:3618）：`false !== opts.animateFrustumChange`
    //     —— 显式 false 才不动画（undefined ⇒ 坐标一致即动画）。
    std::optional<bool> animateFrustumChange;
    // ← ViewAnimationOptions（:36-45）字段平铺（经继承进入参考同名结构）：
    std::optional<double> animationTime;                       // :38 — 毫秒
    bool cancelOnAbort = false;                                // :39 — 参考 undefined 默认
    EasingFunction easingFunction = EasingFunction::CubicOut;  // :42 — 默认 Easing.Cubic.Out
    std::function<void(bool)> animationFinishedCallback;       // :44 — didComplete

    // skipAspectFix：滚轮缩放的正交分支（Viewport.ts:2211-2225 vp.zoom）不调用
    // FixAspectRatio——它直接 setExtents(extents*factor)，y 随 x 同比例缩放。DanQing
    // 的 doZoom 走 synchWithView → SetupFromView → FixAspectRatio，把 y 强行改回
    // x/aspect，对 y 引入 1.185 的额外压缩（8 事件累积 1.185⁸≈3.9 倍），即连滚净比
    // 比 DTA 慢 ~14% 的根因。滚轮路径置 true 跳过 FixAspectRatio，与参考 1:1。
    bool skipAspectFix = false;

    // 转 AnimationOptions（FrustumAnimator 实参）。
    AnimationOptions ToAnimationOptions() const
    {
        AnimationOptions o;
        o.animationTime = animationTime;
        o.cancelOnAbort = cancelOnAbort;
        o.easingFunction = easingFunction;
        o.animationFinishedCallback = animationFinishedCallback;
        o.skipAspectFix = skipAspectFix;
        return o;
    }
};

// Ported from: itwinjs-core ScreenViewport.animation (Viewport.ts:3125-3160)。
// 视图动画的全局设置（时长/缓动/zoom-out 参数）。
struct ViewportAnimationSettings {
    struct Time {                              // ← :3126-3133 animation.time（毫秒）
        double fast = 500.0;                   // :3128
        double normal = 1000.0;                // :3129
        double slow = 1250.0;                  // :3130
        double wheel = 500.0;                  // :3132
    } time;
    EasingFunction easing = EasingFunction::CubicOut;  // :3135 — Easing.Cubic.Out
    struct ZoomOut {                           // ← :3136-3158 animation.zoomOut
        bool enable = true;                    // :3142
        // :3144 interpolation: Interpolation.Bezier（缩放插值器——zoom-out 未落地，
        // 见 Animator.h 登记；数组语义保留供落地时使用）
        double heights[8] = {0.0, 1.5, 2.0, 1.8, 1.5, 1.2, 1.0, 0.0};   // :3149
        double positions[7] = {0.0, 0.0, 0.1, 0.3, 0.5, 0.8, 1.0};      // :3154
        double margin = 2.5;                   // :3156
        double durationFactor = 1.5;           // :3158
    } zoomOut;
};

// ---------------------------------------------------------------------------
// Viewport — native window rendering surface with itwinjs-core renderFrame() pipeline
//
// Combines itwinjs's ScreenViewport (rendering) with Qt's QWidget
// (window system integration). The Swapchain is created from the native
// window handle, and rendering goes through the RHI pipeline.
//
// Ported from: itwinjs-core Viewport.ts ScreenViewport
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT Viewport : public QWidget, public dqRender::TileUser {
    Q_OBJECT

public:
    // Factory (← ScreenViewport.create)
    static Viewport* Create(QWidget* parent, dqBase::RefPtr<ViewState> view);

    ~Viewport() override;

    // IModel connection (← Viewport._iModel)
    IModelConnection* GetIModel() const;

    // Set the widget cursor from a reference CSS cursor name.
    // Ported from: itwinjs-core Viewport.setCursor (Viewport.ts:3580-3582 —
    // `this.canvas.style.cursor = cursor`). The CSS name is kept 1:1 and
    // mapped to the closest Qt cursor shape (see the mapping table in the .cpp).
    void setCursor(std::string const& cursor);

    // The render target (← IModelApp.renderSystem.createTarget(canvas)).
    // Diagnostics tooling reads per-frame state from it (debugControl family —
    // Target.getRenderCommands/displayNormalMaps).
    dqRender::RenderTarget* renderTarget() const noexcept { return m_renderTarget; }

    // Collect texture-memory statistics owned by this viewport's pipeline
    // (MemoryTracker calcMem(viewedTileTrees) consumer slice —
    // MemoryTracker.ts:52-57; texture consumers only — geometry buffers need
    // the collectStatistics walk on the internal Graphic tree).
    void collectTextureStatistics(dqRender::RenderMemory::Statistics& stats) const;

    // Record graphics memory consumed by this viewport (disclosed tile trees +
    // view non-tile-tree sources).
    // Ported from: itwinjs-core Viewport.collectStatistics (Viewport.ts:2948-2954 —
    // non-const, same as the reference; discloseTileTrees is a non-const
    // TileUser override).
    // NOTE: the reference also calls view.collectNonTileTreeStatistics(stats)
    // (background-map terrain etc.) — DanQing's ViewState lacks that walk until
    // the background-map pipeline lands; the tile-tree walk is complete.
    void collectStatistics(dqRender::RenderMemory::Statistics& stats);

    // View management (← Viewport.changeView)
    // :3615 — opts 为空时参考默认 { animationTime: animation.time.slow }（1.25s）。
    void ChangeView(dqBase::RefPtr<ViewState> view, ViewChangeOptions const* opts = nullptr);
    ViewState* GetView() const { return m_view.Get(); }

    // Graphic factories — delegate to this viewport's RenderPipeline (the real GL driver).
    // These bypass the global RenderSystem::get(), which is a no-op stub unless a system
    // was installed via RenderSystem::setInstance() (the DTA pipeline does not install one).
    // Ported from: itwinjs-core RenderSystem.createGraphicFromPolyface/createGraphicList/
    //               createGraphicOwner — DanQing routes them through the per-viewport driver.
    dqRender::RenderGraphic* createGraphicFromPolyface(dqGeom::IndexedPolyface const* polyface,
                                                       uint32_t defaultColor, uint32_t featureId,
                                                       dqRender::rhi::TextureHandle texture = {},
                                                       dqRender::rhi::TextureHandle normalMapTexture = {},
                                                       float normalMapScale = 1.0f,
                                                       bool textureExternal = false,
                                                       bool normalMapTextureExternal = false);
    dqRender::RenderGraphic* createGraphicList(std::vector<dqRender::RenderGraphic*> graphics);
    dqRender::RenderGraphicOwner* createGraphicOwner(dqRender::RenderGraphic* owned);
    // Texture factory — same per-viewport-driver routing as the graphic factories.
    // ← itwinjs-core RenderSystem.createTexture (per-viewport driver routing).
    dqRender::rhi::TextureHandle createTexture(dqRender::CreateTextureArgs const& args);

    // The viewport's GL driver (owned by RenderPipeline; nullptr before initialization).
    // Callers needing driver-bound resource management (texture cache lifetime)
    // route through this — same driver createTexture/createGraphicFromPolyface use.
    dqRender::rhi::Driver* getDriver();
    // PlanarGrid factory — same per-viewport-driver routing.
    // Ported from: itwinjs-core RenderSystem.createPlanarGrid（经
    //              viewport.target.renderSystem，ViewContext.ts:348 调用点）。
    dqRender::RenderGraphic* createPlanarGrid(dqCommon::Frustum const& frustum,
                                              dqRender::PlanarGridProps const& props);
    // GraphicBuilder factory — same per-viewport-driver routing as above.
    // Ported from: itwinjs-core RenderSystem.createGraphicBuilder (GraphicBuilder.ts).
    std::unique_ptr<dqRender::GraphicBuilder> createGraphicBuilder(
        dqRender::GraphicBuilderOptions const& options);

    // ViewState event binding (← itwinjs-core: attachToView/detachFromView)
    // Register listeners on the current ViewState so that property changes
    // (DisplayStyle, categories, models, clip vector) trigger invalidation.
    void AttachToView();
    void DetachFromView();

    // Rendering (← Viewport.renderFrame — 17-step pipeline)
    void RenderFrame();
    // U-flip saga minimal repro (test-only): bypasses compositor; see
    // dqRender/src/render/TextureQuadProbe.cpp. 0=correct 1=U-flipped.

    // Synchronously release the GL pipeline (render target, grid graphic,
    // RenderPipeline/Swapchain/Driver) and mark the viewport shut down so that
    // RenderFrame/resizeEvent become no-ops. Idempotent. Called from the owning
    // MDI view's closeEvent and from ViewManager::ShutdownAll (quit path) to
    // guarantee the pipeline is torn down BEFORE Qt destroys the native window
    // surface — the crash-on-close fix (otherwise the render timer can run
    // RenderFrame -> Swapchain::acquire -> makeCurrent ->
    // [NSOpenGLContext setView:<freed NSView>] -> use-after-free).
    void Shutdown();

    // Pick query: read feature ID at pixel coordinate
    // Ported from: itwinjs-core Viewport.pickAtPoint (reads the pick buffer
    //               built during the most recent render pass).
    uint32_t PickAtPoint(int32_t x, int32_t y);

    // Ported from: itwinjs-core Viewport.pickDepthPoint (Viewport.ts:3435-3503)。
    // 在 pickPoint（世界坐标，光标视线与场景的候选深度点）周围按像素半径读
    // pick 缓冲（featureId + depthAndOrder 附件），返回命中平面原点/法线/来源。
    // DanQing 适配：参考经 ElementPicker.doPick 的圆域拾取；此处读
    // (2r+1)² 矩形取"离圆心最近的非零 featureId 像素"（参考取优先级+距离排序
    // 的首命中——单几何场景等价）。
    struct DepthPointResult {
        dqGeom::Point3d origin;          // 命中平面原点（世界坐标）
        dqGeom::Vector3d normal;         // 命中平面法线
        dqApp::DepthPointSource source = dqApp::DepthPointSource::TargetPoint;
        uint32_t sourceId = 0;           // 几何命中的 elementId（HitDetail.sourceId 对应物；0=无）
    };
    DepthPointResult pickDepthPoint(dqGeom::Point3d const& pickPoint, double radiusPixels);

    // Ported from: itwinjs-core Viewport.backgroundMapGeometry getter
    //              (Viewport.ts:1483 — `return this.view.displayStyle
    //              .getBackgroundMapGeometry()`)。ecefLocation 门（无 ecef 的
    // 连接恒 null）——pickDepthPoint 的背景图求交分支的唯一来源。
    // 返回值归 DisplayStyle 缓存所有（借用语义）。
    BackgroundMapGeometry const* backgroundMapGeometry() const;

    // Ported from: itwinjs-core Viewport.pickNearestVisibleGeometry (Viewport.ts:3404-3425) —
    // pickDepthPoint 的有效性门（Geometry/Model/Map 直通；平面源 npc z∈[0,1]），
    // 有效返回命中点，无效 nullopt。WheelEventProcessor.doZoom 透视分支的目标点来源。
    std::optional<dqGeom::Point3d> pickNearestVisibleGeometry(dqGeom::Point3d const& pickPoint,
                                                              double radiusPixels);

#ifdef DANQING_TESTING
    // DIAG(grid-app): 应用路径合成帧回读。
    bool ReadFrameForTest(std::vector<uint8_t>& rgbaOut);
    // 同 ReadFrameForTest，并输出渲染目标（设备像素）尺寸。
    bool ReadFrameForTest(std::vector<uint8_t>& rgbaOut, uint32_t& wOut, uint32_t& hOut);

    // Test-only injection of a fixed PickAtPoint result. When non-zero, the
    // next PickAtPoint call returns this value verbatim and skips the GL
    // readPixels / pickQuery path (which requires a live render target — not
    // available in shader-link-broken Step 3 or in headless GoogleTest runs).
    // Zero (the default) means "no override; use the real pick path".
    //
    // Rationale: Viewport's ctor is private (factory-only) and PickAtPoint is
    // non-virtual, so the test cannot subclass-and-override. Injecting the
    // feature id mirrors the brief's prescribed seam
    // (.git/sdd/task-13-brief.md "inject the feature id"). The setter name is
    // deliberately verbose so production callers do not reach for it.
    //
    // §8.4 guard: this whole seam (methods + member + PickAtPoint short-circuit
    // in Viewport.cpp) is compiled only when dqApp is built with
    // DANQING_TESTING=1. The macro is set only inside the `DANQING_BUILD_TESTS`
    // block of dqApp/CMakeLists.txt — the symbols vanish from the SDK surface
    // for any non-test consumer.
    //
    // Authored: test affordance — no reference equivalent; itwinjs tests
    //           PickAtPoint via a live WebGL render target.
    void SetPickResultForTest(uint32_t featureId) { m_pickResultOverride = featureId; }
    uint32_t GetPickResultOverrideForTest() const noexcept { return m_pickResultOverride; }

    // Test-only access to the decorations-validity flag (← itwinjs-core
    // Viewport._decorationsValid: set true after changeDecorations
    // (Viewport.ts:2678); cleared by invalidateDecorations (:414)). Headless GoogleTest
    // runs cannot drive RenderFrame's recollection branch (no GL pipeline), so
    // the setter simulates "a frame recollected the decorations" and the getter
    // observes invalidateDecorations / invalidateDecorationsAllViews effects.
    // Same §8.4 guard as SetPickResultForTest: compiled only under
    // DANQING_TESTING=1 (dqApp/CMakeLists.txt DANQING_BUILD_TESTS block).
    //
    // Authored: test affordance — no reference equivalent; itwinjs observes
    //           decoration invalidation through the render loop.
    bool GetDecorationsValidForTest() const noexcept { return m_decorationsValid; }
    void SetDecorationsValidForTest(bool valid) noexcept { m_decorationsValid = valid; }
#endif  // DANQING_TESTING

    // Viewport ID
    int GetViewportId() const { return m_viewportId; }

    // --- Frustum / view wrappers (Task 4) ---
    // Thin wrappers over the Task-3 ViewState3d frustum API, exposed on the
    // Viewport for ViewTool / ViewManip camera machinery (Tasks 9-10). These
    // mirror itwinjs-core Viewport.ts accessors (camelCase per §3.3 — Viewport
    // is a TS-ported class).

    // Ported from: itwinjs-core Viewport.getFrustum (Viewport.ts:2113)
    //   getFrustum(sys = World, adjustedBox = true, box?) → viewingSpace.getFrustum
    // Returns the view's frustum in world (world=true) or view-local (false)
    // coordinates. Guards null view → default Frustum. adjustedBox=false 取
    // 未扩展盒（zClip 调整前——视图工具的工作视锥，ViewingSpace.ts:471-503）。
    dqCommon::Frustum getFrustum(bool world = true, bool adjustedBox = true) const;

    // Ported from: itwinjs-core Viewport.getWorldFrustum (Viewport.ts:2116)
    //   getFrustum(CoordSystem.World, /*adjustedBox=*/false) —— **未扩展**世界视锥
    // （参考传 false；ViewRotate/ViewPan/ViewLook 的 firstPoint/perform 用此
    // 做 SetupFromFrustum——用扩展深锥会把 extents 炸到背景图尺度，2026-09-20
    // 锚定拖动视口错乱事故的根因）。
    dqCommon::Frustum getWorldFrustum() const;

    // Ported from: itwinjs-core Viewport.setupViewFromFrustum (Viewport.ts:2289)
    // Apply a frustum to the view, then synchronize the viewport. Returns the
    // SetupFromFrustum validity result. Always calls SetupFromView + invalidate
    // (matches itwinjs "always call setupFromView, even if setupFromFrustum failed").
    bool setupViewFromFrustum(dqCommon::Frustum const& inFrustum);

    // Ported from: itwinjs-core Viewport.setupFromView (Viewport.ts:2001-2022).
    // Public sync: rebuild the ViewingSpace transform chain from the current
    // ViewState. Tools that mutate the view (pan/rotate/scroll) call this to keep
    // m_viewingSpace current — Viewport::getFrustum reads it, so without a sync
    // after a view mutation a subsequent getFrustum would be stale.
    void SetupFromView();
    // skipAspectFix=true 时跳过 FixAspectRatio——滚轮缩放正交分支（Viewport.ts:
    // 2211-2225 vp.zoom）不走 FixAspectRatio 的 1:1 对齐（见 ViewChangeOptions::
    // skipAspectFix 注释）。
    void SetupFromView(bool skipAspectFix);
    // Ported from: itwinjs-core Viewport.setupFromView(pose?)（Viewport.ts:2062-2066）：
    // pose 非空则先 applyPose 再同步（FrustumAnimator 完成帧调用点，:110）。
    void SetupFromView(ViewPose const* pose);

    // Ported from: itwinjs-core Viewport.scroll (Viewport.ts:2121-2141)
    // Translate the view origin by `dist` in world coordinates (orthographic
    // branch only — camera-on perspective path is deferred with TODO).
    void scroll(dqGeom::Vector3d const& dist);

    // Ported from: itwinjs-core ViewState3d.getRotation (accessed via
    // Viewport.view.getRotation(); the Viewport wrapper is DanQing glue for
    // tool access). Returns the 3D view's rotation matrix, or identity if
    // the view is null or 2D.
    dqGeom::Matrix3d getRotation() const;

    // Ported from: itwinjs-core Viewport.getContrastToBackgroundColor
    // (Viewport.ts:2480-2483 + _wantInvertBlackAndWhite :2475-2478). Returns black
    // when the background is bright ((r+g+b) > (255*3)/2), else white — used for the
    // grid color (ViewContext.drawStandardGrid, ViewContext.ts:347).
    dqCommon::ColorDef getContrastToBackgroundColor() const;

    // Ported from: itwinjs-core Viewport.isGridOn (Viewport.ts:637) — true iff the
    // view's viewFlags.grid bit is set (the on/off switch for grid drawing).
    bool isGridOn() const noexcept;

    // Ported from: itwinjs-core ScreenViewport.viewRect (Viewport.ts:3505)
    // Pixel rectangle of the viewport: {0, 0, width(), height()}.
    ViewRect viewRect() const noexcept;

    // Ported from: itwinjs-core Viewport.isCameraOn (Viewport.ts:1741)
    // True iff the view is a 3D view with camera enabled.
    bool isCameraOn() const noexcept;

    // Ported from: itwinjs-core Viewport.pickDepthPoint (Viewport.ts:3394)
    // Step 4 stub: returns the input point unchanged. Real depth-pick
    // (readPixels against the pick buffer) is deferred until geometry is
    // renderable end-to-end.
    // TODO: real depth pick (readPixels) — Step 4 with geometry.
    dqGeom::Point3d pickDepthPoint(dqGeom::Point3d pt, double pickRadius) const;

    // Invalidation methods (← itwinjs-core cascading invalidation chain)
    // invalidateController → invalidateRenderPlan → invalidateScene → invalidateDecorations
    void InvalidateController();
    void InvalidateRenderPlan();
    void InvalidateScene();
    void InvalidateDecorations();
    void RequestRedraw();

    // If true, a new frame renders on every tick of the render loop (e.g. FPS
    // tracking). May negatively impact battery life.
    // Ported from: itwinjs-core Viewport.continuousRendering
    // (Viewport.ts:1455-1461 — the setter requests the next frame when turning
    // on; the renderFrame tail keeps requesting while on).
    bool continuousRendering() const noexcept { return m_doContinuousRendering; }
    void setContinuousRendering(bool contRend);

    // Mark selection set as dirty (triggers hilite set update on next frame)
    void SetSelectionSetDirty() { m_selectionSetDirty = true; }

    // Set the hilited feature ID (0 = none) — DanQing-side state mirror for the
    // GetHilitedFeature accessor. The DRAW path follows the reference: the
    // iModel's HiliteSet drives RenderTarget::setHiliteSet from renderFrame
    // step 6 when the selection set is dirty (Viewport.ts:2613-2617).
    // Ported from: itwinjs-core Viewport.renderFrame step 6
    //               (target.setHiliteSet(view.iModel.hilited))
    void SetHilitedFeature(uint32_t featureId);

    // Get the currently-hilited feature ID (0 = none).
    // Ported from: itwinjs-core Target HiliteSet query (mirrors setHiliteSet
    //               state; reading it back is the natural pair).
    uint32_t GetHilitedFeature() const noexcept { return m_hiliteFeatureId; }
    // ← itwinjs-core Viewport.hilite.color（Hilite.Settings 默认 (0x23,0xbb,0xfc)，
    //   Hilite.ts:53）——ViewManip.previewDepthPoint 的非几何源圆色（ViewTool.ts:365）。
    dqCommon::ColorDef GetHiliteColorDef() const noexcept
    {
        return dqCommon::ColorDef::from(
            static_cast<uint8_t>(m_hiliteColor[0] * 255.0f + 0.5f),
            static_cast<uint8_t>(m_hiliteColor[1] * 255.0f + 0.5f),
            static_cast<uint8_t>(m_hiliteColor[2] * 255.0f + 0.5f));
    }

    // Set the hilite color (RGB). 参考默认 ColorDef.from(0x23,0xbb,0xfc)
    // （Hilite.ts:53）。
    void setHiliteColor(float r, float g, float b);

    // Get this viewport's render system (per-viewport pipeline owner) — the
    // factory for batch-wrapped pickable graphics.
    // ← itwinjs-core: viewport.target.renderSystem（GltfDecoration.ts 经
    //   IModelApp.renderSystem；DanQing 渲染系统按视口管道持有）
    dqRender::RenderSystem* renderSystem() const;

    // Get the current scene (for testing)
    dqRender::Scene const& GetScene() const { return m_scene; }

    // Get the current decorations (for testing)
    dqRender::Decorations const& GetDecorations() const { return m_decorations; }

    // Tile tree management (Phase 2b)
    // Ported from: itwinjs-core Viewport.addTileTreeReference()
    void AddTileTree(dqRender::TileTree* tree);
    void RemoveTileTree(dqRender::TileTree* tree);

    // Register a provider of tile graphics (application injection channel).
    // <- itwinjs-core Viewport.addTiledGraphicsProvider (Viewport.ts:1729-1732).
    void AddTiledGraphicsProvider(class TiledGraphicsProvider* provider);
    // <- Viewport.dropTiledGraphicsProvider (Viewport.ts:1738-1740).
    void DropTiledGraphicsProvider(class TiledGraphicsProvider* provider);
    // <- Viewport.hasTiledGraphicsProvider (Viewport.ts:1743-1745).
    bool HasTiledGraphicsProvider(class TiledGraphicsProvider* provider) const;
    std::vector<dqRender::TileTree*> const& GetTileTrees() const { return m_tileTrees; }
    // NOTE(2026-09-21): AddTileTree is a temporary scaffold for the offline
    // tileset tests (TileTreeRender*). The faithful port replaces it with the
    // reference's view-side seam — TileTreeReference/TileTreeOwner/
    // TileTreeSupplier + the overridable SpatialTileTreeReferences.create
    // factory (PrimaryTileTree.ts:601-606, called from SpatialViewState's
    // constructor SpatialViewState.ts:109 — the seam frontend-tiles replaces
    // in initializeFrontendTiles, FrontendTiles.ts:215). Trees are
    // caller-owned raw pointers that must outlive the viewport (teardown
    // frees their contents first, then never touches them again).

    // TileUser interface (← itwinjs-core TileUser)
    uint32_t getTileUserId() const override { return static_cast<uint32_t>(m_viewportId); }
    void discloseTileTrees(std::vector<dqRender::TileTree*>& trees) override;

    // The number of outstanding requests for tiles to be displayed in this viewport.
    // Ported from: itwinjs-core Viewport.numRequestedTiles (Viewport.ts:1836).
    size_t numRequestedTiles() const;
    // The number of tiles selected for display in the view as of the most
    // recently-drawn frame.
    // Ported from: itwinjs-core Viewport.numSelectedTiles (Viewport.ts:1844).
    size_t numSelectedTiles() const;
    // The number of tiles which were ready and met the desired level-of-detail
    // for display as of the most recently-drawn frame.
    // Ported from: itwinjs-core Viewport.numReadyTiles (Viewport.ts:1855).
    size_t numReadyTiles() const;

    // --- Coordinate transforms ---
    // Ported from: itwinjs-core Viewport.worldToView/viewToWorld/worldToNpc/npcToWorld
    dqGeom::Point3d WorldToView(dqGeom::Point3d const& pt) const;
    dqGeom::Point3d ViewToWorld(dqGeom::Point3d const& pt) const;
    dqGeom::Point3d WorldToNpc(dqGeom::Point3d const& pt) const;
    dqGeom::Point3d NpcToWorld(dqGeom::Point3d const& pt) const;
    // Ported from: itwinjs-core Viewport.npcToView/viewToNpc (Viewport.ts:2045/2050)
    dqGeom::Point3d NpcToView(dqGeom::Point3d const& pt) const;
    dqGeom::Point3d ViewToNpc(dqGeom::Point3d const& pt) const;
    // Ported from: itwinjs-core Viewport.pixelsFromInches (Viewport.ts:2099);
    //              pixelsPerInch = 96 (Viewport.ts:1441-1444).
    double PixelsFromInches(double inches) const;

    // --- Coordinate transform array helpers (WindowArea/Look W4) ---
    // 循环包装：逐点调上面的单点 API（参考内部同为此结构——ViewingSpace 的
    // worldToViewArray 等亦逐点 transform0，ViewingSpace.ts:427-457）。
    // Ported from: itwinjs-core Viewport.worldToViewArray (Viewport.ts:2092)
    void worldToViewArray(std::vector<dqGeom::Point3d>& pts) const;
    // Ported from: itwinjs-core Viewport.viewToWorldArray (Viewport.ts:2096)
    void viewToWorldArray(std::vector<dqGeom::Point3d>& pts) const;
    // Ported from: itwinjs-core Viewport.worldToNpcArray (Viewport.ts:2088)
    void worldToNpcArray(std::vector<dqGeom::Point3d>& pts) const;
    // Ported from: itwinjs-core Viewport.npcToWorldArray (Viewport.ts:2090)
    void npcToWorldArray(std::vector<dqGeom::Point3d>& pts) const;

    // Ported from: itwinjs-core Viewport.viewDelta (Viewport.ts:562 —
    //              `return this._viewingSpace.viewDelta`，视口当前视图体的世界尺寸)。
    dqGeom::Vector3d viewDelta() const;

    // Get the ViewingSpace (transform chain)
    ViewingSpace const& GetViewingSpace() const { return m_viewingSpace; }

    // Flash management (← itwinjs-core Viewport.flashedId, flashIntensity)
    // Ported from: itwinjs-core Viewport.ts lines 2485-2507
    uint32_t GetFlashedId() const { return m_flashedId; }
    void SetFlashedId(uint32_t id);
    bool processFlash();  // returns true if flash state changed

    // Animation (← itwinjs-core Viewport.setAnimator)
    // Ported from: itwinjs-core Viewport.ts lines 2537-2539
    void setAnimator(std::unique_ptr<Animator> animator);
    // 非拥有 animator 注册（← 参考同一 setAnimator 的 GC 语义适配）：参考的
    // vp.setAnimator(this)（ViewTool.ts:1072，HandleWithInertia/AnimatedHandle）
    // 依赖 TS GC 保证句柄存活；C++ 中这些 Animator 由 ViewHandleArray 持有，
    // 独占所有权会悬空——非拥有引用等价实现。拥有型/非拥有型互斥，后设者
    // 先清对方（参考单槽语义）。
    void setAnimatorRef(Animator* animator);
    Animator* getAnimator() const { return m_animator.get(); }
    // 非拥有槽只读访问（ViewManip::onCleanup 的所有权转移判据——
    // 惯性动画器在工具退出时若仍装机，须转拥有槽续跑而不是随句柄一并释放）。
    Animator* getAnimatorRef() const noexcept { return m_animatorRef; }

    // Ported from: itwinjs-core ScreenViewport.animation 静态设置（Viewport.ts:3125）。
    // 返回可变引用——参考是 public static（ToolSettingsTracker 的 Animation
    // Duration 控件运行时写 `ScreenViewport.animation.time.normal`）。
    static ViewportAnimationSettings& animation() noexcept
    {
        static ViewportAnimationSettings s_settings;
        return s_settings;
    }

    // Smoothly transition the viewport to its current view state (uses
    // FrustumAnimator). Called by ViewTools after modifying the ViewState,
    // as opposed to synchWithView which transitions immediately.
    // Ported from: itwinjs-core Viewport.animateFrustumChange (Viewport.ts:3519-3522)。
    void animateFrustumChange(AnimationOptions const& options);

    // Per-model category visibility (← itwinjs-core Viewport.perModelCategoryVisibility)
    // Ported from: itwinjs-core Viewport.ts line 1513
    dqCommon::PerModelCategoryVisibilityOverrides& GetPerModelCategoryVisibility() { return m_perModelCategoryVisibility; }
    dqCommon::PerModelCategoryVisibilityOverrides const& GetPerModelCategoryVisibility() const { return m_perModelCategoryVisibility; }

    // Bridge: populate per-model subcategory overrides into FeatureOverrides.
    // Ported from: itwinjs-core Viewport.ts addModelSubCategoryVisibilityOverrides (line 1519)
    void addModelSubCategoryVisibilityOverrides(dqCommon::FeatureOverrides& fs);

    // Fired when ChangeView() replaces the ViewState.
    // Ported from: itwinjs-core Viewport.ts onChangeView (line 337)
    // Uses DqEvent (not Qt signal) so ViewportSync can subscribe with AddListener.
    dqBase::DqEvent<ViewState*> OnChangeView;

    // --- 视图撤销/重做（Viewport.ts:3164-3718） ---
    int maxUndoSteps = 20;                                     // :3166
    // :521（0.5s，公有可变——saveViewUndo :3669 的 undoDelay.isZero 分支正是置零
    // 关防抖的参考逃生口）。
    static dqBase::DqDuration s_undoDelay;
    void saveViewUndo();                                       // :3649-3676
    void doUndo();                                             // :3679-3690（动画未移植：去 animationTime 形参）
    void doRedo();                                             // :3693-3704（同上）
    void clearViewUndo();                                      // :3641-3646
    void resetUndo();                                          // :3715-3718
    bool isUndoPossible() const { return !m_backStack.empty(); }   // :3635
    bool isRedoPossible() const { return !m_forwardStack.empty(); } // :3638
    // synchWithView = setupFromView + saveViewUndo（noSaveInUndo 跳过保存）。
    // 注：参考的 setupFromView 内部级联失效（doSetupFromView:2052-2053）；
    // DanQing 的 SetupFromView 故意不失效（RenderFrame 控制器同步路径依赖），
    // 故 synchWithView 末尾补 InvalidateController 以匹配参考语义。
    void synchWithView(ViewChangeOptions const& options = {});
    dqBase::DqEvent<Viewport*, ViewUndoEvent> OnViewUndoRedo;   // onViewUndoRedo (:314)

signals:
    // Viewport events — fired at end of renderFrame() based on ChangeFlags.
    // Ported from: itwinjs-core Viewport.ts event dispatch (lines 2662-2699)
    void ViewChanged();
    void ViewportChanged(ChangeFlags flags);
    void DisplayStyleChanged();
    void ViewedModelsChanged();
    void FeatureOverridesChanged();
    void AlwaysDrawnChanged();
    void NeverDrawnChanged();
    void ViewedCategoriesChanged();
    void ViewedCategoriesPerModelChanged();
    void FeatureOverrideProviderChanged();
    void SceneInvalidated();
    void ViewportResized();
    void OnFlashedIdChanged(uint32_t previous, uint32_t current);  // emitted when flashedId changes

protected:
    // QWidget overrides — own the GL lifecycle directly (native handle + Swapchain, not QOpenGLWidget)
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;   // WinIdChange → Swapchain::rebind（原生窗口重建后重绑 GL 表面）

    // QWidget overrides for native rendering (no Qt paint system)
    QPaintEngine* paintEngine() const override { return nullptr; }

    // Mouse interaction (← itwinjs ViewTool / FreeCAD NavigationStyle)
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;    void mouseReleaseEvent(QMouseEvent* event) override;
    // Ported from: itwinjs-core EventController 的 DOM "mouseout" 监听
    // （EventController.ts:27 → ToolAdmin.ts:841 onMouseLeave）——Qt 等价物即
    // QWidget::leaveEvent。
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    // Keyboard interaction (Task 14 bridge: Qt key events → ToolAdmin.addEvent).
    // Ported from: itwinjs-core EventController.ts DOM keydown/keyup listeners
    //               → ToolAdmin.addEvent (ToolAdmin.ts:792).
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    Viewport(QWidget* parent, dqBase::RefPtr<ViewState> view);

    // Initialization — creates RenderPipeline + Swapchain from native window handle
    void Init();

    // renderFrame() internal steps (← itwinjs-core Viewport.ts:2546).
    // (SetupFromView is public — see above; both renderFrame and tools sync via it.)
    void CreateScene();               // Step 10: build scene from tile trees
    void ValidateRenderPlan();        // Step 11: validate render plan
    void CollectDecorations();        // Step 12: collect decorations from decorators

    // ViewState
    dqBase::RefPtr<ViewState> m_view;
    ViewingSpace m_viewingSpace;  // transform chain for coordinate conversion

    // Event subscriptions (← itwinjs-core: attachToView/detachFromView)
    // RAII scope that automatically unsubscribes all listeners when cleared.
    dqBase::DqEventScope m_viewEventScope;

    int m_viewportId;
    bool m_initialized = false;
    bool m_needsInit = true;  // deferred until showEvent (native handle available)
    bool m_isShuttingDown = false;  // set by Shutdown(); makes RenderFrame/resizeEvent no-ops
    bool m_doContinuousRendering = false;  // ← Viewport.ts:1455 _doContinuousRendering
    bool m_inRenderFrame = false;   // RenderFrame 进行中（resizeEvent 重入守卫）

public:
    void* m_dbgInitHwnd = nullptr;  // TEMP-DIAG：Init 时绑定的原生窗口句柄
    int m_dbgPresentCount = 0;      // TEMP-DIAG：present 计数（黑块/最小化黑屏排查）
private:
    bool m_pendingResize = false;   // RenderFrame 中途收到的 resize（帧尾补处理）

    // Render pipeline (owns Swapchain, Driver, RenderSystem, Techniques)
    std::unique_ptr<dqRender::RenderPipeline> m_pipeline;

    // Render target (← IModelApp.renderSystem.createTarget(canvas))
    dqRender::RenderTarget* m_renderTarget = nullptr;

    // Change flags (← itwinjs-core _changeFlags)
    MutableChangeFlags m_changeFlags;

    // Validity flags (← itwinjs-core _controllerValid, _sceneValid, etc.)
    bool m_controllerValid = true;
    bool m_sceneValid = false;
    bool m_renderPlanValid = false;
    bool m_decorationsValid = false;
    bool m_redrawPending = true;
    bool m_selectionSetDirty = false;
    bool m_freezeScene = false;
    bool m_analysisFractionValid = true;   // Step 7
    bool m_timePointValid = true;          // Step 8

    // Scene (← itwinjs-core target.changeScene())
    dqRender::Scene m_scene;

    // Decorations (← itwinjs-core target.changeDecorations())
    dqRender::Decorations m_decorations;

    // 网格图形由 Viewport 持有（m_gridGraphic，见下）——参考经 DecorationsCache
    // （ViewState.ts:181 useCachedDecorations=true）+ TS GC；C++ 所有权在
    // Viewport（GridDecorator::UseCachedDecorations=false 的适配注释）。
    // Sky sphere graphic (create-once; itwinjs creates once + initWorldPos per
    // frame). Owned by the Viewport; freed in Shutdown. Caching avoids the
    // per-frame createSkyBox leak (GL resource exhaustion → glDrawElements crash).
    dqRender::RenderGraphic* m_skyGraphic = nullptr;

    // m_skyGraphic 的参数指纹——环境（渐变色/twoColor/zOffset/backgroundMapOn）
    // 变化时销毁重建。参考 EnvironmentDecorations.decorate 每帧 createSkyBox
    // （EnvironmentDecorations.ts:90-95），sky.params 始终反映当下环境；DanQing
    // 的缓存必须同样对参数敏感（否则首个视图的默认蓝渐变在示例改环境后残留）。
    std::optional<dqCommon::SkyGradient> m_skyGradientCache;
    float m_skyZOffsetCache = 0.0f;
    bool m_skyBackgroundMapOnCache = false;

    // Planar grid graphic（create-once + updatePlanarGridFrustum 原地更新——
    // m_skyGraphic 同款生命周期）。Owned by the Viewport; freed in Shutdown。
    // 无缓存时缩放/resize 每帧新建 GL buffer（卡顿 + bump arena 增长）。
    dqRender::RenderGraphic* m_gridGraphic = nullptr;

    // Highlight state (← itwinjs-core target.setHiliteSet/setFlashed)
    // The hilite feature-override LUTs live on the render target's batches
    // (§8.4: @internal FeatureOverrideLUT not exposed to PublicAPI); the
    // Viewport pushes the iModel HiliteSet via RenderTarget::setHiliteSet
    // from renderFrame step 6 when the selection set is dirty.
    uint32_t m_hiliteFeatureId = 0;  // currently hilited feature (0 = none)
    // 参考默认色 ColorDef.from(0x23, 0xbb, 0xfc)（Hilite.ts:53）。
    float m_hiliteColor[4] = {0x23 / 255.0f, 0xbb / 255.0f, 0xfc / 255.0f, 1.0f};

    // Test-only PickAtPoint result override (see SetPickResultForTest). Zero
    // means "no override" — PickAtPoint falls through to the GL path.
    // Authored: test affordance — no reference equivalent.
    // Only compiled in when dqApp / dqAppTest are built with DANQING_TESTING=1
    // (§8.4: implementation details must not be exposed to PublicAPI). The
    // seam vanishes from the SDK surface in production builds.
#ifdef DANQING_TESTING
    uint32_t m_pickResultOverride = 0;
#endif

    // Tile trees (Phase 2b)
    std::vector<dqRender::TileTree*> m_tileTrees;

    // Scaffold bridge references (AddTileTree trees wrapped in the faithful
    // owner/reference chain — see CreateScene step 3).
    std::vector<std::unique_ptr<class SimpleTileTreeReference>> m_scaffoldRefs;

    // Application-injected tile graphics providers.
    // <- itwinjs-core Viewport._tiledGraphicsProviders (Viewport.ts:532).
    std::vector<class TiledGraphicsProvider*> m_tiledGraphicsProviders;

    // Tile-load → scene-invalidation subscription.
    // ← itwinjs-core: Viewport.onRequestStateChanged = invalidateScene
    //   (Viewport.ts:3082-3084) + TileAdmin's onTileLoad → invalidateAllScenes
    //   (TileAdmin.ts:581-585) — a tile's content landing must rebuild the
    //   scene next frame so the loaded graphic gets drawn.
    dqBase::DqEventDisconnect m_onTileLoadDisconnect;

    // Render plan (← itwinjs-core RenderPlan)
    // Captures the complete rendering configuration from ViewState.
    dqRender::RenderPlan m_currentRenderPlan;

    // Feature symbology overrides (← itwinjs-core FeatureSymbology.Overrides)
    // Per-feature appearance overrides computed in RenderFrame Step 9.
    dqCommon::FeatureOverrides m_featureOverrides;
    bool m_featureOverridesDirty = true;

    // Per-model category visibility overrides (← itwinjs-core Viewport._perModelCategoryVisibility)
    // Ported from: itwinjs-core Viewport.ts line 464
    dqCommon::PerModelCategoryVisibilityOverrides m_perModelCategoryVisibility;

    // Flash state (← itwinjs-core Viewport._flashedId/_flashIntensity/
    // _lastFlashedElem/_flashUpdateTime)
    // Ported from: itwinjs-core Viewport.ts:473-476/2521-2543 — intensity ramps
    // 0→maxIntensity over flashDuration（默认 0.25s/1.0，FlashSettings.ts:69-85）
    // by REAL elapsed time; flash-off notifies one more frame.
    uint32_t m_flashedId = 0;           // currently flashed element (0 = none)
    uint32_t m_lastFlashedElem = 0;     // previous flash target（:476 语义）
    float m_flashIntensity = 0.0f;      // current flash intensity (0..maxIntensity)
    qint64 m_flashUpdateTimeMs = 0;     // flash start time（ms, steady clock）

    // Hover locate（EQUIVALENCE: 参考为事件驱动——EventController mousemove →
    // ToolAdmin → AccuSnap.onMotion → locateManager.doLocate → HitDetail ctor
    // 设 vp.flashedId（AccuSnap.ts:1095-1121、HitDetail.ts:333）；Chromium 按
    // 帧合并 motion。DanQing：mouseMoveEvent 记录 hover 点 + 置脏，renderFrame
    // 内帧合并 locate（PickAtPoint = doLocate 的既有 Step 3 等价物，
    // SelectionTool.cpp 同款登记）。发散 = 逐 motion 的 locate 频率（帧合并
    // 后与 Chromium 对齐）；验证法 = HoverMotionFlashesDecoration（sendEvent
    // 真 Qt 事件路径）。
    bool m_hoverDirty = false;
    int32_t m_hoverCssX = 0;
    int32_t m_hoverCssY = 0;

    // Animation (← itwinjs-core Viewport._animator)
    // Ported from: itwinjs-core Viewport.ts line 2537
    std::unique_ptr<Animator> m_animator;
    Animator* m_animatorRef = nullptr;  // 非拥有（ViewHandleArray 持有的句柄）

    // Decorations cache (← itwinjs-core Viewport._decorationCache)
    // Ported from: itwinjs-core DecorationsCache.ts
    DecorationsCache m_decorationCache;

    // --- 视图撤销/重做栈（Viewport.ts:3166-3169, 374） ---
    void finishUndoRedo();                                     // :3706-3712（无动画）
    std::vector<std::unique_ptr<ViewPose>> m_backStack;        // _backStack (:3168)
    std::vector<std::unique_ptr<ViewPose>> m_forwardStack;     // _forwardStack (:3167)
    std::unique_ptr<ViewPose> m_currentBaseline;               // _currentBaseline (:3169)
    // _lastPose (:3170) — the pose the last time this view was rendered（在
    // ValidateRenderPlan 逐帧保存，:3602；frustum-change 动画的起点姿态）。
    std::unique_ptr<ViewPose> m_lastPose;
    // :374 echo 守卫——参考在 doSetupFromView 内 raise onViewChanged 期间置位
    // （:2055-2057），使事件回调里的 saveViewUndo 被忽略；DanQing 的 SetupFromView
    // 不发 onViewChanged，当前无置位路径，保留语义位。
    // TODO: onViewChanged 事件（doSetupFromView 的 :2055-2057 置位）落地前此守卫
    //       恒假，属预期；事件落地时恢复参考语义。
    bool m_inViewChangedEvent = false;

    static int sNextViewportId;
};

}  // namespace dqApp
