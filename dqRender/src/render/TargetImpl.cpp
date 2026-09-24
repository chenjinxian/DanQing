// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render target implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Target.ts
//
// Bridges public RenderTarget → internal TargetImpl.
// Converts public Scene/Decorations to internal Graphic tree for SceneCompositor.
#include "TargetImpl.h"
#include "SceneCompositorImpl.h"
#include "TechniqueImpl.h"
#include "GLCanvasContext.h"
#include "RenderGraphicAdapter.h"
#include "gl/GL.h"

#include <dqCommon/PackedFeatureTable.h>

#include <cstdio>
#include <cstdlib>
#include <vector>


BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Constructor
// Ported from: itwinjs-core Target constructor
// ---------------------------------------------------------------------------
TargetImpl::TargetImpl(RenderSystemImpl& system, Techniques& techniques, ViewRect const& rect)
    : m_system(system)
    , m_techniques(techniques)
    , m_rect(rect)
    , m_commands(*this, m_branchStack, m_batchState)
    , m_compositor(std::make_unique<SceneCompositor>(*this, techniques))
    , m_decorationsState(BranchState::createForDecorations())
{
    // Create a render target for this viewport
    m_renderTarget = m_system.getDriver().createRenderTarget(
        rhi::TargetBufferFlags::ALL, rect.width(), rect.height(), 1, 1);

    // Pick buffer 不在构造期创建——allocateFbo 的 `!m_pickTarget` 门惰性建
    // R32UI 版本（GL current 下）。构造期创建的 legacy RGBA8 pick target 会
    // 让 R32UI 请求永远被"已存在"短路（拾取 saga：uint 输出/整数回读对 RGBA8
    // 附件 = GL_INVALID_OPERATION → pick 恒 0）。

    // Configure overlay render state
    // Ported from: itwinjs-core Target._overlayRenderState
    // depthMask=false, blend=true, blendFunc=(ONE, ONE_MINUS_SRC_ALPHA)
    m_overlayRenderState.flags.depthMask = false;
    m_overlayRenderState.flags.blend = true;
    m_overlayRenderState.blend.setBlendFunc(
        GL::BlendFactor::One,
        GL::BlendFactor::OneMinusSrcAlpha
    );
}

void TargetImpl::createPickBuffer()
{
    // 已废弃的构造期入口（见构造函数注释）——保留空壳防外部调用点链接失败；
    // pick target 由 allocateFbo 惰性创建（R32UI + DEPTH24）。
}

// Ported from: itwinjs-core Target.updateViewRect（Target.ts:1330-1336）+
// OnScreenTarget.onResized → disposeFbo（Target.ts:1441-1443, 315-330）。
// resize 时：更新 renderRect；尺寸变化标记 FBO 失效（惰性销毁+重建都在下一帧
// drawFrame 入口 allocateFbo——参考 disposeFbo 在浏览器保证 GL 上下文 current
// 的前提下同步调 glDelete；DanQing 的 resizeEvent 在 Qt 主线程、GL 上下文未
// current，此处直接销毁是 UB——实测驱动内核死锁（进程不可 kill）或静默
// 失效（最大化后 Grid 消失：GL 资源已坏但句柄残留）。GL 操作必须全部在
// drawFrame（swapchain->acquire 已 makeCurrent）内进行。
void TargetImpl::setViewRect(ViewRect const& rect)
{
    if (rect.width() == m_rect.width() && rect.height() == m_rect.height())
        return;  // 尺寸未变（Target.ts:1335 的 changed 判定 false 半边）
    m_rect = rect;        // renderRect.init(0, 0, w, h)（:1334）
    m_fboStale = true;    // disposeFbo 语义——销毁推迟到 allocateFbo（GL current）
}

// Ported from: itwinjs-core Target.allocateFbo（Target.ts:290-313）+
// assignDC（:751-766）——绘制入口的惰性重建门。drawFrame 首行调用；
// setViewRect 标记失效后的下一帧在此销毁旧 FBO（GL 上下文已 current——
// 参考 :758-760 的 rect.width < 1 早退同款守卫）并按当前 m_rect 重建。
void TargetImpl::allocateFbo()
{
    if (m_rect.width() < 1 || m_rect.height() < 1)
        return;
    auto& driver = m_system.getDriver();
    if (m_fboStale) {
        // disposeFbo（:315-330）——GL current 下销毁自身合成 FBO/pick。
        if (m_renderTarget) {
            driver.destroyRenderTarget(m_renderTarget);
            m_renderTarget = {};
        }
        if (m_pickTarget) {
            driver.destroyRenderTarget(m_pickTarget);
            m_pickTarget = {};
        }
        m_fboStale = false;
    }
    if (!m_renderTarget) {
        m_renderTarget = driver.createRenderTarget(
            rhi::TargetBufferFlags::ALL, m_rect.width(), m_rect.height(), 1, 1);
    }
    if (!m_pickTarget) {
        // Pick 附件：R32UI featureId + RGBA8 depthAndOrder + DEPTH24。Pick 着色器
        // 变体输出 `out uint fragColor`（loc 0）+ `out vec4 fragDepthOrder`
        // （loc 1 = renderOrder*0.0625 + encodeDepthRgb(linearDepth)，参考
        // Fragment.ts addPickBufferOutputs 的 output1/output2 通道拆分：DanQing
        // 的 featureId 直存 R32UI（登记于 chain-audit §3），depthAndOrder 与参考
        // 同格式）。回读：featureId 走 GL_RED_INTEGER；depth 走附件 1 RGBA8。
        rhi::TextureFormat const pickColorFormats[2] = {rhi::TextureFormat::R32UI,
                                                        rhi::TextureFormat::RGBA8};
        m_pickTarget = driver.createRenderTargetMRT(
            rhi::TargetBufferFlags::COLOR0 | rhi::TargetBufferFlags::COLOR1 | rhi::TargetBufferFlags::DEPTH,
            m_rect.width(), m_rect.height(), 1, 1,
            2, pickColorFormats, rhi::TextureFormat::DEPTH24);
    }
}

TargetImpl::~TargetImpl()
{
    // Canvas-decoration backend GPU handles (VBO/VBH/VBIH/IBH/VAO/program) die
    // with the target — the module's dispose(driver) pattern
    // (BackgroundMapDrape::dispose, SceneCompositor::destroyOitResources).
    if (m_canvasContext)
        m_canvasContext->dispose(m_system.getDriver());
    if (m_pickTarget) {
        m_system.getDriver().destroyRenderTarget(m_pickTarget);
    }
    if (m_renderTarget) {
        m_system.getDriver().destroyRenderTarget(m_renderTarget);
    }
}

// ---------------------------------------------------------------------------
// readColorData — RGBA8888 color readback from the color attachment
// Ported from: itwinjs-core Target.readPixels()（Pixel.Selector.Color）
// ---------------------------------------------------------------------------
bool TargetImpl::readColorData(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                               std::vector<uint8_t>& pixels)
{
    pixels.resize(static_cast<size_t>(width) * height * 4);
    rhi::PixelBufferDescriptor pbd(pixels.data(), pixels.size(),
                                    static_cast<GLenum>(GL::Texture::Format::Rgba),
                                    static_cast<GLenum>(GL::DataType::UnsignedByte));
    m_system.getDriver().readPixels(m_renderTarget, x, y, width, height, std::move(pbd));
    return true;
}

// ---------------------------------------------------------------------------
// setScene — set the scene graphics to render
// Ported from: itwinjs-core Target.changeScene()
// ---------------------------------------------------------------------------
void TargetImpl::setScene(std::unique_ptr<Graphic> scene)
{
    m_scene = std::move(scene);
}

// ---------------------------------------------------------------------------
// setSceneContainer — set Scene for initForRender path
// Ported from: itwinjs-core TargetGraphics.setScene()
// ---------------------------------------------------------------------------
void TargetImpl::setSceneContainer(Scene const* scene)
{
    m_targetGraphics.setScene(scene);
}

// ---------------------------------------------------------------------------
// setDecorations — set decorations for rendering pipeline
// Ported from: itwinjs-core TargetGraphics.decorations
// ---------------------------------------------------------------------------
void TargetImpl::setDecorations(Decorations const* decorations)
{
    m_targetGraphics.setDecorations(decorations);
}

// ---------------------------------------------------------------------------
// setLightSettings — set light settings from ViewState
// Ported from: itwinjs-core RenderPlan.lights
// ---------------------------------------------------------------------------
void TargetImpl::setLightSettings(float const* sunDir, float sunIntensity, float const* ambientColor)
{
    for (int i = 0; i < 3; ++i) {
        m_sunDir[i] = sunDir[i];
        m_ambientColor[i] = ambientColor[i];
    }
    m_sunIntensity = sunIntensity;
}

// ---------------------------------------------------------------------------
// setBackgroundColor — set background color from ViewState
// Ported from: itwinjs-core RenderPlan.backgroundColor
// ---------------------------------------------------------------------------
void TargetImpl::setBackgroundColor(float r, float g, float b, float a)
{
    m_backgroundColor[0] = r; m_backgroundColor[1] = g;
    m_backgroundColor[2] = b; m_backgroundColor[3] = a;
}

// ---------------------------------------------------------------------------
// drawFrame — main rendering pipeline
// Ported from: itwinjs-core Target.paintScene() (lines 657-738)
//
// Flow:
//   1. beginPaint() — push FBO to framebuffer stack
//   2. populateCommandsFromScene() — walk scene graph
//   3. compositor.draw(commands) — main scene rendering
//   4. drawOverlays() — world and view overlay passes
//   5. endPaint() — pop FBO, blit to canvas
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Perf metric records — Target.ts:610-645 的 GLTimer 接线（GPU profiler）
// ---------------------------------------------------------------------------
bool TargetImpl::glTimerActive() const noexcept
{
    auto* debugControl = m_system.debugControl();
    return debugControl != nullptr && debugControl->isGLTimerSupported
        && debugControl->resultsCallback != nullptr;
}

// Ported from: Target.beginPerfMetricFrame (Target.ts:610-616).
void TargetImpl::beginPerfMetricFrame()
{
    if (glTimerActive())
        m_system.glTimer().beginFrame();
}

// Ported from: Target.endPerfMetricFrame (Target.ts:618-630).
void TargetImpl::endPerfMetricFrame()
{
    if (glTimerActive())
        m_system.glTimer().endFrame();
}

// Ported from: Target.beginPerfMetricRecord (Target.ts:633-639).
void TargetImpl::beginPerfMetricRecord(char const* operation)
{
    if (glTimerActive())
        m_system.glTimer().beginOperation(operation);
}

// Ported from: Target.endPerfMetricRecord (Target.ts:641-645).
void TargetImpl::endPerfMetricRecord()
{
    if (glTimerActive())
        m_system.glTimer().endOperation();
}

void TargetImpl::drawFrame()
{
    // ← itwinjs-core Target.drawFrame 入口的 assignDC 门（Target.ts:546-549）：
    // FBO 未分配（构造后首帧 / onResized 销毁后）时惰性 allocateFbo（:751-766）。
    allocateFbo();
    if (!m_renderTarget)
        return;  // assignDC 失败（尺寸无效）——跳过本帧（参考 :548-549 同形）

    if (!m_scene) return;

    // ← Target.ts:662 beginPerfMetricFrame → glTimer.beginFrame
    beginPerfMetricFrame();

    // ← Target.ts:663 beginPerfMetricRecord("Begin Paint")
    beginPerfMetricRecord("Begin Paint");
    // Step 1: Begin paint — push FBO to framebuffer stack
    // Ported from: itwinjs-core Target._beginPaint()
    beginPaint();
    endPerfMetricRecord();

    // ← Target.ts:700 beginPerfMetricRecord("Init Commands")
    beginPerfMetricRecord("Init Commands");
    // Step 2: Populate commands from scene graph
    // Ported from: itwinjs-core RenderCommands.initForRender()
    m_commands.clear();
    populateCommandsFromScene();
    endPerfMetricRecord();

    // Render-command breakdown snapshot for the diagnostics panel
    // (RenderCommands.dump, RenderCommands.ts:742-771 — Target.getRenderCommands
    // source). Captured post-population; the panel reads it between frames.
    m_lastCommandCount = m_commands.dump();

    // Step 3: Compositor draw — main rendering pipeline
    // Ported from: itwinjs-core Target.paintScene() line 704
    m_compositor->draw(m_commands);

    // ← Target.ts:707-715 Overlay Draws（World/View Overlays 嵌套）
    beginPerfMetricRecord("Overlay Draws");
    beginPerfMetricRecord("World Overlays");
    // Step 4: Overlay rendering — world and view overlay passes
    // Ported from: itwinjs-core Target.paintScene() lines 706-707
    drawOverlays();
    endPerfMetricRecord();
    endPerfMetricRecord();

    // ← Target.ts:732 beginPerfMetricRecord("End Paint")
    beginPerfMetricRecord("End Paint");
    // Step 5: End paint — pop FBO, blit to canvas
    // Ported from: itwinjs-core Target._endPaint()
    endPaint();
    endPerfMetricRecord();

    // ← Target.ts:735 endPerfMetricFrame → glTimer.endFrame
    endPerfMetricFrame();
}

// ---------------------------------------------------------------------------
// beginPaint — push FBO to framebuffer stack
// Ported from: itwinjs-core Target._beginPaint()
// ---------------------------------------------------------------------------
void TargetImpl::beginPaint()
{
    // Ported from: itwinjs-core Target._beginPaint()
    //
    // In itwinjs-core, beginPaint pushes the FBO to the framebuffer stack
    // and updates compositor texture sizes if the viewport resized.
    //
    // In DanQing, the render target FBO is created during construction and
    // remains valid. The compositor renders to m_renderTarget, and endPaint()
    // blits to the output target.
    //
    // No additional setup needed here — the FBO is already bound when
    // the compositor calls beginRenderPass(m_renderTarget, ...).
}

// ---------------------------------------------------------------------------
// populateCommandsFromScene — walk scene graph and populate RenderCommands
// Ported from: itwinjs-core RenderCommands.initForRender()
// ---------------------------------------------------------------------------
void TargetImpl::populateCommandsFromScene()
{
    // Ported from: itwinjs-core RenderCommands.initForRender()
    //
    // Two TargetGraphics shapes feed the same decoration walk:
    //   - dqApp/Viewport supplies a Scene container (m_targetGraphics.getScene()).
    //   - The offscreen BlankViewport path (RenderSmokeTest) supplies the flattened
    //     single tree via setScene(graphics) (m_scene), with decorations set via
    //     changeDecorations → TargetGraphics.
    // The flattened tree carries the foreground batch; the decorations walk below
    // matches the initForRender decoration block (world → decorationState-scoped
    // overlays) for BOTH shapes, so WorldDecoration/WorldOverlay/ViewOverlay
    // decorator graphics render on either path.
    if (m_targetGraphics.getScene()) {
        m_commands.initForRender(m_targetGraphics);
        return;
    }

    if (m_scene)
        m_scene->addCommands(m_commands);

    auto const* dec = m_targetGraphics.getDecorations();
    if (!dec)
        return;

    // Ported from: initForRender() decoration block (RenderCommands.ts:561-574).
    // addBackground/addSkyBox: decorations-state-scoped branches for the background
    // passes. addGraphics: decorations.normal drawn with scene lighting.
    m_commands.addBackground(static_cast<Graphic*>(dec->viewBackground));
    m_commands.addSkyBox(static_cast<Graphic*>(dec->skyBox));

    if (!dec->normal.empty()) {
        std::vector<Graphic*> normal;
        normal.reserve(dec->normal.size());
        for (auto* rg : dec->normal) {
            if (!rg) continue;
            if (rg->isBranch()) {
                // GraphicBranch（仅继承 RenderGraphic，非 Graphic）：裸 cast 会
                // 类型双关（-fno-rtti 无检查）→ SEH。经 adapter 桥接遍历 entries。
                normal.push_back(new RenderGraphicAdapter(rg));
            } else {
                // Primitive 等内部 Graphic（继承 Graphic）——裸 cast 合法（原路径）。
                normal.push_back(static_cast<Graphic*>(rg));
            }
        }
        m_commands.addGraphics(normal);
        // adapter 是即时桥——不 delete（所有权契约独立 pass 再定，保守泄漏）。
    }

    // World decorations: wrapped in a WorldDecorations branch (addWorldDecorations).
    // Null entries are filtered by addWorldDecorations itself.
    if (!dec->world.empty()) {
        std::vector<Graphic*> world;
        world.reserve(dec->world.size());
        for (auto* rg : dec->world)
            world.push_back(static_cast<Graphic*>(rg));
        m_commands.addWorldDecorations(world);
    }

    // Overlay decorations: decoration state scope matches initForRender.
    pushAndPopDecorationsState([dec, this]() {
        if (!dec->viewOverlay.empty()) {
            std::vector<Graphic*> vo;
            vo.reserve(dec->viewOverlay.size());
            for (auto* rg : dec->viewOverlay) {
                if (rg)
                    vo.push_back(static_cast<Graphic*>(rg));
            }
            m_commands.addDecorations(vo, RenderPass::ViewOverlay);
        }
        if (!dec->worldOverlay.empty()) {
            std::vector<Graphic*> wo;
            wo.reserve(dec->worldOverlay.size());
            for (auto* rg : dec->worldOverlay) {
                if (rg)
                    wo.push_back(static_cast<Graphic*>(rg));
            }
            m_commands.addDecorations(wo, RenderPass::WorldOverlay);
        }
    });
}

// ---------------------------------------------------------------------------
// drawOverlays — draw world and view overlay passes
// Ported from: itwinjs-core Target.drawPass() for WorldOverlay/ViewOverlay
//
// itwinjs-core implementation:
//   private drawPass(pass: RenderPass): void {
//     this.renderSystem.applyRenderState(this.getRenderState(pass));
//     this.techniques.execute(this, this._renderCommands.getCommands(pass), pass);
//   }
//
// getRenderState() returns _overlayRenderState for WorldOverlay/ViewOverlay:
//   depthMask=false, blend=true, blendFunc=(ONE, ONE_MINUS_SRC_ALPHA)
// ---------------------------------------------------------------------------
void TargetImpl::drawOverlays()
{
    // Apply overlay render state
    m_overlayRenderState.apply(m_currentRenderState);
    m_currentRenderState = m_overlayRenderState;

    // Bind m_renderTarget so overlays draw into the composited target (FBO), not
    // the default framebuffer (FB 0). Without this, Scene decorations routed to
    // the overlay pass (e.g., the glTF cube) draw to FB 0, which is then
    // overwritten by endPaint's blit (m_renderTarget → screen) → invisible.
    rhi::RenderPassParams overlayParams;
    overlayParams.flags.clear = {};  // LOAD — don't clear the scene already rendered
    auto rect = getViewRect();
    overlayParams.viewport = {static_cast<int32_t>(rect.left), static_cast<int32_t>(rect.top),
                              rect.width(), rect.height()};
    auto& driver = getDriver();
    driver.beginRenderPass(getRenderTarget(), overlayParams);

    if (std::getenv("DANQING_DP_TRACE"))
        fprintf(stderr, "[DP] drawOverlays woCmds=%zu\n",
                m_commands.getCommands(RenderPass::WorldOverlay).size());
    m_compositor->drawPass(m_commands, RenderPass::WorldOverlay);
    m_compositor->drawPass(m_commands, RenderPass::ViewOverlay);

    driver.endRenderPass();
}

// ---------------------------------------------------------------------------
// endPaint — blit FBO to output target (screen)
// Ported from: itwinjs-core Target._endPaint()
// ---------------------------------------------------------------------------
void TargetImpl::endPaint()
{
    if (!m_renderTarget) return;
    blitToOutput();
}

// ---------------------------------------------------------------------------
// blitToOutput — blit the composited frame to the output surface
// Ported from: itwinjs-core Target._endPaint()（blit 半）
//
// The compositor renders to m_renderTarget (an off-screen FBO).
// We blit the result to the output target (SwapChain surface).
//
// When m_outputTarget is set (Swapchain-based rendering), blit to it.
// This enables Vulkan support (no "default framebuffer" concept).
// When m_outputTarget is not set, fall back to blitToDefaultFramebuffer (FBO 0).
// ---------------------------------------------------------------------------
void TargetImpl::blitToOutput()
{
    auto& driver = m_system.getDriver();

    // Convert ViewRect to RHI Viewport
    // ViewRect: left, top, right, bottom (top-left origin)
    // Viewport: left, bottom, width, height (bottom-left origin)
    rhi::Viewport vp;
    vp.left = static_cast<int32_t>(m_rect.left);
    vp.bottom = static_cast<int32_t>(m_rect.top);  // ViewRect top = Viewport bottom (flip)
    vp.width = m_rect.width();
    vp.height = m_rect.height();

    // Blit from the off-screen render target to the output surface.
    // ← itwinjs-core: the canvas is updated after rendering.
    if (m_outputTarget) {
        // Swapchain-based: blit to SwapChain's render target.
        // Works for both OpenGL (FBO 0) and Vulkan (VkImage target).
        rhi::Viewport dstVp;
        dstVp.left = 0;
        dstVp.bottom = 0;
        dstVp.width = m_rect.width();
        dstVp.height = m_rect.height();
        driver.blit(rhi::TargetBufferFlags::ALL, m_outputTarget, dstVp, m_renderTarget, vp);
    } else {
        // Legacy path: blit to default framebuffer (FBO 0, OpenGL-specific).
        driver.blitToDefaultFramebuffer(m_renderTarget, vp);
    }

    // TEMP-DIAG（DANQING_OIT_DUMP=1）：呈现目标回读——FBO(合成帧)与屏幕不符时，
    // 判定 blit 后的输出内容是否已坏（真 present bug）还是屏幕抓取伪象。
    if (std::getenv("DANQING_OIT_DUMP")) {
        std::vector<uint8_t> px(static_cast<size_t>(m_rect.width()) * m_rect.height() * 4, 0);
        rhi::PixelBufferDescriptor pbd(px.data(), px.size(),
            static_cast<GLenum>(GL::Texture::Format::Rgba),
            static_cast<GLenum>(GL::DataType::UnsignedByte),
            0, 1, 0, 0, m_rect.width(), m_rect.height());
        driver.readPixels(m_outputTarget, 0, 0, m_rect.width(), m_rect.height(), std::move(pbd));
        int dark = 0, black = 0;
        for (size_t i = 0; i < px.size() / 4; ++i) {
            uint8_t const* p = &px[i * 4];
            if (p[0] < 80 && p[1] < 80 && p[2] < 90) ++dark;
            if (p[0] < 40 && p[1] < 40 && p[2] < 40) ++black;
        }
        uint8_t const* c = &px[(static_cast<size_t>(m_rect.height() / 2) * m_rect.width()
                              + m_rect.width() / 2) * 4];
        uint8_t const* k = &px[(static_cast<size_t>(4) * m_rect.width() + 4) * 4];
        std::fprintf(stderr, "[OITDUMP] output(blit dst) %ux%u dark=%d black=%d "
                             "center=(%d,%d,%d) corner=(%d,%d,%d)\n",
                     m_rect.width(), m_rect.height(), dark, black,
                     c[0], c[1], c[2], k[0], k[1], k[2]);

        // TEMP-DIAG（2026-09-19 leave 取证）：DANQING_DUMP_OUT=1 → 输出目标全帧落盘
        // PPM（GL bottom-up 翻转为 top-down；绝对路径——测试 exe 的相对路径写
        // 失败坑见 §12.10 教训 4）。
        if (std::getenv("DANQING_DUMP_OUT")) {
            static int s_dumpCount = 0;
            if (s_dumpCount < 200) {  // 有界：只 dump 前 200 帧
                char path[256];
                snprintf(path, sizeof(path), "D:/Github/DanQing/build/outtarget-%03d.ppm", s_dumpCount++);
                if (FILE* f = fopen(path, "wb")) {
                    fprintf(f, "P6\n%u %u\n255\n", m_rect.width(), m_rect.height());
                    for (int row = static_cast<int>(m_rect.height()) - 1; row >= 0; --row) {
                        for (uint32_t col = 0; col < m_rect.width(); ++col) {
                            uint8_t const* p = &px[(static_cast<size_t>(row) * m_rect.width() + col) * 4];
                            fwrite(p, 1, 3, f);
                        }
                    }
                    fclose(f);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// drawCanvasDecorations — rasterize 2D canvas decorations inside the GL frame
// Ported from: itwinjs-core Target.drawFrame → drawOverlayDecorations
//              (Target.ts:546-554, the call at :552 — unconditional per drawn
//              frame) + OnScreenTarget.drawOverlayDecorations body
//              (Target.ts:1408-1439: per entry save → position?translate →
//              drawDecoration → restore).
//
// APPROVED DEVIATION (预批准兜底，原 spec 2026-09-11-windowarea-look-design §2.4；
// spec 已随 2026-09-24 历史文档清理删除，偏差登记以本注释为准): the reference
// rasterizes into an HTML 2D canvas composited over the
// WebGL canvas; DanQing rasterizes in-GL (GLCanvasContext records strokes → GL_LINES)
// into the composited frame FBO, then re-blits to the output target (drawFrame's
// endPaint blit already happened — the caller invokes this between drawFrame and
// present). The reference's _2dCanvas.needsClear clearRect (:1409-1415) has no GL
// equivalent: GL frames are redrawn wholesale each drawn frame (the scene pass
// clears), so an empty/strokeless list simply draws nothing.
// ---------------------------------------------------------------------------
void TargetImpl::drawCanvasDecorations(std::vector<CanvasDecoration> const& canvasDecs)
{
    if (!m_renderTarget) return;
    if (canvasDecs.empty())
        return;   // ← Target.ts:1413-1414（canvasDecs 缺失 → 无绘制；无清除需求，见上）

    if (!m_canvasContext)
        m_canvasContext = std::make_unique<GLCanvasContext>();
    m_canvasContext->beginFrame();

    // Target.ts:1415-1423：save → (position → translate) → drawDecoration → restore。
    for (auto const& overlay : canvasDecs) {
        m_canvasContext->save();
        if (overlay.position.has_value())
            m_canvasContext->translate(overlay.position->x, overlay.position->y);
        if (overlay.drawDecoration)
            overlay.drawDecoration(*m_canvasContext);
        m_canvasContext->restore();
    }

    if (!m_canvasContext->hasContent())
        return;

    // Rasterize into the composited frame. LOAD — preserve the scene already
    // rendered (drawOverlays 的同形态：flags.clear={}，TargetImpl.cpp drawOverlays)。
    rhi::RenderPassParams passParams;
    passParams.flags.clear = {};  // LOAD
    passParams.viewport = {static_cast<int32_t>(m_rect.left), static_cast<int32_t>(m_rect.top),
                           m_rect.width(), m_rect.height()};
    auto& driver = m_system.getDriver();
    if (getenv("DANQING_CURSOR_TRACE")) {
        // 落盘探针（app 的 stderr 在无控制台 GUI 进程不落盘——取证明文写文件）。
        if (FILE* f = fopen("C:/Windows/Temp/danqing_cursor_flush.log", "a")) {
            fprintf(f, "[CURSOR] flush rect=(%d,%d %dx%d) dpr=%.2f decs=%zu\n",
                    static_cast<int>(m_rect.left), static_cast<int>(m_rect.top),
                    static_cast<int>(m_rect.width()), static_cast<int>(m_rect.height()),
                    static_cast<double>(devicePixelRatio()), canvasDecs.size());
            fclose(f);
        }
    }
    driver.beginRenderPass(m_renderTarget, passParams);
    m_canvasContext->flush(driver, static_cast<float>(m_rect.width()),
                           static_cast<float>(m_rect.height()), devicePixelRatio());
    driver.endRenderPass();

    // Re-present: endPaint's blit ran before the decorations were recorded.
    blitToOutput();

    // TEMP-DIAG（2026-09-19 leave 取证）：canvas 光栅化后回读**输出目标**上十字
    // 位置的 81×81 块——屏幕无光圈的实锤切分：输出目标里有 = present 链路丢；
    // 没有 = flush/光栅化在 dpr=2 下就丢了。明文写文件（GUI 进程 stderr 不落盘）。
    if (getenv("DANQING_CURSOR_TRACE") && !canvasDecs.empty() && canvasDecs[0].position
        && m_outputTarget) {
        double const dpr = devicePixelRatio();
        int const dcx = static_cast<int>(canvasDecs[0].position->x * dpr);
        int const ry = m_rect.height() - static_cast<int>(canvasDecs[0].position->y * dpr);
        int const px0 = std::max(0, dcx - 40);
        int const py0 = std::max(0, ry - 40);
        int const pw = std::min(81, static_cast<int>(m_rect.width()) - px0);
        int const ph = std::min(81, static_cast<int>(m_rect.height()) - py0);
        std::vector<uint8_t> patch(static_cast<size_t>(pw) * ph * 4, 0);
        rhi::PixelBufferDescriptor pbd(patch.data(), patch.size(),
            static_cast<GLenum>(GL::Texture::Format::Rgba),
            static_cast<GLenum>(GL::DataType::UnsignedByte),
            0, 1, 0, 0, pw, ph);
        driver.readPixels(m_outputTarget, px0, py0, pw, ph, std::move(pbd));
        // 光圈签名（ToolAdmin.cpp:75-88）：白描边 .4 → (243,246,248)，黑外圈
        // rgba(0,0,0,.8) 在亮背景上 → (~47,48,49)。黑外圈是最易检出的分量。
        int whitish = 0, darkish = 0;
        for (size_t i = 0; i + 2 < patch.size(); i += 4) {
            if (patch[i] > 246 && patch[i + 1] > 246 && patch[i + 2] > 246) ++whitish;
            if (patch[i] < 90 && patch[i + 1] < 90 && patch[i + 2] < 90) ++darkish;
        }
        // 中心十字（参考 drawLocateCursor 中心点）周围 4 像素抽样值。
        int const ci = (std::min(81, ph) / 2 * std::min(81, pw) + std::min(81, pw) / 2) * 4;
        if (FILE* f = fopen("C:/Windows/Temp/danqing_cursor_flush.log", "a")) {
            fprintf(f, "[CURSOR] outTarget patch@(%d,%d) dev(%d,%d) whitish=%d darkish=%d center=(%d,%d,%d)\n",
                    px0, py0, dcx, ry, whitish, darkish,
                    patch[ci], patch[ci + 1], patch[ci + 2]);
            fclose(f);
        }
    }

    // TEMP-DIAG：DANQING_DUMP_OUT=1 → canvas 帧的输出目标全帧 PPM 落盘（有界 40 帧）。
    // 直读输出目标亲眼定位光圈实际落点（绝对路径，§12.10 教训 4）。
    if (std::getenv("DANQING_DUMP_OUT") && m_outputTarget) {
        static int s_canvasDump = 0;
        if (s_canvasDump < 40) {
            std::vector<uint8_t> full(static_cast<size_t>(m_rect.width()) * m_rect.height() * 4, 0);
            rhi::PixelBufferDescriptor fpbd(full.data(), full.size(),
                static_cast<GLenum>(GL::Texture::Format::Rgba),
                static_cast<GLenum>(GL::DataType::UnsignedByte),
                0, 1, 0, 0, m_rect.width(), m_rect.height());
            driver.readPixels(m_outputTarget, 0, 0, m_rect.width(), m_rect.height(), std::move(fpbd));
            char path[256];
            snprintf(path, sizeof(path), "D:/Github/DanQing/build/outtarget-%03d.ppm", s_canvasDump++);
            if (FILE* f = fopen(path, "wb")) {
                fprintf(f, "P6\n%u %u\n255\n", m_rect.width(), m_rect.height());
                for (int row = static_cast<int>(m_rect.height()) - 1; row >= 0; --row)
                    for (uint32_t col = 0; col < m_rect.width(); ++col)
                        fwrite(&full[(static_cast<size_t>(row) * m_rect.width() + col) * 4], 1, 3, f);
                fclose(f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// changeRenderPlan — update view flags, 3D mode, frustum
// Ported from: itwinjs-core Target.changeRenderPlan() (line 495-544)
// ---------------------------------------------------------------------------
void TargetImpl::changeRenderPlan(ViewFlags const& viewFlags, bool is3d)
{
    // Update 3D flag
    if (m_is3d != is3d) {
        m_is3d = is3d;
        // Update decorations state for dimensionality change
        m_decorationsState.changeRenderPlan(m_decorationsState.getViewFlags(), is3d);
    }

    // Update branch stack with new view flags
    m_branchStack.changeRenderPlan(viewFlags, is3d);
}

// ---------------------------------------------------------------------------
// setHiliteSet — store the hilited element-id set and desync.
// Ported from: itwinjs-core Target.setHiliteSet() (Target.ts:478-481:
//               this._hilites = hilite; desync(this._hiliteSyncTarget)).
// The LUTs live on each Batch and update lazily at draw time when the version
// they last observed is stale (FeatureOverrides.update → updateHilite,
// FeatureOverrides.ts:412-441 — the compositor's PushBatch drives it).
// ---------------------------------------------------------------------------
void TargetImpl::setHiliteSet(uint32_t const* elementIds, size_t count)
{
    m_hiliteElementIds.assign(elementIds, elementIds + count);
    ++m_hiliteVersion;
}

// ---------------------------------------------------------------------------
// setFlashed — store the flashed element + intensity; id change desyncs LUTs.
// Ported from: itwinjs-core Target.setFlashed() (Target.ts:482-489:
//               if (id !== this._flashedId) { this._flashedId = id; ... }
//               this._flashIntensity = intensity).
// The Flashed bit lives in the per-batch LUT (FeatureOverrides.updateFlashed
// runs when flashedId changed — :420 flashedId !== this._lastFlashId), so the
// id change bumps the shared LUT version; intensity alone does not.
// ---------------------------------------------------------------------------
void TargetImpl::setFlashed(uint32_t elementId, float intensity)
{
    if (elementId != m_flashedId) {
        m_flashedId = elementId;
        ++m_hiliteVersion;
    }
    m_flashIntensity = intensity;
}

// ---------------------------------------------------------------------------
// setHiliteColor — set hilite color
// Ported from: itwinjs-core Target.setHiliteColor()
// ---------------------------------------------------------------------------
void TargetImpl::setHiliteColor(float r, float g, float b)
{
    if (m_compositor)
        m_compositor->setHiliteColor(r, g, b);
}

// ---------------------------------------------------------------------------
// readPixels — batch read pixels from pick buffer
// Ported from: itwinjs-core Target.readPixels() (line 768-825)
//
// Renders the scene into the pick FBO with simplified view flags
// (no lighting, no shadows, no transparency), then reads back the
// pixel data for feature selection.
// ---------------------------------------------------------------------------
void TargetImpl::readPixels(int32_t x, int32_t y, uint32_t width, uint32_t height,
                            uint32_t* outputBuffer, uint32_t bufferSize)
{
    if (!outputBuffer || bufferSize == 0 || width == 0 || height == 0)
        return;

    // Ensure pick buffer is large enough
    uint32_t requiredSize = width * height;
    if (bufferSize < requiredSize)
        return;

    // Step 1: Begin readPixels — push simplified view flags
    beginReadPixels();

    // Step 2: Re-populate commands with pick-friendly settings
    // (translucent drawn as opaque, non-pickable decorations excluded)
    m_commands.clear();
    m_commands.initForReadPixels(m_targetGraphics);

    // Step 3: Render the scene into the pick target
    m_compositor->drawForPick(m_commands);

    // Step 4: Read pixels from the pick buffer
    auto& driver = m_system.getDriver();
    rhi::PixelBufferDescriptor pbd(outputBuffer, requiredSize * sizeof(uint32_t),
                                    static_cast<GLenum>(GL::Texture::Format::RedInteger),
                                    static_cast<GLenum>(GL::DataType::UnsignedInt));
    driver.readPixels(m_pickTarget, x, y, width, height, std::move(pbd));

    // TEMP-DIAG（拾取 saga，env 门控）：pick 回读层界证据。
    if (getenv("DANQING_PICK_TRACE")) {
        printf("[PICK] rect=(%d,%d %ux%u) raw[0]=%u batches=%zu nextBatchId=%u\n",
               x, y, width, height, outputBuffer ? outputBuffer[0] : 0xDEADu,
               m_batchState.getNumBatches(), m_batchState.getNextBatchId());
    }

    // Step 4.5: Translate pick values (batchId + featureIndex) to element ids.
    // The pick shader emits the global feature id; the caller wants element ids
    // (e.g. a decoration's pickableId). BatchState owns the batchId→featureTable
    // mapping built during drawForPick's PushBatch pass.
    // Ported from: itwinjs-core PixelBuffer → Pixel.Data.elementId via the
    //               batch map (Pixel.ts computeElementId; Target.ts:824
    //               resetBatchState happens later, in endReadPixels).
    for (uint32_t i = 0; i < requiredSize; ++i) {
        if (outputBuffer[i] != 0)
            outputBuffer[i] = static_cast<uint32_t>(m_batchState.getElementId(outputBuffer[i]));
    }

    // Step 5: End readPixels — restore state
    endReadPixels();
}

// ---------------------------------------------------------------------------
// readPickDepth — 同一 pick 渲染通道读 depthAndOrder 附件（附件 1，RGBA8）
// Ported from: itwinjs-core Target.readPixels 的 Pixel.Selector geometryAndDistance
//              深度半边（Target.ts:768-825；附件布局 SceneCompositor.ts:60-70）。
// 返回线性深度分数（decodeDepthRgb：r + g/255 + b/65025，0=近 1=远）。
// ---------------------------------------------------------------------------
bool TargetImpl::readPickDepth(int32_t x, int32_t y, uint32_t width, uint32_t height,
                               float* outFractions, uint32_t outCount)
{
    if (!outFractions || outCount == 0 || width == 0 || height == 0)
        return false;
    uint32_t const requiredSize = width * height;
    if (outCount < requiredSize)
        return false;

    beginReadPixels();
    m_commands.clear();
    m_commands.initForReadPixels(m_targetGraphics);
    m_compositor->drawForPick(m_commands);

    auto& driver = m_system.getDriver();
    std::vector<uint8_t> rgba(static_cast<size_t>(requiredSize) * 4);
    rhi::PixelBufferDescriptor pbd(rgba.data(), rgba.size(),
                                   static_cast<GLenum>(GL::Texture::Format::Rgba),
                                   static_cast<GLenum>(GL::DataType::UnsignedByte));
    driver.readPixels(m_pickTarget, x, y, width, height, std::move(pbd), 1);

    // decodeDepthRgb(pdo.yzw)（FeatureSymbology.ts:395-401）：深度字节在 G/B/A
    // 三通道（R = renderOrder），CPU 侧同式。
    for (uint32_t i = 0; i < requiredSize; ++i) {
        float const g = rgba[i * 4 + 1] / 255.0f;
        float const b = rgba[i * 4 + 2] / 255.0f;
        float const a = rgba[i * 4 + 3] / 255.0f;
        outFractions[i] = g + b / 255.0f + a / 65025.0f;
    }
    endReadPixels();
    return true;
}

// ---------------------------------------------------------------------------
// beginReadPixels — simplified view flags for picking
// Ported from: itwinjs-core Target.beginReadPixels() (line 879-919)
//
// Temporarily disables lighting, shadows, transparency, etc. to speed up
// the pick pass. Pushes a new BranchState with simplified view flags.
// ---------------------------------------------------------------------------
void TargetImpl::beginReadPixels()
{
    m_isReadPixelsInProgress = true;

    // Create simplified view flags for picking (no lighting, no shadows, etc.)
    // Ported from: itwinjs-core beginReadPixels() viewFlags copy
    ViewFlagsProperties pickProps;
    pickProps.transparency = false;
    pickProps.lighting = false;
    pickProps.shadows = false;
    pickProps.acsTriad = false;
    pickProps.grid = false;
    pickProps.monochrome = false;
    pickProps.materials = false;
    pickProps.ambientOcclusion = false;

    ViewFlags pickFlags(pickProps);

    // Push a BranchState with simplified flags carrying the CURRENT VIEW
    // matrices. The view's single source of truth is FrustumUniforms (the
    // reference renders the pick view with the same frustum uniforms as the
    // frame — Target.ts readPixelsFromFbo → compositor.drawForReadPixels).
    // DanQing's frame pushes these same matrices transiently
    // (OpenGLRenderTarget::drawFrame → compositor's branch stack push) and
    // unwinds at frame end, so readPixels invoked BETWEEN frames sees a stale/
    // identity stack top — deriving mv/mvp from the frustum uniforms here
    // (identical math to that push) keeps the pick view aligned with the
    // displayed frame. NOTE: pushes go on the COMPOSITOR's stack — that is the
    // one drawPass consumes (pushing TargetImpl's own stack is invisible to
    // draws; the pick pass then rendered with identity mvp → zero fragments).
    // Ported from: itwinjs-core Target.beginReadPixels() — pushState(state)
    //               (view matrices per FrustumUniforms, Target.ts:899-910)
    if (!m_compositor)
        return;
    auto& drawStack = m_compositor->getBranchStack();
    auto const& top = drawStack.getTop();
    std::array<float, 16> mv, mvp;
    {
        auto const& view = m_uniforms.frustum.getViewMatrix();  // Transform
        auto const& r = view.matrix.coffs;
        // Transform (row-major Matrix3d + origin) → row-major Matrix4d.
        Matrix4d const viewRow = Matrix4d::CreateRowValues(
            r[0], r[1], r[2], view.origin.x,
            r[3], r[4], r[5], view.origin.y,
            r[6], r[7], r[8], view.origin.z,
            0.0, 0.0, 0.0, 1.0);
        Matrix4d const mvpRow =
            m_uniforms.frustum.getProjectionMatrix().MultiplyMatrixMatrix(viewRow);
        // Row-major Matrix4d → column-major float[16]: transpose [col*4+row].
        for (int row = 0; row < 4; ++row) {
            mv[static_cast<size_t>(row)] = static_cast<float>(viewRow.at(row, 0));
            mv[static_cast<size_t>(4 + row)] = static_cast<float>(viewRow.at(row, 1));
            mv[static_cast<size_t>(8 + row)] = static_cast<float>(viewRow.at(row, 2));
            mv[static_cast<size_t>(12 + row)] = static_cast<float>(viewRow.at(row, 3));
        }
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                mvp[static_cast<size_t>(col * 4 + row)] =
                    static_cast<float>(mvpRow.at(row, col));
    }
    BranchState pickState(pickFlags, mv, mvp, top.is3d());
    drawStack.pushState(pickState);
}

// ---------------------------------------------------------------------------
// endReadPixels — restore state after picking
// Ported from: itwinjs-core Target.endReadPixels() (line 921-928)
// ---------------------------------------------------------------------------
void TargetImpl::endReadPixels()
{
    // Pop the BranchState pushed by beginReadPixels (the compositor's stack —
    // the one draws consume; see beginReadPixels).
    if (m_compositor)
        m_compositor->getBranchStack().pop();
    m_batchState.reset();

    m_isReadPixelsInProgress = false;
}

END_DQ_RENDER_NAMESPACE
