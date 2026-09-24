// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instanced geometry (instanced draw calls)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/InstancedGeometry.ts
//
// Wraps a base CachedGeometry + InstanceBuffers to issue instanced draw calls.
// All render queries are forwarded to the base (repr) geometry, while
// hasFeatures and collectStatistics include instance buffer data.
#pragma once

#include "CachedGeometry.h"
#include "InstanceBuffers.h"
#include "dqRender/RenderMemory.h"
#include "SurfaceGeometry.h"
#include "IndexedEdgeGeometry.h"
#include "RealityMeshGeometry.h"
#include "PlanarGridGraphic.h"

#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class LUTGeometry;

// ---------------------------------------------------------------------------
// InstancedGeometry — geometry drawn with instanced draw calls
// (Ported from: itwinjs-core InstancedGeometry.ts, line 345-470)
//
// All render queries (techniqueId, pass, renderOrder, material, etc.) are
// forwarded to the base representation geometry.  The instance buffers
// provide per-instance transforms, feature IDs, and symbology overrides.
// ---------------------------------------------------------------------------
class InstancedGeometry : public CachedGeometry {
public:
    /// Construct with a base representation geometry and instance buffers.
    /// @param repr The base geometry to instance (not owned — shared with Batch).
    /// @param buffers Per-instance data (owned).
    InstancedGeometry(CachedGeometry* repr, InstanceBuffers* buffers);
    ~InstancedGeometry() override;

    InstancedGeometry(InstancedGeometry const&) = delete;
    InstancedGeometry& operator=(InstancedGeometry const&) = delete;

    // --- Forwarding accessors (Ported from: itwinjs-core line 357-362) ---
    // These forward to the repr geometry so that casting works correctly
    // through instanced geometry.  Return types match base class for covariance.
    CachedGeometry* asLUT() override;
    SurfaceGeometry* asSurface() override;
    MeshGeometry* asMesh() override;
    EdgeGeometry* asEdge() override;
    IndexedEdgeGeometry* asIndexedEdge() override;
    RealityMeshGeometry* asRealityMesh() override;
    SilhouetteEdgeGeometry* asSilhouette() override;
    InstancedGeometry* asInstanced() override { return this; }
    PointCloudGeometry* asPointCloud() override;
    PlanarGridGraphic* asPlanarGrid() override;

    // --- Forwarded render properties (Ported from: itwinjs-core line 365-376) ---
    TechniqueId getTechniqueId() const override;
    Pass getPass() const override;
    RenderOrder getRenderOrder() const override;
    bool isLitSurface() const override;
    bool hasBakedLighting() const override;
    bool hasAnimation() const override;
    bool usesQuantizedPositions() const override;
    float const* getQOrigin() const override;
    float const* getQScale() const override;
    RenderMaterialInternal const* getMaterialInfo() const override;
    PolylineBuffers const* getPolylineBuffers() const override;
    bool supportsThematicDisplay() const override;
    rhi::TextureHandle getSurfaceTexture() const override;
    rhi::TextureHandle getNormalMapTexture() const override;
    float getNormalMapScale() const override;

    // --- Instance-specific queries ---
    bool hasFeatures() const override;

    // --- Draw (Ported from: itwinjs-core line 453-455) ---
    void draw(rhi::Driver& driver) override;

    // --- Range (Ported from: itwinjs-core line 457-459) ---
    InstanceBuffers::Range3d getRange() const;

    // --- Statistics (Ported from: itwinjs-core line 461-465) ---
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // --- Accessors ---
    CachedGeometry const* getRepr() const noexcept { return m_repr; }
    InstanceBuffers const* getBuffers() const noexcept { return m_buffers; }
    uint32_t getInstanceCount() const noexcept;

    /// Get the RTC (relative-to-center) model transform = modelMatrix * rtcCenter.
    /// Ported from: itwinjs-core InstancedGeometry.getRtcModelTransform (line 171)
    /// (forwards to InstanceData; cached + recomputed when modelMatrix changes).
    void getRtcModelTransform(float const* modelMatrix16, float* out16) const
    {
        m_buffers->getRtcModelTransform(modelMatrix16, out16);
    }

    /// Get the RTC model transform in Transform space (forwards to InstanceBuffers).
    /// Ported from: itwinjs-core InstancedGeometry.getRtcModelTransform (line 351)
    dqGeom::Transform getRtcModelTransform(dqGeom::Transform const& modelMatrix) const
    {
        return m_buffers->getRtcModelTransform(modelMatrix);
    }

private:
    CachedGeometry* m_repr;  // not owned — shared with Batch
    InstanceBuffers* m_buffers;  // owned
};

END_DQ_RENDER_NAMESPACE
