// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGLRenderTarget implementation
// Ported from: filament filament/src/RenderTarget.cpp + itwinjs-core RenderTarget.ts
//
// Bridges public RenderTarget → internal TargetImpl.
// Converts public Scene/Decorations to internal Graphic tree for SceneCompositor.
#include "OpenGLRenderTarget.h"
#include "OpenGLRenderSystem.h"
#include "RenderGraphicAdapter.h"
#include "TargetImpl.h"
#include "SceneCompositorImpl.h"
#include "dqRender/RenderPlan.h"

#include <dqGeom/Matrix4d.h>

#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

OpenGLRenderTarget::OpenGLRenderTarget(OpenGLRenderSystem& system, std::unique_ptr<TargetImpl> impl)
    : m_system(system), m_impl(std::move(impl))
{
}

RenderSystem& OpenGLRenderTarget::renderSystem()
{
    return m_system;
}

// Debug control routing (TargetImpl's per-frame snapshot + toggle).
OpenGLRenderTarget::RenderCommandCount OpenGLRenderTarget::getRenderCommands() const
{
    auto const& c = m_impl->getLastCommandCount();
    return { c.primitives, c.batches, c.branches };
}

bool OpenGLRenderTarget::displayNormalMaps() const
{
    return m_impl->displayNormalMaps;
}

void OpenGLRenderTarget::setDisplayNormalMaps(bool on)
{
    m_impl->displayNormalMaps = on;
}

ViewRect OpenGLRenderTarget::viewRect() const
{
    return m_impl->getViewRect();
}

void OpenGLRenderTarget::setViewRect(const ViewRect& rect)
{
    m_impl->setViewRect(rect);
}

bool OpenGLRenderTarget::updateViewRect()
{
    return false;
}

void OpenGLRenderTarget::changeScene(Scene& scene)
{
    // Store the scene for later merging with decorations in drawFrame().
    m_scene = &scene;
}

void OpenGLRenderTarget::changeDecorations(Decorations& decorations)
{
    // Store decorations for merging with scene in drawFrame().
    m_decorations = &decorations;
}

void OpenGLRenderTarget::changeRenderPlan(RenderPlan const& plan)
{
    // Pass background color from RenderPlan to TargetImpl
    // ← itwinjs-core: target.changeRenderPlan(plan)
    uint32_t bgColor = plan.backgroundColor;
    float r = static_cast<float>((bgColor >> 0) & 0xFF) / 255.0f;
    float g = static_cast<float>((bgColor >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>((bgColor >> 16) & 0xFF) / 255.0f;
    float a = static_cast<float>((bgColor >> 24) & 0xFF) / 255.0f;
    m_impl->setBackgroundColor(r, g, b, a);

    // View flags → branch stack (bottom BranchState) — BEFORE changeFrustum,
    // matching the reference order. Ported from: itwinjs-core Target.changeRenderPlan
    // (Target.ts:520-532):
    //   let vf = plan.viewFlags;
    //   if (!plan.is3d) vf = vf.withRenderMode(RenderMode.Wireframe);
    //   ... (AO gate :524-530 — DanQing's plan has no `ao` payload yet; AO stays
    //        on its setWantAmbientOcclusion path until that pass)
    //   this.uniforms.branch.changeRenderPlan(vf, plan.is3d, plan.hline, plan.contours);
    // Previously only bgColor/frustum/uniforms were forwarded — the branch stack
    // stayed at its BranchState default (renderMode=Wireframe), so any viewFlags
    // consumer (wantNormalMaps' SmoothShade gate) never saw the real plan flags.
    dqCommon::ViewFlagsProperties vf = plan.viewFlags.Properties();
    if (!plan.is3d)
        vf.renderMode = dqCommon::RenderMode::Wireframe;
    m_impl->changeRenderPlan(vf, plan.is3d);

    // Frustum uniforms FIRST — the projection/view pair (u_proj/u_mv) comes from
    // FrustumUniforms.changeFrustum (lookIn + ortho(0,depth) / frustum()), the
    // reference's ONLY projection source. The worldToNdc linear map (m22>0)
    // inverts depth under LEQUAL (farthest surface wins — the glTF cube's bottom
    // face rendered on top in standard views, 2026-09-16 取证见
    // build/gltf-mirror-forensics.md).
    // Ported from: itwinjs-core Target.changeRenderPlan (Target.ts:534):
    //   this.changeFrustum(plan.frustum, plan.fraction, plan.is3d);
    m_impl->getFrustumUniforms().changeFrustum(plan.frustum, plan.fraction, plan.is3d);

    // Update target uniforms from the plan.
    // ← itwinjs-core Target.changeRenderPlan (Target.ts:543):
    //   this.uniforms.updateRenderPlan(plan);  // NB: after changeFrustum()
    // — style + lights (u_lightSettings[16]) + sun direction. Previously only
    // the background color was forwarded, so LightingUniforms stayed at its
    // all-zero initial state: with surface lighting enabled the light array
    // uploaded zeros (sun 0, ambient 0) and every lit surface went black.
    // (This was the "cube-not-lit" lighting gap of the 2026-07-25 blank-connection
    //  parity work; fixed here by forwarding the full render plan.)
    m_impl->getUniforms().updateRenderPlan(plan);
}

void OpenGLRenderTarget::setLightSettings(float const* sunDir, float sunIntensity,
                                           float const* ambientColor)
{
    // Forward to TargetImpl for use by SceneCompositor
    m_impl->setLightSettings(sunDir, sunIntensity, ambientColor);
}

void OpenGLRenderTarget::drawFrame(float elapsedMs)
{
    // Merge scene and decorations into a single internal Graphic tree,
    // then set it on TargetImpl for SceneCompositor rendering.
    //
    // ← itwinjs-core: target.drawFrame() processes scene + decorations together
    //
    // The merged tree contains:
    //   - Scene foreground (tile-based graphics)
    //   - Decorations normal (decorator graphics drawn with scene)

    auto mergedScene = std::make_unique<GraphicsArray>();

    // add scene foreground graphics
    if (m_scene) {
        for (auto* g : m_scene->foreground) {
            if (g) {
                mergedScene->add(std::make_unique<RenderGraphicAdapter>(g));
            }
        }
    }

    // add decoration normal graphics (drawn with scene, opaque/translucent passes)
    if (m_decorations) {
        for (auto* g : m_decorations->normal) {
            if (g) mergedScene->add(std::make_unique<RenderGraphicAdapter>(g));
        }
    }

    // Add the skybox decoration (← itwinjs-core: decorations.skyBox). It routes
    // to RenderPass::SkyBox via its geometry's getPass() == Pass::SkyBox.
    if (m_decorations && m_decorations->skyBox) {
        mergedScene->add(std::make_unique<RenderGraphicAdapter>(m_decorations->skyBox));
    }

    // Set the merged scene on TargetImpl
    m_impl->setScene(std::move(mergedScene));

    // Set the viewport transform on the compositor's branch stack.
    // This establishes the camera MV/MVP as the initial transform,
    // so all scene geometry (grid, tiles, etc.) is rendered in camera space.
    // ← itwinjs-core: the viewport's viewing transform is applied to the target
    size_t stackDepthBefore = 0;
    if (m_hasViewportTransform) {
        auto& stack = m_impl->getCompositor().getBranchStack();
        stackDepthBefore = stack.getDepth();
        // Push the changeFrustum-consistent pair (ef247b8 语义) — assembled AT
        // DRAW TIME from FrustumUniforms. The m_viewportMv/Mvp snapshots were
        // taken at setViewportTransform() time and go stale when
        // changeRenderPlan→changeFrustum runs afterwards (the offscreen tests
        // call transform-then-plan; the snapshot then holds the pre-changeFrustum
        // identity pair and u_mv/u_mvp lose the view+projection — the second
        // root cause of the 13 legacy dqRenderTest failures, 2026-09-23).
        // Reference: BranchUniforms.update computes u_mv = view·model at draw
        // time (BranchUniforms.ts:215-226) — no snapshot mechanism exists there.
        auto const& vu = m_impl->getFrustumUniforms();
        auto const& view = vu.getViewMatrix();
        Matrix4d const& proj = vu.getProjectionMatrix();
        auto const& r = view.matrix.coffs;
        Matrix4d const viewRow = Matrix4d::CreateRowValues(
            r[0], r[1], r[2], view.origin.x,
            r[3], r[4], r[5], view.origin.y,
            r[6], r[7], r[8], view.origin.z,
            0.0, 0.0, 0.0, 1.0);
        Matrix4d const mvpRow = proj.MultiplyMatrixMatrix(viewRow);
        float mv[16] = {
            static_cast<float>(r[0]), static_cast<float>(r[3]), static_cast<float>(r[6]), 0.0f,
            static_cast<float>(r[1]), static_cast<float>(r[4]), static_cast<float>(r[7]), 0.0f,
            static_cast<float>(r[2]), static_cast<float>(r[5]), static_cast<float>(r[8]), 0.0f,
            static_cast<float>(view.origin.x), static_cast<float>(view.origin.y),
            static_cast<float>(view.origin.z), 1.0f};
        float mvp[16];
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                mvp[col * 4 + row] = static_cast<float>(mvpRow.at(row, col));
        dqCommon::ViewFlagsProperties defaultFlags;
        stack.push(mv, mvp, defaultFlags);
    }

    // Propagate decorations to TargetGraphics so the single-tree render path
    // (populateCommandsFromScene) can process overlay decorations. (normal and
    // skyBox are merged into mergedScene above; worldOverlay/viewOverlay are not,
    // so they must reach TargetGraphics for the overlay-decoration handling.)
    m_impl->setDecorations(m_decorations);

    // Delegate to TargetImpl::drawFrame() which drives the SceneCompositor
    (void)elapsedMs;
    m_impl->drawFrame();

    // Unwind to the pre-push depth: commands that pushed without popping (e.g.
    // overlay decorations under the stack-driven overlay pass) must not leak
    // branch states into the next frame.
    if (m_hasViewportTransform)
        m_impl->getCompositor().getBranchStack().unwindToDepth(stackDepthBefore);
}

void OpenGLRenderTarget::drawCanvasDecorations(std::vector<CanvasDecoration> const& canvasDecs)
{
    // Forward to TargetImpl (records + rasterizes the strokes into the composited
    // frame, then re-blits to the output target). ← Target.ts:552 (per drawn frame).
    if (m_impl)
        m_impl->drawCanvasDecorations(canvasDecs);
}

bool OpenGLRenderTarget::readPixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                    std::vector<uint8_t>& pixels)
{
    // RGBA8888 color readback（itwinjs target.readPixels 的 Pixel.Selector.Color
    // 语义；featureId 回读走 readPickData，见 TargetImpl::readPickData）
    if (!m_impl)
        return false;
    // ← Target.ts:990 beginPerfMetricRecord("Read Pixels")（GPU profiler 置尾标签）
    m_impl->beginPerfMetricRecord("Read Pixels");
    bool const result = m_impl->readColorData(x, y, width, height, pixels);
    m_impl->endPerfMetricRecord();
    return result;
}

bool OpenGLRenderTarget::readPickData(int32_t x, int32_t y, uint32_t width, uint32_t height,
                                      uint32_t* outIds, uint32_t outCount)
{
    // featureId 拾取回读（itwinjs readPixels 的 Pixel.Selector.Feature 语义；
    // RGBA 颜色回读走 readPixels 上方重载）——TargetImpl::readPixels 渲染
    // pick 视图（drawForPick）后从 R32UI 附件回读并经 BatchState 反查 elementId。
    // Ported from: itwinjs-core Target.readPixels(rect, selector, receiver)
    //               (Target.ts:768-825)
    if (!m_impl || !outIds || outCount == 0 || width == 0 || height == 0
        || outCount < width * height)
        return false;
    m_impl->readPixels(x, y, width, height, outIds, outCount);
    return true;
}

// depthAndOrder 附件读回（附件 1）——pickDepthPoint 的深度半边。
// Ported from: itwinjs-core Target.readPixels geometryAndDistance（Target.ts:768-825）。
bool OpenGLRenderTarget::readPickDepth(int32_t x, int32_t y, uint32_t width, uint32_t height,
                                       float* outFractions, uint32_t outCount)
{
    if (!m_impl || !outFractions || outCount < width * height)
        return false;
    return m_impl->readPickDepth(x, y, width, height, outFractions, outCount);
}

void OpenGLRenderTarget::setHiliteSet(uint32_t const* elementIds, size_t count)
{
    if (m_impl)
        m_impl->setHiliteSet(elementIds, count);
}

void OpenGLRenderTarget::setFlashed(uint32_t elementId, float intensity)
{
    // Ported from: itwinjs-core Target.setFlashed() (Target.ts:482-489)
    if (m_impl)
        m_impl->setFlashed(elementId, intensity);
}

void OpenGLRenderTarget::setHiliteColor(float r, float g, float b)
{
    if (m_impl)
        m_impl->setHiliteColor(r, g, b);
}

void OpenGLRenderTarget::setViewportTransform(float const* /*mv16*/, float const* /*mvp16*/)
{
    // Ported from: itwinjs-core Target.changeRenderPlan (Target.ts:534) — the
    // reference's ONLY u_proj/u_mv source is FrustumUniforms.changeFrustum
    // (lookIn + ortho(0,depth) / frustum()), kept fresh via changeRenderPlan.
    // The (mv16, worldToNdc) pair this entry used to derive the projection from
    // is a linear box→NDC map whose m22 is POSITIVE — under LEQUAL it inverts
    // depth (farthest surface wins): the glTF cube rendered its BOTTOM face on
    // top in standard ortho views, presenting the texture u-mirrored
    // (2026-09-16, build/gltf-mirror-forensics.md).
    //
    // drawFrame pushes the changeFrustum-consistent pair (mv = FrustumUniforms
    // view, mvp = projection · view) assembled AT DRAW TIME from FrustumUniforms
    // — no snapshot here (a snapshot goes stale when changeRenderPlan runs
    // after this call, the transform-then-plan path of the offscreen tests;
    // reference BranchUniforms.update computes u_mv = view·model at draw time,
    // BranchUniforms.ts:215-226).
    m_hasViewportTransform = true;
}

void OpenGLRenderTarget::setDevicePixelRatio(float ratio)
{
    // ← Target.devicePixelRatio (Viewport.ts:2099 pixelsFromInches' input) —
    //   canvas-decoration flush scales view-px to device-px by it.
    if (m_impl)
        m_impl->setDevicePixelRatio(ratio);
}

END_DQ_RENDER_NAMESPACE
