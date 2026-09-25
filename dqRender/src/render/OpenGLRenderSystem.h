// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Concrete OpenGL RenderSystem implementation
// Ported from: filament filament/src/details/Engine.h + itwinjs-core core/frontend/src/render/RenderSystem.ts
//
// Bridges the public RenderSystem interface to the internal RenderSystemImpl.
#pragma once

#include "dqRender/RenderSystem.h"
#include "dqRender/RenderTarget.h"

#include "RenderSystemImpl.h"

#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Concrete RenderSystem using OpenGL.
// Bridges public RenderSystem interface → internal RenderSystemImpl.
class OpenGLRenderSystem : public RenderSystem {
public:
    explicit OpenGLRenderSystem(std::unique_ptr<rhi::Driver> driver);
    explicit OpenGLRenderSystem(RenderSystemImpl& impl) : m_impl(&impl), m_ownsImpl(false) {}
    ~OpenGLRenderSystem() override { if (m_ownsImpl) delete m_impl; }

    // RenderSystem interface
    std::unique_ptr<RenderTarget> createTarget(void* nativeWindow, uint32_t width, uint32_t height) override;
    std::unique_ptr<GraphicBuilder> createGraphicBuilder(const GraphicBuilderOptions& options) override;
    GraphicBranch* createBranch(bool ownsEntries = true) override;
    RenderGraphic* createBranchGraphic(GraphicBranch* branch) override;
    RenderGraphic* createGraphicList(std::vector<RenderGraphic*> graphics) override;
    RenderGraphicOwner* createGraphicOwner(RenderGraphic* owned) override;
    bool isValid() const noexcept override;

    // The RHI driver backing this system（RenderSystem::driver 的 concrete 实现——
    // itwinjs 侧经 concrete WebGL System 内部持有 context，DanQing 经 RenderSystemImpl）。
    rhi::Driver* driver() noexcept override { return m_impl ? &m_impl->getDriver() : nullptr; }

    // Access internal implementation
    RenderSystemImpl& getImpl() noexcept { return *m_impl; }

    // Record graphics memory consumed by the render system.
    // Ported from: itwinjs-core System.collectStatistics (System.ts:255 —
    // textures/gradients walk). DanQing: the GL driver's live-texture walk
    // (OpenGLDriver::collectTextureStatistics — per-texture bytes → Textures
    // consumer).
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // Debug controls (GPU profiler) — forwards to RenderSystemImpl's GLTimer
    // (System.ts:967-970 equivalent: isGLTimerSupported + resultsCallback).
    RenderSystemDebugControl* debugControl() override
    {
        return m_impl ? m_impl->debugControl() : nullptr;
    }

    // Set the techniques registry (called during RenderPipeline initialization).
    void setTechniques(void* techniques) override
    {
        m_techniques = static_cast<Techniques*>(techniques);
    }

    // Create a ground grid graphic.
    RenderGraphic* createGridGraphic(float extent, int gridLines) override;

    // Create a procedural planar grid from the current view frustum.
    // Ported from: itwinjs-core RenderSystem.createPlanarGrid (System.ts:481) →
    //               PlanarGridGeometry.create (PlanarGrid.ts:52).
    RenderGraphic* createPlanarGrid(dqCommon::Frustum const& frustum, PlanarGridProps const& grid) override;

    // Update an existing planar grid's geometry from a new frustum (in place).
    // Ported from: itwinjs-core ViewContext.drawStandardGrid (ViewContext.ts:348).
    bool updatePlanarGridFrustum(RenderGraphic* grid, dqCommon::Frustum const& frustum,
                                 PlanarGridProps const& gridProps) override;

    // Update the sky sphere's per-frame worldPos (a_worldPos) + eye (u_worldEye).
    // Ported from: itwinjs-core SkySphereViewportQuadGeometry worldPos +
    //               SkySphere.ts u_worldEye (ortho pseudo-camera)。
    // globe = 计划的 globe-mode 状态（非空走 CachedGeometry.ts:597-651 globe 分支）。
    bool updateSkySphere(RenderGraphic* sky, dqCommon::Frustum const& worldFrustum,
                         SkySphereGlobeParams const& globe = {}) override;

    // Create a render graphic from an IndexedPolyface mesh.
    RenderGraphic* createGraphicFromPolyface(
        void const* polyface, uint32_t defaultColor, uint32_t featureId,
        rhi::TextureHandle texture) override;

    // Create a GPU texture from decoded image data.
    // ← itwinjs-core RenderSystem.createTexture (System.ts)
    rhi::TextureHandle createTexture(CreateTextureArgs const& args) override;

    // --- Geometry factory methods ---
    // Ported from: itwinjs-core System.ts
    RenderGraphic* createRenderGraphic(void* cachedGeometry) override;
    RenderGraphic* createBatch(RenderGraphic* graphic,
                               dqCommon::FeatureTable const* featureTable,
                               dqGeom::Range3d const& range) override;
    RenderGraphic* createSkyBox(void const* skyBoxParams) override;

    // Create a render graphic from a GraphicTemplate.
    // Ported from: itwinjs-core RenderSystem.createGraphicFromTemplate()
    RenderGraphic* createGraphicFromTemplate(
        void const* graphicTemplate,
        void const* instancedGraphicParams) override;

private:
    RenderSystemImpl* m_impl = nullptr;
    bool m_ownsImpl = true;
    Techniques* m_techniques = nullptr;  // Not owned — owned by RenderPipeline
};

END_DQ_RENDER_NAMESPACE
