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
#include "ThematicUniforms.h"
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

    /// Get the clip stack.
    ClipStack& getClipStack() { return m_clipStack; }

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

    /// Render translucent geometry to OIT FBO.
    /// Ported from: itwinjs-core Compositor.renderTranslucent()
    void renderTranslucent(RenderCommands& commands);

    /// Render hilite geometry.
    /// Ported from: itwinjs-core Compositor.renderHilite()
    void renderHilite(RenderCommands& commands);

    /// Composite OIT and hilite to main framebuffer.
    /// Ported from: itwinjs-core Compositor.composite()
    void composite(bool wantTranslucent);

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
    void compositeOit(rhi::Driver& driver);

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
    BranchStack m_branchStack;
    // BatchState belongs to the TARGET (reference: compositor.target.uniforms.batch.state,
    // Target.ts:793). The compositor references it — RenderCommands (populate-time
    // assignBatchId/findBatch registration) and the compositor's PushBatchCommand lookup
    // (draw-time) must see ONE shared registry, or batch-registered hilite/uniform state
    // never reaches the dispatch.
    BatchState& m_batchState;
    ClipStack m_clipStack;
    BranchUniforms m_branchUniforms;  // per-geometry matrix computation
    SyncObserver m_branchObserver;    // skip redundant BranchUniforms updates

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

    // --- OIT composite shader (kept separate from textures/FBOs) ---
    bool m_oitInitialized = false;
    ThematicUniforms m_thematicUniforms;  // gradient texture for thematic display
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
