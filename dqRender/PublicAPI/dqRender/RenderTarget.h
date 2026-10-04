// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderTarget abstract interface
//
// Ported from: itwinjs-core core/frontend/src/render/RenderTarget.ts
// Connects a Viewport to a graphics renderer.
#pragma once

#include "Export.h"
#include "Decorations.h"
#include "RenderGraphic.h"
#include "RenderMemory.h"
#include "RenderPlan.h"
#include "Scene.h"

#include <dqBase/DqEvent.h>
#include <dqCommon/FeatureOverrides.h>  // M-O(2) I10——overrideFeatureSymbology
#include <dqCommon/Image.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif
#include <dqGeom/Range3d.h>

#include <cstdint>
#include <set>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

class RenderSystem;

// Integer viewport rectangle.
// Ported from: itwinjs-core ViewRect
struct DQ_RENDER_EXPORT ViewRect {
    uint32_t left = 0;
    uint32_t top = 0;
    uint32_t right = 0;
    uint32_t bottom = 0;

    constexpr ViewRect() noexcept = default;
    constexpr ViewRect(uint32_t l, uint32_t t, uint32_t r, uint32_t b) noexcept
        : left(l), top(t), right(r), bottom(b)
    {
    }

    uint32_t width() const noexcept { return right - left; }
    uint32_t height() const noexcept { return bottom - top; }
    bool isNull() const noexcept { return left >= right || top >= bottom; }
    bool isValid() const noexcept { return !isNull(); }
};

// Abstract render target.
// Ported from: itwinjs-core RenderTarget.ts
//
// Connects a Viewport to the rendering pipeline. Receives scene, decorations,
// and render plan updates from the Viewport, then draws the frame.
class DQ_RENDER_EXPORT RenderTarget {
public:
    virtual ~RenderTarget() = default;

    // Get the render system.
    virtual RenderSystem& renderSystem() = 0;

    // Get the view rect.
    virtual ViewRect viewRect() const = 0;

    // --- Debug control (RenderTargetDebugControl family) ---------------------
    // Render-command breakdown from the last populated frame
    // (Target.getRenderCommands → RenderCommands.dump, RenderCommands.ts:742-771).
    // Default: empty (targets without command tracking — the off-screen/
    // pick targets report zeros, same as the reference's debugControl-less
    // targets).
    struct RenderCommandCount { uint32_t primitives = 0, batches = 0, branches = 0; };
    virtual RenderCommandCount getRenderCommands() const { return {}; }

    // Display normal maps (Target.ts:158 displayNormalMaps — the
    // wantNormalMaps gate; default true on concrete targets).
    virtual bool displayNormalMaps() const { return true; }
    virtual void setDisplayNormalMaps(bool /*on*/) {}

    // Record graphics memory consumed by this target.
    // Ported from: itwinjs-core RenderTarget.collectStatistics (RenderTarget.ts:140
    // — base class no-op; the WebGL target overrides with the compositor's
    // FBO/attachment accounting). DanQing's concrete targets override when the
    // compositor gains byte instrumentation; until then they report zero
    // (faithful to the reference default).
    virtual void collectStatistics(RenderMemory::Statistics& stats) const { (void)stats; }

    // Change the view rect.
    virtual void setViewRect(const ViewRect& rect) = 0;

    // Update the view rect if needed (e.g., on resize).
    // Returns true if dimensions changed.
    // ← RenderTarget.updateViewRect()
    virtual bool updateViewRect() = 0;

    // Called when the target is resized.
    // ← RenderTarget.onResized()
    virtual void onResized() {}

    // Update the scene to be rendered.
    // ← RenderTarget.changeScene(scene)
    virtual void changeScene(Scene& scene) = 0;

    // Update decorations.
    // ← RenderTarget.changeDecorations(decorations)
    virtual void changeDecorations(Decorations& decorations) = 0;

    // Update the render plan.
    // ← RenderTarget.changeRenderPlan(plan)
    virtual void changeRenderPlan(RenderPlan const& plan) { (void)plan; }

    // Render the current frame.
    // ← RenderTarget.drawFrame(elapsedMs)
    virtual void drawFrame(float elapsedMs = 0.0f) = 0;

    // Draw the 2D canvas decorations inside the GL frame, over the composited
    // scene — called per DRAWN frame (unconditionally, even for an empty list).
    // Ported from: itwinjs-core Target.ts:546-554 (drawFrame → the call at :552)
    //              + OnScreenTarget.drawOverlayDecorations (Target.ts:1408-1439:
    //              per entry save → position?translate → drawDecoration → restore).
    // APPROVED DEVIATION (预批准兜底，原 spec 2026-09-11-windowarea-look-design §2.4；
    // spec 已随 2026-09-24 历史文档清理删除，偏差登记以本注释为准): the reference
    // rasterizes into an HTML 2D canvas composited over the WebGL canvas; DanQing
    // rasterizes in-GL (the QPainter alien-widget backend hung the native WGL driver —
    // 2x LiveKernelEvent 141 TDR). Same CanvasContext API and same drawDecoration
    // bodies — only the rasterization backend changes.
    // Default no-op: targets without a 2D overlay backend ignore the call.
    virtual void drawCanvasDecorations(std::vector<CanvasDecoration> const& /*canvasDecs*/) {}

    // Set the light settings for rendering.
    // ← itwinjs-core: RenderPlan.lights
    virtual void setLightSettings(float const* sunDir, float sunIntensity,
                                   float const* ambientColor) = 0;

    // Read RGBA8888 color pixels from the target（itwinjs readPixels 的
    // Pixel.Selector.Color 语义；featureId 拾取回读另走 pick buffer 路径）。
    virtual bool readPixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                            std::vector<uint8_t>& pixels) = 0;

    // Render the pick view on demand (simplified view flags, translucent drawn
    // opaque, non-pickable decorations excluded) and read back feature ids for
    // a width×height rect. Returned values are ELEMENT ids (pick values are
    // translated through the batch map). Coordinates are device pixels with GL
    // bottom-up origin — the caller converts from CSS screen coordinates
    // (reference: Viewport.readPixels → cssPixelsToDevicePixels).
    // Ported from: itwinjs-core RenderTarget.readPixels(rect, selector,
    //               receiver, ...) — Pixel.Selector.Feature path
    //               (Viewport.ts:2778-2785 → Target.ts:768-825)
    virtual bool readPickData(int32_t /*x*/, int32_t /*y*/, uint32_t /*width*/,
                              uint32_t /*height*/, uint32_t* /*outIds*/,
                              uint32_t /*outCount*/)
    {
        return false;
    }

    // 同 readPickData 的 pick 渲染，但读 depthAndOrder 附件（附件 1）——返回每个
    // 像素的线性深度分数（0=近、1=远；参考 depthAndOrder 通道 decodeDepthRgb，
    // FeatureSymbology.ts readDepthAndOrder :395-401）。用于 pickDepthPoint
    // （Viewport.ts:3435）的深度锚定。
    virtual bool readPickDepth(int32_t /*x*/, int32_t /*y*/, uint32_t /*width*/,
                               uint32_t /*height*/, float* /*outFractions*/,
                               uint32_t /*outCount*/)
    {
        return false;
    }

    // Set the hilited element-id set. The per-batch feature-override LUTs pick
    // this up lazily at draw time (§8.4 — the @internal LUT types stay inside
    // dqRender; mirrors itwinjs where Target/FeatureOverrides own the LUTs).
    // Ported from: itwinjs-core Target.setHiliteSet(hilite) (Target.ts:478-481)
    virtual void setHiliteSet(uint32_t const* /*elementIds*/, size_t /*count*/) {}

    // Set the hilite color (RGB).
    virtual void setHiliteColor(float /*r*/, float /*g*/, float /*b*/) {}

    // Set the viewport's model-view and model-view-projection matrices.
    // These are the camera transform matrices that the compositor uses as
    // the initial branch stack state for rendering.
    // ← itwinjs-core: viewport.setViewingSpace() sets the transform on the target
    virtual void setViewportTransform(float const* /*mv16*/, float const* /*mvp16*/) {}

    // Set the device pixel ratio (Qt devicePixelRatioF equivalent — the canvas
    // decoration flush scales view-px to device-px by it; default 1.0).
    // Ported from: itwinjs-core Target.devicePixelRatio (the browser's
    // devicePixelRatio; DanQing's host supplies it).
    virtual void setDevicePixelRatio(float /*ratio*/) {}

    // Pre-render hook — fired before the actual draw call.
    // Ported from: itwinjs-core RenderTarget.onBeforeRender
    dqBase::DqEvent<> onBeforeRender;

    // Set the flashed element and intensity.
    // Ported from: itwinjs-core Target.setFlashed()
    virtual void setFlashed(uint32_t /*elementId*/, float /*intensity*/) {}

    // Set the invisible subCategory-id set (M-N(1)——category visibility). The
    // per-batch feature-override LUTs re-apply visibility lazily at draw time
    // (setHiliteSet 同构). The reference's inverse set (_visibleSubCategories,
    // FeatureSymbology.ts:134-143) is built from the categorySelector +
    // iModel.subcategories RPC table; the dump-replay plane has no such table,
    // so the set arrives pre-expanded by subCategoryId (each feature's
    // subCategoryId is in the packed feature table).
    virtual void setInvisibleSubCategories(std::set<uint64_t> const& /*invisibleSubCategories*/) {}

    // Set the viewport's current feature symbology overrides (M-O(2) I10). The
    // per-batch LUTs recompute per-feature appearance lazily at draw time when
    // the version changed (PushBatch drives it).
    // Ported from: itwinjs-core Target.overrideFeatureSymbology(ovr)
    // (Target.ts:534-536——Viewport.ts:2643 Step 9 重建后传入).
    // EQUIVALENCE: 参考源=webgl FeatureOverrides.update :412-441 的 ovrsUpdated
    // 判据（target.currentFeatureSymbologyOverrides 对象同一性——参考 Step 9 每次
    // new Overrides 对象，:2642）；发散=DanQing 的 overrides 为持久成员就地改写，
    // 以版本计数承载"对象换了"语义；验证法=PickDumpScene
    // FeatureOverrideProviderRestylesHitElement 像素锁。
    virtual void overrideFeatureSymbology(dqCommon::FeatureOverrides const* /*ovrs*/) {}
    virtual dqCommon::FeatureOverrides const* getFeatureOverrides() const { return nullptr; }
    virtual uint32_t getFeatureOverridesVersion() const { return 0; }
};

END_DQ_RENDER_NAMESPACE
