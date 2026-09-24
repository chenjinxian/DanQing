// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render target implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Target.ts
//
// Bridges the Viewport to the RHI renderer. Owns the SceneCompositor,
// RenderCommands, and uniform state.
//
// Integration point: itwinjs's Target held the WebGL context and compositor.
// This version holds the Driver and compositor.
#pragma once

#include "FrameBuffer.h"
#include "Graphic.h"
#include "RenderCommands.h"
#include "RenderState.h"
#include "RenderSystemImpl.h"
#include "TargetGraphics.h"
#include "Uniforms.h"       // FrustumUniforms
#include "ShadowUniforms.h" // SolarShadowMap
#include "Batch.h"          // BatchState
#include "BranchStack.h"    // BranchStack
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/ContourDisplay.h>

#include <dqRender/CanvasDecoration.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class SceneCompositor;
class GLCanvasContext;

// ---------------------------------------------------------------------------
// TargetImpl — render target (viewport)
// Ported from: itwinjs-core Target.ts
// ---------------------------------------------------------------------------
class TargetImpl {
public:
    TargetImpl(RenderSystemImpl& system, Techniques& techniques, ViewRect const& rect);
    ~TargetImpl();

    TargetImpl(TargetImpl const&) = delete;
    TargetImpl& operator=(TargetImpl const&) = delete;

    /// Set the scene graphics to render.
    /// Ported from: itwinjs-core Target.changeScene()
    void setScene(std::unique_ptr<Graphic> scene);

    /// Set the Scene container for initForRender path.
    /// Ported from: itwinjs-core TargetGraphics.setScene()
    void setSceneContainer(Scene const* scene);

    /// Set decorations for the rendering pipeline.
    /// Ported from: itwinjs-core TargetGraphics.decorations
    void setDecorations(Decorations const* decorations);

    /// Get the TargetGraphics for initForRender path.
    TargetGraphics& getTargetGraphics() { return m_targetGraphics; }

    /// Draw a frame.
    /// Ported from: itwinjs-core Target.paintScene()
    void drawFrame();

    /// Draw the 2D canvas decorations into the composited frame (m_renderTarget),
    /// then re-blit to the output target so the drawn frame on screen includes
    /// them (drawFrame's endPaint blit happened before the decorations existed).
    /// Ported from: itwinjs-core Target.drawFrame → drawOverlayDecorations
    ///               (Target.ts:546-554, the call at :552; OnScreenTarget body
    ///               Target.ts:1408-1439). APPROVED DEVIATION (spec §2.4): the
    ///               reference rasterizes to an HTML 2D canvas; DanQing rasterizes
    ///               in-GL (GLCanvasContext) inside the frame.
    void drawCanvasDecorations(std::vector<CanvasDecoration> const& canvasDecs);

    /// Get the view rectangle.
    ViewRect const& getViewRect() const noexcept { return m_rect; }

    /// resize 标记的 FBO 失效（GL current 下的惰性销毁在 allocateFbo）。
    bool m_fboStale = false;

    /// Update the view rectangle on resize.
    /// Ported from: itwinjs-core Target.updateViewRect（Target.ts:1330-1336，
    /// renderRect.init）+ renderFrame step7 的 `resized → target.onResized()`
    /// （Viewport.ts:2604-2608 → Target.ts:1441-1443 → disposeFbo :315-330）。
    /// Qt 调用点（Viewport::resizeEvent）天然知晓尺寸已变，故 updateViewRect
    /// 与 onResized 合并：更新 m_rect，尺寸变化时销毁自身合成 FBO/pick
    /// （disposeFbo 语义），**惰性重建**——drawFrame 入口 allocateFbo 按当前
    /// m_rect 重建（Target.ts:751-766 assignDC + :290-313 allocateFbo）。
    void setViewRect(ViewRect const& rect);

    /// 惰性重建自身 FBO（Target.ts:290-313 allocateFbo + :751-766 assignDC
    /// 的绘制入口门）。drawFrame 首行调用——onResized 销毁后的下一帧在此重建。
    void allocateFbo();

    /// Get the render target handle (off-screen FBO for compositor).
    rhi::RenderTargetHandle getRenderTarget() const noexcept { return m_renderTarget; }

    /// Set the output render target for final blit (SwapChain surface).
    /// When set, endPaint() blits to this target instead of the default framebuffer.
    /// This enables Vulkan support (no "default framebuffer" concept).
    void setOutputTarget(rhi::RenderTargetHandle target) { m_outputTarget = target; }

    /// Get the driver.
    rhi::Driver& getDriver() noexcept { return m_system.getDriver(); }

    /// Get the render system (for texture binding etc.).
    /// Ported from: itwinjs-core System.instance (DanQing has no singleton; the
    ///  System is held by the Target).
    RenderSystemImpl& getSystem() noexcept { return m_system; }

    // --- Perf metric records (Target.ts:610-645 beginPerfMetricFrame/
    //     endPerfMetricFrame + beginPerfMetricRecord/endPerfMetricRecord) ---
    // These are the GLTimer instrumentation entry points: no-ops unless the
    // debugControl's resultsCallback is set (the GpuProfiler's "Profile GPU"
    // checkbox) and the backend supports timer queries — same gates as the
    // reference (isGLTimerSupported + resultsCallback).
    void beginPerfMetricFrame();                    // Target.ts:610-616
    void endPerfMetricFrame();                      // Target.ts:618-630
    void beginPerfMetricRecord(char const* operation);  // Target.ts:633-639
    void endPerfMetricRecord();                     // Target.ts:641-645

    /// Get the techniques registry.
    Techniques& getTechniques() noexcept { return m_techniques; }

    /// Set the hilited element-id set. Marks the set changed (the reference's
    /// desync(_hiliteSyncTarget)); each Batch updates its LUT lazily at draw
    /// time when its observed version is stale (FeatureOverrides.update).
    /// Ported from: itwinjs-core Target.setHiliteSet() (Target.ts:478-481)
    void setHiliteSet(uint32_t const* elementIds, size_t count);

    /// The current hilite-set version (incremented on every setHiliteSet).
    uint32_t getHiliteVersion() const noexcept { return m_hiliteVersion; }

    /// The current hilited element ids.
    std::vector<uint32_t> const& getHiliteElementIds() const noexcept { return m_hiliteElementIds; }

    /// Set the flashed element id + current flash intensity. The id change
    /// desyncs the per-batch LUTs (Flashed bit — reference updateFlashed runs
    /// when flashedId changed, FeatureOverrides.ts:420-426/338-375); the
    /// intensity is a per-frame uniform (u_flash_intensity) and does NOT
    /// dirty the LUT.
    /// Ported from: itwinjs-core Target.setFlashed() (Target.ts:482-489)
    void setFlashed(uint32_t elementId, float intensity);

    /// The currently-flashed element id (0 = none).
    uint32_t getFlashedId() const noexcept { return m_flashedId; }

    /// The current flash intensity (0..maxIntensity).
    float getFlashIntensity() const noexcept { return m_flashIntensity; }

    /// Set the hilite color (RGB).
    void setHiliteColor(float r, float g, float b);

    /// Get the scene compositor.
    SceneCompositor& getCompositor() { return *m_compositor; }

    /// Get the branch stack (transform/viewFlags stack).
    /// Ported from: itwinjs-core Target.branchStack
    BranchStack& getBranchStack() noexcept { return m_branchStack; }

    /// Get the batch state (feature batch stack).
    /// Ported from: itwinjs-core Target.batchState
    BatchState& getBatchState() noexcept { return m_batchState; }

    /// Read RGBA8888 color pixels from the color attachment.
    /// Ported from: itwinjs-core Target.readPixels（Pixel.Selector.Color 部分）
    bool readColorData(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                       std::vector<uint8_t>& pixels);

    /// Batch read pixels from the pick buffer into a caller-provided buffer.
    /// Ported from: itwinjs-core Target.readPixels()
    /// Renders the scene into an offscreen FBO with simplified view flags,
    /// then reads back the pixel data for feature selection.
    void readPixels(int32_t x, int32_t y, uint32_t width, uint32_t height,
                    uint32_t* outputBuffer, uint32_t bufferSize);

    /// 读 pick depthAndOrder 附件为线性深度分数（FeatureSymbology.ts
    /// readDepthAndOrder 的 CPU 等价）。Ported from: Target.readPixels 的
    /// geometryAndDistance 深度半边（Target.ts:768-825）。
    bool readPickDepth(int32_t x, int32_t y, uint32_t width, uint32_t height,
                       float* outFractions, uint32_t outCount);

    /// Get the pick render target.
    rhi::RenderTargetHandle getPickTarget() const noexcept { return m_pickTarget; }

    /// Set the light settings for rendering.
    /// Ported from: itwinjs-core RenderPlan.lights
    void setLightSettings(float const* sunDir, float sunIntensity, float const* ambientColor);

    float const* getSunDir() const noexcept { return m_sunDir; }
    float getSunIntensity() const noexcept { return m_sunIntensity; }
    float const* getAmbientColor() const noexcept { return m_ambientColor; }

    /// Set the background color for clearing.
    /// Ported from: itwinjs-core RenderPlan.backgroundColor
    void setBackgroundColor(float r, float g, float b, float a);

    float const* getBackgroundColor() const noexcept { return m_backgroundColor; }

    /// Change the render plan (view flags, 3D mode, frustum).
    /// Ported from: itwinjs-core Target.changeRenderPlan()
    void changeRenderPlan(ViewFlags const& viewFlags, bool is3d);

    /// Get the current view flags from the branch stack.
    /// Ported from: itwinjs-core Target.currentViewFlags
    ViewFlags const& getCurrentViewFlags() const { return m_branchStack.getCurrentViewFlags(); }

    /// Display normal maps (debug toggle; consumed by wantNormalMaps —
    /// SurfaceGeometry.ts :417). Default TRUE per Target.ts :158
    /// (`public displayNormalMaps = true`).
    bool displayNormalMaps = true;

    /// Render-command breakdown snapshot from the last populated frame
    /// (Target.getRenderCommands → RenderCommands.dump, RenderCommands.ts:742-771).
    /// Read by the diagnostics panel between frames.
    RenderCommands::CommandCount const& getLastCommandCount() const noexcept { return m_lastCommandCount; }

    /// Get the decorations state (BranchState for overlays).
    /// Ported from: itwinjs-core Target.decorationsState
    BranchState& getDecorationsState() { return m_decorationsState; }
    BranchState const& getDecorationsState() const { return m_decorationsState; }

    /// Check if this is a 3D target.
    bool is3d() const noexcept { return m_is3d; }

    /// Whether ambient occlusion is desired.
    /// Ported from: itwinjs-core Target.wantAmbientOcclusion
    bool wantAmbientOcclusion() const noexcept { return m_wantAO; }
    void setWantAmbientOcclusion(bool v) noexcept { m_wantAO = v; }

    /// Get the current contour display settings.
    /// Ported from: itwinjs-core Target.currentContours
    dqCommon::ContourDisplay const* getCurrentContours() const noexcept { return m_contours; }

    /// Set the contour display settings.
    /// Ported from: itwinjs-core Target (contour display from view state)
    void setContours(dqCommon::ContourDisplay const* contours) noexcept { m_contours = contours; }

    /// Get the current transform (from branch stack).
    /// Ported from: itwinjs-core Target.currentTransform
    std::array<float, 16> const& getCurrentTransform() const { return m_branchStack.getCurrentMv(); }

    /// The current branch's model (local-to-world) transform.
    /// Ported from: itwinjs-core Target.currentTransform (line 254)
    /// (reference returns currentBranch.transform — the model Transform that
    ///  BranchUniforms.update combines with the view matrix at draw time).
    dqGeom::Transform const& currentModelTransform() const noexcept
    {
        return m_branchStack.getTop().getLocalToWorld();
    }

    /// Get the RenderCommands.
    RenderCommands& getCommands() { return m_commands; }

    // --- Classifier/drape lifecycle ---
    // Ported from: itwinjs-core Target.changePlanarClassifiers/changeTextureDrapes

    /// Set the planar classifier for this target.
    void setPlanarClassifier(PlanarClassifier* classifier) noexcept { m_planarClassifier = classifier; }

    /// Get the planar classifier.
    PlanarClassifier* getPlanarClassifier() const noexcept { return m_planarClassifier; }

    // --- Animation state ---
    // Ported from: itwinjs-core Target.animationBranches

    /// Get the animation branches (node IDs for time-based filtering).
    std::vector<uint32_t> const& getAnimationBranches() const noexcept { return m_animationBranches; }
    void setAnimationBranches(std::vector<uint32_t> branches) { m_animationBranches = std::move(branches); }

    // --- Display feature flags ---
    // Ported from: itwinjs-core Target.wantThematicDisplay/wantAtmosphere

    /// Whether thematic display is desired.
    bool wantThematicDisplay() const noexcept { return m_wantThematicDisplay; }
    void setWantThematicDisplay(bool v) noexcept { m_wantThematicDisplay = v; }

    /// Whether atmosphere rendering is desired.
    bool wantAtmosphere() const noexcept { return m_wantAtmosphere; }
    void setWantAtmosphere(bool v) noexcept { m_wantAtmosphere = v; }

    // --- Device pixel ratio ---
    // Ported from: itwinjs-core Target.devicePixelRatio (line 1288)
    // Reference cascade: devicePixelRatioOverride -> renderSystem.options -> window.
    // dqRender surfaces the override + a settable ratio (host platform assigns the
    // actual window ratio at startup).
    float devicePixelRatio() const noexcept
    {
        return (m_devicePixelRatioOverride > 0.0f) ? m_devicePixelRatioOverride : m_devicePixelRatio;
    }
    void setDevicePixelRatio(float ratio) noexcept { m_devicePixelRatio = ratio; }
    void setDevicePixelRatioOverride(float override_) noexcept { m_devicePixelRatioOverride = override_; }

    // --- Clip volume ---
    // Ported from: itwinjs-core Target._clipVolume

    /// Get the current clip volume.
    ClipVolume* getClipVolume() const noexcept { return m_clipVolume; }
    void setClipVolume(ClipVolume* vol) noexcept { m_clipVolume = vol; }

    // --- Shadow map ---
    // Ported from: itwinjs-core Target.solarShadowMap

    /// Get the solar shadow map.
    SolarShadowMap const* getShadowMap() const noexcept { return m_shadowMap; }
    void setShadowMap(SolarShadowMap* map) noexcept { m_shadowMap = map; }

    // --- Read pixels state ---
    bool isReadPixelsInProgress() const noexcept { return m_isReadPixelsInProgress; }

    // --- Target uniforms aggregator ---
    // Ported from: itwinjs-core Target.uniforms
    TargetUniforms& getUniforms() noexcept { return m_uniforms; }
    TargetUniforms const& getUniforms() const noexcept { return m_uniforms; }

    // --- Frustum uniforms (forwarding to aggregator) ---
    // Ported from: itwinjs-core Target.uniforms.frustum
    FrustumUniforms& getFrustumUniforms() noexcept { return m_uniforms.frustum; }
    FrustumUniforms const& getFrustumUniforms() const noexcept { return m_uniforms.frustum; }

    /// Get the view matrix Transform (from frustum uniforms).
    /// Ported from: itwinjs-core Target.uniforms.frustum.viewMatrix
    dqGeom::Transform const& getViewMatrix() const noexcept { return m_uniforms.frustum.getViewMatrix(); }

private:
    bool glTimerActive() const noexcept;            // isGLTimerSupported + resultsCallback

    /// Begin paint — push FBO to framebuffer stack, set up render target.
    /// Ported from: itwinjs-core Target._beginPaint()
    void beginPaint();

    /// Populate RenderCommands by walking the scene graph.
    /// Ported from: itwinjs-core RenderCommands.initForRender()
    void populateCommandsFromScene();

    /// Run `func` with the decorations state pushed (initForRender's
    /// pushAndPopState(decorationsState) scope for overlay decorations).
    /// Ported from: itwinjs-core RenderCommands.pushAndPopState()
    template<typename Func>
    void pushAndPopDecorationsState(Func&& func)
    {
        m_branchStack.pushState(m_decorationsState);
        func();
        m_branchStack.pop();
    }

    /// Draw world and view overlay passes.
    /// Ported from: itwinjs-core Target.drawPass() for WorldOverlay/ViewOverlay
    void drawOverlays();

    /// End paint — pop FBO, blit to canvas.
    /// Ported from: itwinjs-core Target._endPaint()
    void endPaint();

    /// Blit the composited frame (m_renderTarget) to the output target / screen.
    /// Ported from: itwinjs-core Target._endPaint() (blit half — factored out so
    /// drawCanvasDecorations can re-blit after drawing 2D decorations, keeping
    /// endPaint and the post-decoration re-present on the same code path).
    void blitToOutput();

    void createPickBuffer();

    /// Begin readPixels — simplified view flags, repopulate commands.
    /// Ported from: itwinjs-core Target.beginReadPixels()
    void beginReadPixels();

    /// End readPixels — restore state.
    /// Ported from: itwinjs-core Target.endReadPixels()
    void endReadPixels();

    RenderSystemImpl& m_system;
    Techniques& m_techniques;
    ViewRect m_rect;
    rhi::RenderTargetHandle m_renderTarget;
    rhi::RenderTargetHandle m_pickTarget;  // R32UI pick buffer
    rhi::RenderTargetHandle m_outputTarget;  // SwapChain surface (set by RenderTarget)
    std::unique_ptr<Graphic> m_scene;
    RenderCommands::CommandCount m_lastCommandCount;  // diagnostics snapshot (drawFrame)
    TargetGraphics m_targetGraphics;  // Scene data for initForRender path

    // Scene graph traversal state (Ported from: itwinjs-core Target members)
    BranchStack m_branchStack;
    BatchState m_batchState;
    RenderCommands m_commands;

    std::unique_ptr<SceneCompositor> m_compositor;

    // Hilite set (element ids) + version. The LUTs themselves live on each
    // Batch (reference: per-batch FeatureOverrides); the version implements the
    // reference's hiliteSyncTarget desync/observer lazily at draw time.
    // Ported from: itwinjs-core Target._hilites + _hiliteSyncTarget
    //               (Target.ts:111/478-481, FeatureOverrides.ts:422-424)
    std::vector<uint32_t> m_hiliteElementIds;
    uint32_t m_hiliteVersion = 1;

    // Flashed element + intensity (hover). Id changes bump the same LUT
    // version (Flashed bit lives in the LUT); intensity travels as a uniform.
    // Ported from: itwinjs-core Target._flashedId/_flashed/_flashIntensity
    //               (Target.ts:482-489, FeatureOverrides.ts:338-375/420)
    uint32_t m_flashedId = 0;
    float m_flashIntensity = 0.0f;

    // Overlay render state (Ported from: itwinjs-core Target._overlayRenderState)
    // depthMask=false, blend=true, blendFunc=(ONE, ONE_MINUS_SRC_ALPHA)
    RenderState m_overlayRenderState;

    // Previously-applied render state (for diff-and-apply).
    // Ported from: itwinjs-core System._currentRenderState
    RenderState m_currentRenderState;

    // Light settings (passed from ViewState via RenderTarget)
    float m_sunDir[3] = {0.3f, 0.5f, 0.8f};
    float m_sunIntensity = 0.7f;
    float m_ambientColor[3] = {0.3f, 0.3f, 0.35f};

    // Background color (passed from ViewState via RenderTarget)
    float m_backgroundColor[4] = {0.2f, 0.2f, 0.2f, 1.0f};

    // Decorations state (Ported from: itwinjs-core Target.decorationsState)
    // Used when rendering view background and view/world overlays.
    BranchState m_decorationsState;

    // 3D flag (Ported from: itwinjs-core Target.is3d)
    bool m_is3d = true;

    // Ambient occlusion flag (Ported from: itwinjs-core Target.wantAmbientOcclusion)
    bool m_wantAO = false;

    // Contour display settings (Ported from: itwinjs-core Target.currentContours)
    dqCommon::ContourDisplay const* m_contours = nullptr;

    // readPixels state (Ported from: itwinjs-core Target members)
    bool m_isReadPixelsInProgress = false;
    BranchState m_savedBranchState;  // Saved before beginReadPixels, restored in endReadPixels

    // Planar classifier state (Ported from: itwinjs-core Target._planarClassifiers)
    PlanarClassifier* m_planarClassifier = nullptr;

    // Animation state (Ported from: itwinjs-core Target.animationBranches)
    // Stores animation node IDs for time-based filtering.
    std::vector<uint32_t> m_animationBranches;

    // Display feature flags (Ported from: itwinjs-core Target members)
    bool m_wantThematicDisplay = false;
    bool m_wantAtmosphere = false;
    float m_devicePixelRatio = 1.0f;          // host-assigned window ratio
    float m_devicePixelRatioOverride = 0.0f;  // >0 forces a specific ratio

    // Clip volume (Ported from: itwinjs-core Target._clipVolume)
    ClipVolume* m_clipVolume = nullptr;

    // Shadow map (Ported from: itwinjs-core Target.solarShadowMap)
    SolarShadowMap* m_shadowMap = nullptr;

    // Target uniforms aggregator (Ported from: itwinjs-core Target.uniforms)
    // Composes frustum/viewRect/lights/style/hilite/branch/batch.
    TargetUniforms m_uniforms;

    // 2D canvas decoration backend (lazy). Records CanvasContext strokes and
    // rasterizes them as GL_LINES at the end of the drawn frame.
    // Ported from: itwinjs-core OnScreenTarget._2dCanvas (Target.ts:1403-1407)
    //              — APPROVED DEVIATION (spec §2.4): GL rasterization backend
    //              instead of an HTML 2D canvas (the QPainter alien-widget backend
    //              hung the native WGL driver — 2x LiveKernelEvent 141 TDR).
    std::unique_ptr<GLCanvasContext> m_canvasContext;
};

END_DQ_RENDER_NAMESPACE
