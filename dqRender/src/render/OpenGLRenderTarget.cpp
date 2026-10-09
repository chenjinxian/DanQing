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
#include <cmath>
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

// Pick 视图的数据源喂入/撤销：把当前 Scene 装进 TargetGraphics（pick 命令
// 装配的唯一场景口），pick 回读结束后撤销。参考语义——Target.readPixelsFromFbo
// → beginReadPixels → RenderCommands.initForReadPixels(this.graphics)
//（Target.ts:917，drawForReadPixels :933-938）每次拾取从 TargetGraphics 重画
// **当前场景**；容器在参考里由 Target.changeScene → graphics.changeScene
//（Target.ts:426-427 → TargetGraphics.ts:31-35）于 changeScene 时镜像。此前
// DanQing 的 Scene 只进正常帧合并（drawFrame → setScene），从不进
// TargetGraphics（TargetGraphics::setScene 全仓零生产调用方——
// ported-but-uncalled，M-I(2) 清偿）→ pick 命令 0 → PickAtPoint 恒 0 →
// 点选恒 processMiss。
//
// EQUIVALENCE（§11.10）：DanQing 在 pick 入口喂容器、pick 尾撤销，不在
// changeScene 时喂。发散=喂入时点（pick 作用域 vs changeScene 持续镜像）；
// 未发现的发散=喂入后 initForReadPixels 读到的是同一 Scene 指针的同一图形
// 列表（Viewport::m_scene 成员地址稳定，pick 发生在帧间，列表存活至下一
// createScene——与参考 this.graphics 的生存期语义一致），验证法=PickDumpScene
// 三像素判据（瓦 pick/点选/高亮）+ 既有 PickHiliteSelection 6 项（装饰 pick
// 不受影响）。不取 changeScene 时（持续）喂的原因：TargetImpl::
// populateCommandsFromScene（TargetImpl.cpp:340）以"容器非空"为正常帧命令
// 装配的分流判据——容器一旦非空，dqApp 生产绘制主路径即从 drawFrame 合并树
// 改道 initForRender(container)（首版实测：首次拾取后正常帧只重画场景子集
// ——高亮清选不复原的直接根因），故容器必须只在 pick 作用域内非空。
// 回流登记：若后续把正常帧装配统一切到容器路径（参考形态），本喂入点应随之
// 移回 changeScene 并去掉撤销。
void OpenGLRenderTarget::feedPickScene()
{
    if (m_impl)
        m_impl->setSceneContainer(m_scene);
}

// pick 回读尾——撤销容器（见 feedPickScene 的 EQUIVALENCE 登记：容器非空会
// 改变正常帧命令装配的分流）。
void OpenGLRenderTarget::endPickScene()
{
    if (m_impl)
        m_impl->setSceneContainer(nullptr);
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
    // (Target.ts:520-533):
    //   let vf = plan.viewFlags;
    //   if (!plan.is3d) vf = vf.withRenderMode(RenderMode.Wireframe);
    //   ... (AO gate :524-530 — DanQing's plan has no `ao` payload yet; AO stays
    //        on its setWantAmbientOcclusion path until that pass)
    //   this.uniforms.branch.changeRenderPlan(vf, plan.is3d, plan.hline, plan.contours);
    // Previously only bgColor/frustum/uniforms were forwarded — the branch stack
    // stayed at its BranchState default (renderMode=Wireframe), so any viewFlags
    // consumer (wantNormalMaps' SmoothShade gate) never saw the real plan flags.
    // M-I(4)：plan.hline 接线（RenderPlan.ts:58/:124——3d 时 = display style 的
    // hiddenLineSettings；此前 hline 段从未搬运 → EdgeSettings 恒默认、
    // hline.visible.color=0 黑边覆盖从未到达边绘制）。
    dqCommon::ViewFlagsProperties vf = plan.viewFlags.Properties();
    if (!plan.is3d)
        vf.renderMode = dqCommon::RenderMode::Wireframe;
    // View clip（M-P P-C）——Target.ts:519（updateViewClip 在 branch.
    // changeRenderPlan :533 之前）。
    m_impl->updateViewClip(plan.clip, plan.clipStyle);
    // M-S S-c：plan.thematic 段透传（Target.plan.thematic 的读取面——
    // TargetImpl 存管，uniforms.thematic.update 在本函数尾段消费）。
    m_impl->changeRenderPlan(vf, plan.is3d,
                             plan.hline.has_value() ? &*plan.hline : nullptr,
                             plan.thematic.has_value() ? &*plan.thematic : nullptr);

    // Frustum uniforms FIRST — the projection/view pair (u_proj/u_mv) comes from
    // FrustumUniforms.changeFrustum (lookIn + ortho(0,depth) / frustum()), the
    // reference's ONLY projection source. The worldToNdc linear map (m22>0)
    // inverts depth under LEQUAL (farthest surface wins — the glTF cube's bottom
    // face rendered on top in standard views, 2026-09-16 取证见
    // build/gltf-mirror-forensics.md).
    // Ported from: itwinjs-core Target.changeRenderPlan (Target.ts:534):
    //   this.changeFrustum(plan.frustum, plan.fraction, plan.is3d);
    m_impl->getFrustumUniforms().changeFrustum(plan.frustum, plan.fraction, plan.is3d);

    // Thematic uniforms — Ported from: itwinjs-core Target.changeRenderPlan
    //（Target.ts:537——`uniforms.thematic.update(this)`；序：changeFrustum 之
    // 后、updateRenderPlan 之前[参考同序——Slope 轴/HillShade 太阳向依赖新
    // 视矩阵]）。M-S S-c。
    m_impl->getUniforms().thematic.update(*m_impl);
    // 取证探针（M-S S-d 断链定位所加——[THM]；§13.1 族登记保留）。
    static bool const s_thmTrace = getenv("DANQING_THM_TRACE") != nullptr;
    if (s_thmTrace) {
        auto const* pt = m_impl->getPlanThematic();
        printf("[THM] changeRenderPlan: plan.thematic=%s wantThematic=%d gradTex=%d vf.thematicDisplay=%d\n",
               pt ? "set" : "null", (int)m_impl->wantThematicDisplay(),
               (int)(m_impl->getUniforms().thematic.getGradientTexture() != rhi::TextureHandle{}),
               (int)m_impl->getCurrentViewFlags().thematicDisplay);
        fflush(stdout);
    }

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

    // Monochrome mode (M-O(1))：参考 Target 持整个 plan（Monochrome.ts:48 的
    // u_mixMonoColor graphic uniform 读 target.plan.monochromeMode）；DanQing
    // 的 changeRenderPlan 是分解式签名——monochromeMode 单独随 plan 转发。
    m_impl->setMonochromeMode(plan.monochromeMode);
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
        // vf 继承（M-S S-e 双栈统一修复）：参考无此 push——其根态经
        // changeRenderPlan 携 plan vf（BranchUniforms._stack 单栈，
        // BranchUniforms.ts:50 + Target.ts:533）；DanQing 本 push 仅为
        // draw-time 视口变换适配面 → vf 必须继承栈根（=plan vf），不得以
        // 默认 vf 覆写——原 `ViewFlagsProperties defaultFlags; stack.push(...)`
        // 形态把 plan 的 thematicDisplay 等位全掩掉（ThematicDisplayE2E 全族
        // 绘制期 vf.thematic=0 的实锤根因，[THM-DRAW] 探针取证）。
        stack.pushTransform(mv, mvp);
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
    feedPickScene();
    m_impl->readPixels(x, y, width, height, outIds, outCount);
    endPickScene();
    return true;
}

// depthAndOrder 附件读回（附件 1）——pickDepthPoint 的深度半边。
// Ported from: itwinjs-core Target.readPixels geometryAndDistance（Target.ts:768-825）。
bool OpenGLRenderTarget::readPickDepth(int32_t x, int32_t y, uint32_t width, uint32_t height,
                                       float* outFractions, uint32_t outCount)
{
    if (!m_impl || !outFractions || outCount < width * height)
        return false;
    feedPickScene();
    bool const ok = m_impl->readPickDepth(x, y, width, height, outFractions, outCount);
    endPickScene();
    return ok;
}

void OpenGLRenderTarget::setHiliteSet(uint32_t const* elementIds, size_t count)
{
    if (m_impl)
        m_impl->setHiliteSet(elementIds, count);
}

// M-N(1)：subCategory 可见性（TargetImpl 惰性版本链）。
void OpenGLRenderTarget::setInvisibleSubCategories(std::set<uint64_t> const& invisibleSubCategories)
{
    if (m_impl)
        m_impl->setInvisibleSubCategories(invisibleSubCategories);
}

// M-O(2) I10——viewport overrides 交付（桥接 → TargetImpl）。
void OpenGLRenderTarget::overrideFeatureSymbology(dqCommon::FeatureOverrides const* ovrs)
{
    if (m_impl)
        m_impl->overrideFeatureSymbology(ovrs);
}

dqCommon::FeatureOverrides const* OpenGLRenderTarget::getFeatureOverrides() const
{
    return m_impl ? m_impl->getFeatureOverrides() : nullptr;
}

uint32_t OpenGLRenderTarget::getFeatureOverridesVersion() const
{
    return m_impl ? m_impl->getFeatureOverridesVersion() : 0;
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
