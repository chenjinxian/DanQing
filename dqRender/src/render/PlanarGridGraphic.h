// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PlanarGrid GPU geometry
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/PlanarGrid.ts
//
// Builds a ground grid (20×20 lines) and coordinate axes (X=red, Y=green, Z=blue)
// as GPU-ready geometry.  Uses RHI Driver for all resource creation.
#pragma once

#include "CachedGeometry.h"
#include "dqRender/RenderMemory.h"
#include "dqRender/PlanarGridProps.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/Frustum.h>
#include <dqGeom/Point2d.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PlanarGridPolygon - pure geometry of the procedural grid (no GL).
// Ported from: itwinjs-core PlanarGridGeometry.create (PlanarGrid.ts:52-111).
//   The convex polygon where the view frustum intersects the grid plane, with a
//   per-vertex UV (grid-line coordinate) and a fan triangulation. The grid plane is
//   Plane3dByOriginAndUnitNormal(grid.origin, grid.rMatrix.rowZ()); UVs are
//   ((p·rowX - origin·rowX)/spacing.x, (p·rowY - origin·rowY)/spacing.y). Quantization
//   (QPoint2dList/QPoint3dList) is deferred - UVs are float (result-faithful for the
//   blank-connection grid; precision only matters for far-from-origin reality models).
// ---------------------------------------------------------------------------
struct PlanarGridPolygon {
    std::vector<dqGeom::Point3d> positions;   // polygon vertices (world)
    std::vector<dqGeom::Point2d> uvs;         // per-vertex 量化 UV（[0,1]，见 build 注释）
    std::vector<uint32_t> indices;            // fan triangulation (0, i+1, i+2)
    // u_qTexCoordParams = (origin.xy, extent.xy)——unquantize2d 解码参数
    //（PlanarGrid.ts:70-77 的 QParams2d + origin%gridsPerRef 相位对齐）。
    std::array<float, 4> qTexCoordParams = {0.0f, 0.0f, 1.0f, 1.0f};
};

/// Build the procedural grid polygon (frustum ∩ grid plane) + UVs + fan indices.
/// Returns nullopt if the frustum does not intersect the grid plane (<3 polygon points).
/// Ported from: itwinjs-core PlanarGridGeometry.create (PlanarGrid.ts:52-111).
std::optional<PlanarGridPolygon> BuildPlanarGridPolygon(
    dqCommon::Frustum const& frustum, PlanarGridProps const& grid);

// ---------------------------------------------------------------------------
// PlanarGridGraphic — ground grid + coordinate axes
// (Ported from: itwinjs-core PlanarGrid.ts)
// ---------------------------------------------------------------------------
class PlanarGridGraphic : public CachedGeometry {
public:
    /// Construct and upload grid geometry to GPU.
    /// @param driver RHI driver for resource creation.
    /// @param extent Half-extent of the grid (grid goes from -extent to +extent).
    /// @param gridLines Number of grid lines per axis (default 20).
    /// @deprecated Static one-time build from a synthetic ±extent cube frustum. The
    ///             faithful path rebuilds per-frame from the live view frustum — use
    ///             the (driver, frustum, props) ctor below (kept for test compat).
    PlanarGridGraphic(rhi::Driver& driver, float extent = 100.0f, int gridLines = 20);

    /// Construct and upload the procedural grid from the CURRENT view frustum.
    /// The polygon is `frustum ∩ grid plane` (props.origin/props.rMatrix), rebuilt
    /// by the caller whenever the frustum changes — the infinite grid plane tracks
    /// the camera. Faithful per-frame path.
    /// Ported from: itwinjs-core PlanarGridGeometry.create (PlanarGrid.ts:52-111),
    ///               invoked from ViewContext.drawStandardGrid (ViewContext.ts:348).
    PlanarGridGraphic(rhi::Driver& driver, dqCommon::Frustum const& frustum,
                      PlanarGridProps const& props);

    ~PlanarGridGraphic() override;

    PlanarGridGraphic(PlanarGridGraphic const&) = delete;
    PlanarGridGraphic& operator=(PlanarGridGraphic const&) = delete;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const override { return TechniqueId::PlanarGrid; }
    // Ported from: itwinjs-core PlanarGridGeometry.getPass (PlanarGrid.ts:39)
    //   → "translucent"：网格是半透明面（plane/line/ref 三级 transparency），必须
    //   走半透明 pass 经混合/OIT 合成。此前误归 OpaquePlanar：不透明 pass 无混合，
    //   平面以不透明对比色铺满 → 线与面同色无对比（用户实测"看不到网格"根因）。
    Pass getPass() const override { return Pass::Translucent; }
    // Ported from: itwinjs-core PlanarGridGeometry.renderOrder (PlanarGrid.ts:41)
    //   → RenderOrder.UnlitSurface（非 PlanarBit——pass 内排序键与 depthAndOrder
    //   编码均以参考值为准；网格在 translucent pass 与 UnlitSurface 同位）。
    RenderOrder getRenderOrder() const override { return RenderOrder::UnlitSurface; }
    // Type guard so the compositor can upload the procedural-grid uniforms.
    PlanarGridGraphic* asPlanarGrid() override { return this; }

    /// Issue the draw call.
    void draw(rhi::Driver& driver) override;

    /// Collect memory statistics.
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    /// Set the model-view-projection matrix (uploaded as uniform before draw).
    void setMvpMatrix(float const* mvp16) noexcept;

    /// Rebuild the polygon from a NEW view frustum, reusing GPU handles (in-place
    /// buffer update). Avoids the per-frame GL handle churn of recreate — DanQing's
    /// RHI state tracker misbehaves under rapid handle create/destroy (unlike raw
    /// WebGL, where itwinjs recreates PlanarGridGeometry each frame). Buffers are
    /// allocated once at kMaxGridVerts capacity and updated as a subset each frame.
    /// Ported from: itwinjs-core ViewContext.drawStandardGrid re-gathering the grid
    ///               decoration each frame (ViewContext.ts:348).
    void updateFrustum(dqCommon::Frustum const& frustum, PlanarGridProps const& props);

    /// Get vertex count (for testing).
    uint32_t getVertexCount() const noexcept { return m_vertexCount; }

    /// Get index count (for testing).
    uint32_t getIndexCount() const noexcept { return m_indexCount; }

    // --- Procedural-grid uniforms (uploaded by the draw path) ---
    // Ported from: itwinjs-core glsl/PlanarGrid.ts u_gridColor/u_gridProps/u_qTexCoordParams.
    /// Grid line color as normalized RGB (color/255).
    std::array<float, 3> getGridColorRgb() const noexcept;
    /// u_gridProps = (gridsPerRef, 1-planeTransparency, 1-lineTransparency, 1-refTransparency).
    std::array<float, 4> getGridProps() const noexcept;
    /// u_qTexCoordParams——量化 UV 的解码参数（origin.xy + extent.xy·qpos），
    /// 由 BuildPlanarGridPolygon 按多边形 UV range 计算（PlanarGrid.ts:70-77）。
    std::array<float, 4> getQTexCoordParams() const noexcept { return m_qTexCoordParams; }

private:
    // Max polygon capacity (frustum ∩ grid plane). Observed max = 8 verts; 16 gives
    // margin. Buffers are allocated ONCE at this capacity and updated in place each
    // rebuild (stable GPU handles avoid RHI state-tracker churn).
    static constexpr uint32_t kMaxGridVerts = 16;
    static constexpr uint32_t kMaxGridIndices = (kMaxGridVerts - 2u) * 3u;

    void buildGeometry(float extent, int gridLines);
    /// Build the frustum∩grid-plane polygon (position+uv) + fan indices from the
    /// given frustum using m_props. Shared by both ctors (the extent ctor feeds a
    /// synthetic cube frustum). Ported from: itwinjs-core PlanarGrid.ts:52-111.
    void buildGeometryFromFrustum(dqCommon::Frustum const& frustum);
    void uploadToGpu(rhi::Driver& driver);

    // Vertex layout: position (vec3) + uv (vec2) = 5 floats = 20 bytes.
    struct Vertex {
        std::array<float, 3> position;
        std::array<float, 2> uv;
    };

    PlanarGridProps m_props;  // procedural-grid settings (for uniform getters)
    std::array<float, 4> m_qTexCoordParams = {0.0f, 0.0f, 1.0f, 1.0f};  // 量化解码参数
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    // RHI resources
    rhi::VertexBufferInfoHandle m_vbih;
    rhi::VertexBufferHandle m_vbh;
    rhi::IndexBufferHandle m_ibh;
    rhi::BufferObjectHandle m_vbo;  // actual vertex data
    rhi::RenderPrimitiveHandle m_primitive;

    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;

    // MVP matrix (cached for uniform upload)
    std::array<float, 16> m_mvpMatrix = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    rhi::Driver& m_driver;
};

END_DQ_RENDER_NAMESPACE
