// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scene compositor (multi-pass orchestrator)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SceneCompositor.ts
//
// Orchestrates the entire multi-pass rendering pipeline:
//   1. preDraw() — viewport change detection, resource allocation
//   2. clearOpaque() — clear pick data buffers + depth
//   3. renderBackground() — RenderPass.Background
//   4. renderSkyBox() — RenderPass.SkyBox
//   5. renderBackgroundMap() — RenderPass.BackgroundMap
//   6. target.pushViewClip() — enable clipping
//   7. renderVolumeClassification() — volume classifier stencil + blend
//   8. renderLayers(OpaqueLayers) — opaque layer priority rendering
//   9. renderPointClouds() — point cloud rendering with EDL
//  10. renderOpaque() — opaque geometry (Linear, Planar, General, HiddenEdge)
//  11. renderLayers(TranslucentLayers) — translucent layer rendering
//  12. IF needComposite:
//      a. composite.update(compositeFlags)
//      b. clearTranslucent() — clear OIT textures
//      c. renderTranslucent() — render to translucent FBO
//      d. renderHilite() — render to hilite FBO
//      e. composite() — resolve all FBOs to screen
//  13. renderLayers(OverlayLayers) — overlay layer rendering
//  14. target.popViewClip()
#pragma once

#include "Batch.h"
#include "BranchStack.h"
#include "ClipStack.h"
#include "ContourUniforms.h"
#include "Uniforms.h"
#include "CompositorTextures.h"
#include "CompositorFrameBuffers.h"
#include "RenderCommands.h"
#include "RenderState.h"
#include "ShaderProgramImpl.h"
#include "gl/RenderFlags.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Bring GL::CompositeFlags into dqRender namespace for convenience.
using GL::CompositeFlags;

class TargetImpl;
class Techniques;
class FeatureOverrideLUT;

// ---------------------------------------------------------------------------
// SceneCompositor — multi-pass rendering orchestrator
// (Ported from: itwinjs-core SceneCompositor.ts Compositor)
// ---------------------------------------------------------------------------
class SceneCompositor {
public:
    explicit SceneCompositor(TargetImpl& target, Techniques& techniques);
    ~SceneCompositor();

    SceneCompositor(SceneCompositor const&) = delete;
    SceneCompositor& operator=(SceneCompositor const&) = delete;

    /// Pre-draw lifecycle: viewport change detection, resource allocation.
    /// Ported from: itwinjs-core Compositor.preDraw()
    /// Must be called before draw() each frame.
    void preDraw(uint32_t width, uint32_t height);

    /// Draw the scene using the given render commands.
    /// Ported from: itwinjs-core Compositor.draw()
    void draw(RenderCommands& commands);

    /// Draw for pixel reading (pick rendering).
    /// Ported from: itwinjs-core Compositor.drawForReadPixels()
    void drawForPick(RenderCommands& commands);

    /// Draw a specific render pass (e.g., WorldOverlay, ViewOverlay).
    /// Ported from: itwinjs-core Compositor.drawPass()
    void drawPass(RenderCommands& commands, RenderPass pass,
                  bool pingPong = false, RenderPass cmdPass = RenderPass::None);

    /// Get the branch stack (for external transform queries).
    BranchStack& getBranchStack() { return m_branchStack; }
    BranchStack const& getBranchStack() const { return m_branchStack; }

    /// Get the batch state.
    BatchState& getBatchState() { return m_batchState; }

    /// Get the clip stack（M-P P-C：归属 TargetImpl——参考 BranchUniforms.clipStack；
    /// compositor 转发。原 compositor 本地简化栈移除）。
    ClipStack& getClipStack();

    /// Set the hilite color (RGB).
    /// 参考默认色 = ColorDef.from(0x23, 0xbb, 0xfc)（Hilite.ts:53）。hilite LUT
    /// 不在 compositor 层——每个 Batch 自持 LUT（参考 FeatureOverrides per-batch），
    /// 由 drawPass 的 PushBatch 惰性更新/上传。
    void setHiliteColor(float r, float g, float b) {
        m_hiliteColor[0] = r; m_hiliteColor[1] = g; m_hiliteColor[2] = b;
    }

private:
    // --- Rendering pipeline methods (Ported from: itwinjs-core Compositor) ---

    /// 程序生命周期（参考契约）：同一程序跳过、切换先 endUse 旧再 use 新。
    /// Ported from: itwinjs-core ShaderProgramExecutor.changeProgram (ShaderProgram.ts:724-739)
    /// compositor 各绘制点不经过 ShaderProgramExecutor，按该契约就地管理——
    /// 裸调 use() 会在 m_cachedShader 命中/重复 readPixels 时对同一程序二次
    /// use()，触发 ShaderProgram 的 Debug 断言 !m_inUse（参考中该契约由
    /// executor.changeProgram 保证）。
    void activateProgram(ShaderProgram* shader, rhi::Driver& driver,
                         ShaderProgramParams const& params);
    /// endUse 当前程序并清空跟踪（参考 changeProgram 的 _program.endUse() + 置空）。
    void deactivateProgram(rhi::Driver& driver);

    /// Clear opaque buffers (pick data + color + depth).
    /// Ported from: itwinjs-core Compositor.clearOpaque()
    void clearOpaque(bool needComposite);

    /// Render background commands.
    /// Ported from: itwinjs-core Compositor.renderBackground()
    void renderBackground(RenderCommands& commands, bool needComposite);

    /// Render skybox commands.
    /// Ported from: itwinjs-core Compositor.renderSkyBox()
    void renderSkyBox(RenderCommands& commands, bool needComposite);

    /// Render background map commands.
    /// Ported from: itwinjs-core Compositor.renderBackgroundMap()
    void renderBackgroundMap(RenderCommands& commands, bool needComposite);

    /// Render opaque geometry (Linear, Planar, General, HiddenEdge).
    /// Ported from: itwinjs-core Compositor.renderOpaque()
    void renderOpaque(RenderCommands& commands, CompositeFlags compositeFlags,
                      bool renderForReadPixels);

    /// M-T T-e：AO 三绘制（SceneCompositor.ts:1220-1252——AO 计算→X 模糊→
    /// Y 模糊[TestOrder 臂]；renderOpaqueAO 尾部调用位[:1043]在 DanQing 的
    /// renderOpaque AO 臂尾）。
    void renderAmbientOcclusion(rhi::Driver& driver);

    /// AO 帧的 opaque/背景目标（SceneCompositor.ts:943-947 分流 +
    /// getBackgroundFbo(needComposite)[:1189] 语义——DanQing 单 FBO 承载
    /// [color+featureId+depthAndOrder]，参考的 color-only/MRT 双 FBO 系
    /// MSAA 制品面[本仓 m_samples=1 恒]）。
    rhi::RenderTargetHandle currentOpaqueTarget();

    /// Render translucent geometry to OIT FBO.
    /// Ported from: itwinjs-core Compositor.renderTranslucent()
    void renderTranslucent(RenderCommands& commands);

    /// Render hilite geometry.
    /// Ported from: itwinjs-core Compositor.renderHilite()
    void renderHilite(RenderCommands& commands);

    /// Composite OIT and hilite to main framebuffer.
    /// Ported from: itwinjs-core Compositor.composite()
    /// M-T T-e：wantOcclusion 形参（Composite.ts:127-132 的 wantOcclusion
    /// 变体门——DanQing 均匀门承载，见 OitShaders.h kOitCompositeFrag 注）。
    void composite(bool wantTranslucent, bool wantOcclusion);

    /// copy pick data between textures (pingPong).
    /// Ported from: itwinjs-core Compositor.pingPong()
    void pingPong();

    /// Render layer passes (OpaqueLayers, TranslucentLayers, OverlayLayers).
    /// Ported from: itwinjs-core Compositor.renderLayers()
    void renderLayers(RenderCommands& commands, bool needComposite, RenderPass pass);

    /// Render point clouds with optional EDL.
    /// Ported from: itwinjs-core Compositor.renderPointClouds()
    void renderPointClouds(RenderCommands& commands, CompositeFlags compositeFlags);

    /// Render volume classification.
    /// Ported from: itwinjs-core Compositor.renderVolumeClassification()
    void renderVolumeClassification(RenderCommands& commands, CompositeFlags compositeFlags,
                                    bool renderForReadPixels);

    // --- OIT (Order-Independent Transparency) ---
    // Ported from: itwinjs-core SceneCompositor.ts weighted blended OIT
    void initOitResources(rhi::Driver& driver);
    void destroyOitResources(rhi::Driver& driver);
    void compositeOit(rhi::Driver& driver, bool wantTranslucent = true,
                      bool wantOcclusion = false);  // M-T T-e：AO 腿门

    /// Composite the hilite buffer over the opaque scene.
    /// Ported from: itwinjs-core Compositor.composite() (CompositeGeometry with
    /// CompositeHilite technique — opaque + hilite textures, fullscreen quad).
    void compositeHilite(rhi::Driver& driver);

    /// Apply render state for a pass.
    /// Ported from: itwinjs-core Compositor.getRenderState()
    void applyRenderState(RenderPass pass);

    TargetImpl& m_target;
    Techniques& m_techniques;

    // --- Render states (Ported from: itwinjs-core Compositor constructor) ---
    // 7 render states matching itwinjs-core exactly
    RenderState m_opaqueRenderState;           // depthTest=true
    RenderState m_pointCloudRenderState;       // depthTest=true
    RenderState m_translucentRenderState;      // depthMask=false, blend=true, (ONE,ZERO,ONE,ONE_MINUS_SRC_ALPHA)
    RenderState m_hiliteRenderState;           // depthMask=false, blend=true, destRgb=ONE, destAlpha=ONE
    RenderState m_noDepthMaskRenderState;      // depthMask=false
    RenderState m_overlayRenderState;          // depthTest=false, depthMask=false, blend=true, (ONE,ONE_MINUS_SRC_ALPHA)
    RenderState m_backgroundMapRenderState;    // depthMask=false, blend=true, (ONE,ONE_MINUS_SRC_ALPHA)
    RenderState m_layerRenderState;            // depthTest=true, depthFunc=Always, (ONE,ONE_MINUS_SRC_ALPHA)

    // --- State stacks ---
    // BranchStack 归属 TARGET（参考 BranchUniforms._stack 单栈语义
    // [BranchUniforms.ts:50]——命令构建/绘制派发/changeRenderPlan/拾取全共享
    // 同一栈；RenderCommands 本就以 target 栈构造[TargetImpl ctor :31]）。
    // compositor 引用它——原 compositor 自持第二栈=双栈分裂移植偏差：
    // changeRenderPlan 只喂 target 栈根（TargetImpl::changeRenderPlan），绘制
    // 栈根永持默认 vf → 绘制期 getCurrentViewFlags() 的 thematicDisplay 等位
    // 恒默认（M-S S-e 取证实锤：ThematicDisplayE2E 全族 draw 期 vf.thematic=0
    // ——[THM-DRAW]/[THM-PUSH] 探针：draws 周无 PushBranch/PushState 命令，
    // 栈顶=drawFrame 根 push 的 defaultFlags）。与 m_batchState 同引用形态。
    BranchStack& m_branchStack;
    // BatchState belongs to the TARGET (reference: compositor.target.uniforms.batch.state,
    // Target.ts:793). The compositor references it — RenderCommands (populate-time
    // assignBatchId/findBatch registration) and the compositor's PushBatchCommand lookup
    // (draw-time) must see ONE shared registry, or batch-registered hilite/uniform state
    // never reaches the dispatch.
    BatchState& m_batchState;
    // M-S S-d：m_branchUniforms + m_branchObserver 双成员删——归位
    // TargetUniforms.branch（TargetUniforms.ts:133）+ 逐图元恒跑（参考 sync
    // 是上传去重非跳过计算——observer 门在 thematic 切换帧恒跳过的实证在案）。

    // --- Shader cache ---
    TechniqueId m_cachedTechniqueId = TechniqueId::Surface;
    TechniqueFlags m_cachedTechniqueFlags;
    ShaderProgram* m_cachedShader = nullptr;

    // Previously-applied render state (for diff-and-apply).
    // Ported from: itwinjs-core System._currentRenderState
    RenderState m_currentRenderState;

    // Frame-constant uniforms (lighting, viewport) uploaded once per pass
    ShaderProgramParams m_frameParams;

    // --- Compositor texture/FBO infrastructure ---
    // Ported from: itwinjs-core SceneCompositor.ts Textures/FrameBuffers/Geometry
    CompositorTextures m_textures;
    CompositorFrameBuffers m_frameBuffers;
    uint32_t m_lastWidth = 0;
    uint32_t m_lastHeight = 0;
    bool m_resourcesInitialized = false;
    // AO 资源态（SceneCompositor.ts:1354 的 _includeOcclusion——preDraw 逐帧
    // 评估门；M-T T-c）。
    bool m_occlusionIncluded = false;

    // --- OIT composite shader (kept separate from textures/FBOs) ---
    bool m_oitInitialized = false;
    // M-S S-c：m_thematicUniforms 成员删——thematic 归位 TargetUniforms.
    // thematic（TargetUniforms.ts:133），本处原系错位持有（参考 Target 面）。
    ContourUniforms m_contourUniforms;    // packed contour definitions
    bool m_readPickDataFromPingPong = false;  // pingPong: read pick data from pingPong FBO
    uint8_t m_antialiasSamples = 1;  // MSAA sample count (1 = no MSAA)
    rhi::RenderTargetHandle m_oitRenderTarget;
    rhi::TextureHandle m_oitAccumTexture;
    rhi::TextureHandle m_oitRevealageTexture;
    // Opaque 场景快照（composite 前从 m_renderTarget blit 而来；参考
    // Composite.ts computeOpaqueColor 采样场景色做 over 合成——同 FBO 不可
    // 边读边写，经此中间 RT）。
    rhi::RenderTargetHandle m_opaqueSceneRenderTarget;
    rhi::TextureHandle m_opaqueSceneTexture;
    ShaderProgram m_oitCompositeProgram;

    // --- AO pass 资源组（M-T T-d——参考 TechniqueId::AmbientOcclusion/Blur/
    //     BlurTestOrder 的 SingularTechnique 注册面[Technique.ts:910-912/
    //     :1082-1083]；DanQing 合成器内全屏 pass 直持程序[compositeOit 同形]，
    //     EQUIVALENCE：绑定机制=直绑，shader 源与语义 1:1）---
    ShaderProgram m_aoProgram;       // kAmbientOcclusionVert/Frag（PB 变体）
    ShaderProgram m_aoBlurXProgram;  // Blur NoTest（X 向——Blur.ts:24-56）
    ShaderProgram m_aoBlurYProgram;  // Blur TestOrder（Y 向——Blur.ts:58-71）
    // 噪声纹理（System.ts:457-460——4×4 定值表 16 字节、Repeat、单通道）。
    // 参考挂 System.instance.noiseTexture——DanQing 合成器自持后处理资源
    //（OIT 纹理同例），EQUIVALENCE 登记：归属差异、内容逐字节同。
    rhi::TextureHandle m_aoNoiseTexture;
    bool m_aoProgramsCompiled = false;
    /// 惰性编译 AO 程序组 + 建噪声纹理（initOitResources 同形——首个 AO 帧
    /// 调用；renderAmbientOcclusion[T-e]的头部）。
    bool initAoResources(rhi::Driver& driver);
    /// AO 程序组编译态（测试锁面）。
    bool aoProgramsCompiled() const noexcept { return m_aoProgramsCompiled; }
    /// 全屏四边形（initOitResources/initAoResources 共用——AO-only 帧不经
    /// OIT 初始化，quad 须独立可就位）。M-T T-e 抽取。
    void ensureQuadPrimitive(rhi::Driver& driver);
    // 当前 use() 中的程序（activateProgram/deactivateProgram 维护；参考
    // ShaderProgramExecutor._program）。非所有权指针：程序归 Techniques/
    // m_oitCompositeProgram 所有。
    ShaderProgram* m_activeProgram = nullptr;
    rhi::VertexBufferInfoHandle m_quadVbih;
    rhi::VertexBufferHandle m_quadVbh;
    rhi::IndexBufferHandle m_quadIbh;
    rhi::RenderPrimitiveHandle m_quadPrimitive;

    // --- Hilite state ---
    // 参考默认色 ColorDef.from(0x23, 0xbb, 0xfc)（Hilite.ts:53: color =
    // 0x23bbfc, visibleRatio = 0.25）——此前黄色 (1,1,0) 是常量偏差。
    float m_hiliteColor[4] = {0x23 / 255.0f, 0xbb / 255.0f, 0xfc / 255.0f, 1.0f};

};

END_DQ_RENDER_NAMESPACE
