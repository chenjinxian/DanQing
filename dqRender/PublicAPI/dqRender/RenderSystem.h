// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderSystem abstract interface
//
// Ported from: itwinjs-core core/frontend/src/render/RenderSystem.ts
// The rendering system interface.
#pragma once

#include "Export.h"
#include "CreateTextureArgs.h"
#include "GraphicBuilder.h"
#include "GraphicBranch.h"
#include "PlanarGridProps.h"
#include "RenderSkyBoxParams.h"

#include <functional>
#include <string>
#include "RenderTarget.h"
#include "RenderGraphic.h"
#include "rhi/Handle.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/FeatureTable.h>
#include <dqCommon/Frustum.h>
#include <dqGeom/Range3d.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

#include <cstdint>
#include <memory>

BEGIN_DQ_RENDER_NAMESPACE

// GPU profiling result node: label + elapsed nanoseconds + nested children.
// Ported from: itwinjs-core GLTimerResult (RenderSystemDebugControl.ts:20-30).
struct GLTimerResult {
    std::string label;                     // label from GLTimer.beginOperation
    uint64_t nanoseconds = 0;              // elapsed ns, inclusive of children
    std::vector<GLTimerResult> children;   // nested beginOperation results
};

// Callback receiving per-frame GPU profiling results.
// Ported from: GLTimerResultCallback (RenderSystemDebugControl.ts:32).
using GLTimerResultCallback = std::function<void(GLTimerResult const&)>;

// Debug controls optionally exposed by a RenderSystem.
// Ported from: itwinjs-core RenderSystemDebugControl (RenderSystemDebugControl.ts:48-84).
// DanQing: RHI timer queries (EXT_disjoint_timer_query) are not implemented —
// isGLTimerSupported stays false and resultsCallback is never invoked, which
// is exactly the reference's "EXT_disjoint_timer_query is not available in
// this browser" path (the GpuProfiler widget disables its checkbox + shows
// that tooltip). The surface exists 1:1 so enabling the RHI timer wires in
// without touching the widget.
struct RenderSystemDebugControl {
    // Returns true if the backend supports GPU profiling queries.
    bool isGLTimerSupported = false;

    // Record GPU profiling information for each frame drawn. Check
    // isGLTimerSupported before using.
    GLTimerResultCallback resultsCallback;
};

// Abstract rendering system interface.
// Ported from: itwinjs-core RenderSystem.ts
//
// The rendering system is the central factory for all rendering resources.
// It owns GPU resource caches and creates targets, graphics, textures, and materials.
class DQ_RENDER_EXPORT RenderSystem {
public:
    virtual ~RenderSystem() = default;

    // Get the global render system singleton.
    // ← IModelApp.renderSystem
    static RenderSystem& get();

    // Set the global render system instance (called during initialization).
    static void setInstance(RenderSystem* instance);

    // Whether the system is valid/initialized.
    virtual bool isValid() const noexcept = 0;

    // Create a render target.
    virtual std::unique_ptr<RenderTarget> createTarget(void* nativeWindow, uint32_t width, uint32_t height) = 0;

    // Create a graphic builder.
    virtual std::unique_ptr<GraphicBuilder> createGraphicBuilder(const GraphicBuilderOptions& options) = 0;

    // Create a graphic branch.
    virtual GraphicBranch* createBranch(bool ownsEntries = true) = 0;

    // Create a graphic from a branch (takes ownership).
    virtual RenderGraphic* createBranchGraphic(GraphicBranch* branch) = 0;

    // Create a graphic from a list of graphics.
    // ← RenderSystem.createGraphicList(primitives)
    virtual RenderGraphic* createGraphicList(std::vector<RenderGraphic*> graphics) = 0;

    // Create a graphic owner (prevents auto-disposal).
    // ← RenderSystem.createGraphicOwner(ownedGraphic)
    virtual RenderGraphicOwner* createGraphicOwner(RenderGraphic* owned) = 0;

    // Set the techniques registry (for shader management).
    // Default implementation is a no-op; concrete systems override.
    virtual void setTechniques(void* /*techniques*/) {}

    // Create a ground grid graphic.
    // ← itwinjs-core: PlanarGrid geometry
    virtual RenderGraphic* createGridGraphic(float extent = 100.0f, int gridLines = 20)
    {
        (void)extent; (void)gridLines;
        return nullptr;
    }

    // Create a procedural planar grid graphic from the current view frustum.
    // The grid polygon is `frustum ∩ grid plane`; the caller rebuilds whenever the
    // frustum changes so the infinite grid plane tracks the camera.
    // Ported from: itwinjs-core RenderSystem.createPlanarGrid (System.ts:481) →
    //               PlanarGridGeometry.create (PlanarGrid.ts:52).
    virtual RenderGraphic* createPlanarGrid(dqCommon::Frustum const& /*frustum*/,
                                            PlanarGridProps const& /*grid*/)
    {
        return nullptr;
    }

    // Update an existing planar grid graphic's geometry from a new view frustum,
    // reusing GPU handles (in-place buffer update). Cheaper than recreate and
    // avoids RHI state-tracker churn. Returns false if `grid` is not a planar grid.
    // Ported from: itwinjs-core ViewContext.drawStandardGrid re-gathering the grid
    //               decoration each frame (ViewContext.ts:348).
    virtual bool updatePlanarGridFrustum(RenderGraphic* /*grid*/,
                                         dqCommon::Frustum const& /*frustum*/,
                                         PlanarGridProps const& /*gridProps*/)
    {
        return false;
    }

    // Update the sky sphere's per-frame world corners (a_worldPos) + gradient eye
    // (u_worldEye) from the camera-consistent WORLD frustum. The sky gradient is
    // world-fixed (rotates with the view). Returns false if `sky` is not a sky
    // sphere. Ported from: itwinjs-core SkySphereViewportQuadGeometry worldPos
    //                      (CachedGeometry.ts:583-596 non-globe / :597-651 globe)
    //                      + u_worldEye (SkySphere.ts:237-265).
    // `globe` = 计划的 globe-mode 状态（RenderPlan.ts:106/132-141/109）——
    // isGlobeMode3D 时走 globe 分支（默认 false = 非 globe 路径）。
    virtual bool updateSkySphere(RenderGraphic* /*sky*/,
                                 dqCommon::Frustum const& /*worldFrustum*/,
                                 SkySphereGlobeParams const& /*globe*/ = {})
    {
        return false;
    }

    // Create a render graphic from an IndexedPolyface mesh.
    // ← itwinjs-core: RenderSystem.createMeshGeometry() + createRenderGraphic()
    // @param polyface The IndexedPolyface mesh data.
    // @param defaultColor Default vertex color (RGBA packed).
    // @param featureId Feature ID for picking (0 = no picking).
    // @param texture Surface baseColor texture (null handle = untextured).
    virtual RenderGraphic* createGraphicFromPolyface(
        void const* polyface, uint32_t defaultColor = 0xFF8080FF,
        uint32_t featureId = 0, rhi::TextureHandle texture = {})
    {
        (void)polyface; (void)defaultColor; (void)featureId; (void)texture;
        return nullptr;
    }

    // Create a GPU texture from decoded image data.
    // ← itwinjs-core RenderSystem.createTexture({ type, image }) (System.ts)
    virtual rhi::TextureHandle createTexture(CreateTextureArgs const& /*args*/)
    {
        return {};
    }

    // --- Geometry factory methods ---
    // Ported from: itwinjs-core System.ts createMeshGeometry/createPolylineGeometry/etc.

    /// Create a render graphic from cached geometry.
    /// Wraps CachedGeometry in a Primitive or MeshGraphic.
    /// ← itwinjs-core: System.createRenderGraphic(geometry)
    virtual RenderGraphic* createRenderGraphic(void* /*cachedGeometry*/)
    {
        return nullptr;
    }

    /// Create a batch graphic (wraps graphic with feature table for picking).
    /// The batch is pickable (Graphic.ts:311 Batch.isPickable = true) and its
    /// feature table drives the per-batch feature-override LUT (hilite).
    /// ← itwinjs-core: System.createBatch(graphic, features, range)
    ///   (System.ts:555-561 — glTF 装饰的 pickable/hilite 来源)
    virtual RenderGraphic* createBatch(RenderGraphic* /*graphic*/,
                                        dqCommon::FeatureTable const* /*featureTable*/,
                                        dqGeom::Range3d const& /*range*/)
    {
        return nullptr;
    }

    /// Create a sky box graphic.
    /// ← itwinjs-core: System.createSkyBox(params)
    virtual RenderGraphic* createSkyBox(void const* /*skyBoxParams*/)
    {
        return nullptr;
    }

    /// Create a depth buffer for off-screen rendering.
    /// ← itwinjs-core: System.createDepthBuffer(width, height, numSamples)
    virtual void* createDepthBuffer(uint32_t /*width*/, uint32_t /*height*/, uint32_t /*numSamples*/ = 1)
    {
        return nullptr;
    }

    /// Create a render clip volume from a clip vector.
    /// ← itwinjs-core: System.createClipVolume(clipVector)
    virtual void* createClipVolume(void const* /*clipVector*/)
    {
        return nullptr;
    }

    /// Create a render graphic from a GraphicTemplate.
    /// If instances is provided, creates an instanced graphic; otherwise returns the template's graphic directly.
    /// ← itwinjs-core: RenderSystem.createGraphicFromTemplate({ template, instances })
    virtual RenderGraphic* createGraphicFromTemplate(
        void const* /*graphicTemplate*/,
        void const* /*instancedGraphicParams*/ = nullptr)
    {
        return nullptr;
    }

    // Record graphics memory consumed by the render system (system-wide
    // textures/resources).
    // Ported from: itwinjs-core System.collectStatistics (System.ts:255 —
    // the WebGL system's texture/gradient/resource-cache walk). Default
    // no-op; concrete systems override with their driver's accounting.
    virtual void collectStatistics(RenderMemory::Statistics& stats) const { (void)stats; }

    // Debug controls for this system (GPU profiler etc.), or nullptr when the
    // system exposes none (the reference's `renderSystem.debugControl`
    // undefined case).
    // Ported from: itwinjs-core IModelApp.renderSystem.debugControl
    // (RenderSystemDebugControl.ts — internal interface surfaced for devtools).
    virtual RenderSystemDebugControl* debugControl() { return nullptr; }
};

END_DQ_RENDER_NAMESPACE
