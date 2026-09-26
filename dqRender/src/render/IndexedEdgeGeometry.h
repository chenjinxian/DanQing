// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Indexed edge geometry for silhouette / feature edges
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/IndexedEdgeGeometry.ts
//              （EdgeLUT :31-58 / IndexedEdgeGeometry :63-134）
#pragma once

#include "MeshGeometry.h"
#include "ColorInfo.h"
#include "dqRender/RenderMemory.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class DrawParams;

// ---------------------------------------------------------------------------
// EdgeLUT — edge lookup table texture wrapping an EdgeTable
// Ported from: itwinjs-core IndexedEdgeGeometry.ts EdgeLUT (:31-58)
//
// The table is partitioned: the lower partition holds simple segment edges
// (2×24-bit vertex indices = 6 bytes each), the upper partition holds
// silhouette edges (same plus a pair of 16-bit oct-encoded normals = 10 bytes;
// EdgeParams.ts:44-64 EdgeTable).  EdgeLUT owns the GPU texture created from
// the table bytes.
// ---------------------------------------------------------------------------
class EdgeLUT {
public:
    EdgeLUT() = default;

    /// Ported from: IndexedEdgeGeometry.ts EdgeLUT.create (:46-49)——
    /// `TextureHandle.createForData(table.width, table.height, table.data)` +
    /// numSegments/silhouettePadding。data 为 width*height*4 字节（RGBA8 texel）。
    /// 创建失败（驱动拒绝）返回无效 EdgeLUT（create 内部不自持半成品）。
    static EdgeLUT create(rhi::Driver& driver, uint8_t const* data,
                          uint32_t width, uint32_t height,
                          uint32_t numSegments, uint32_t silhouettePadding);

    /// The GPU texture handle for the edge lookup table. (:32)
    rhi::TextureHandle getTexture() const { return m_texture; }

    /// Table dimensions（参考经 edge.edgeLut.texture.width/height 读取——
    /// Edge.ts:255-256 的 u_edgeParams.xy 数据源）。
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    /// Number of segments in the lower partition. (:33)
    uint32_t getNumSegments() const { return m_numSegments; }

    /// Number of padding bytes inserted between the partitions. (:34)
    uint32_t getSilhouettePadding() const { return m_silhouettePadding; }

    /// Approximate GPU memory used by the texture (bytes) — :51-53
    /// `texture.bytesUsed`（RGBA8 = width*height*4）。
    uint32_t getBytesUsed() const { return m_bytesUsed; }

    /// True if the texture handle is still valid (not disposed). (:56-57)
    bool isValid() const { return static_cast<bool>(m_texture); }

    /// Release the GPU texture（:42-44 [Symbol.dispose] → dispose(texture)；
    /// §12.9：同步失效全部缓存句柄成员）。
    void dispose(rhi::Driver& driver);

private:
    rhi::TextureHandle m_texture;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_numSegments = 0;
    uint32_t m_silhouettePadding = 0;
    uint32_t m_bytesUsed = 0;
};

// ---------------------------------------------------------------------------
// IndexedEdgeGeometry — GPU geometry for indexed edge rendering
// Ported from: itwinjs-core IndexedEdgeGeometry.ts IndexedEdgeGeometry (:63-134)
//
// Renders edge primitives (silhouettes + segment edges, compactly represented
// as indices into an edge lookup table) via TechniqueId::IndexedEdge.  a_pos is
// a 24-bit index into the edge LUT（每边 6 个相同索引 = quad；索引流为 UBYTE3
// array buffer ——:80 BufferParameters.create(attrPos.location, 3, UnsignedByte)），
// _draw 为 drawArrays(Triangles, 0, numIndices)（:109-114，无 element index
// buffer）；顶点表（u_vertLUT）由 surface 几何持有、本几何经 MeshGeometry::setLut
// 非拥有观察（参考 :74 ctor 的 mesh: MeshData——MeshData.ts:60 lut）。
// ---------------------------------------------------------------------------
class IndexedEdgeGeometry : public MeshGeometry {
public:
    /// LUT 形态构造。Ported from: IndexedEdgeGeometry.ts create (:98-102) + ctor
    /// (:74-86)：`indexBuffer = BufferHandle.createArrayBuffer(params.indices.data)`、
    /// `lut = EdgeLUT.create(params.edges)`、`numIndices = params.indices.length`、
    /// width/lineCode ← appearance ?? MeshData.edgeWidth/edgeLineCode（:84-85 →
    /// MeshData.ts:93-94）、colorInfo ← appearance?.color ?? mesh.lut.colorInfo
    /// （:83——DanQing imdl 通路为均匀色 → ColorInfo::fromUniform，创建点换算）。
    /// `vertexLut` 为 surface 几何持有的顶点 LUT（非拥有观察，EdgeGeometry LUT
    /// 形态同款；生命周期：MeshGraphic 成员序 edges 先于 surfaces 析构）。
    /// GL 资源（索引 BO + VAO）由创建点组装后经 setPrimitive/setVbhResources
    /// 注入；析构清单见 ~IndexedEdgeGeometry。
    IndexedEdgeGeometry(rhi::Driver& driver, EdgeLUT edgeLut,
                        rhi::BufferObjectHandle indices, uint32_t numIndices,
                        VertexLutTexture const& vertexLut, float edgeWeight,
                        uint32_t edgeLineCode, ColorInfo colorInfo, bool isPlanar);
    ~IndexedEdgeGeometry() override;

    // --- Casting accessor (IndexedEdgeGeometry.ts:72 asIndexedEdge) ---
    IndexedEdgeGeometry* asIndexedEdge() override { return this; }

    IndexedEdgeGeometry(IndexedEdgeGeometry const&) = delete;
    IndexedEdgeGeometry& operator=(IndexedEdgeGeometry const&) = delete;

    // --- CachedGeometry interface ---
    // Ported from: IndexedEdgeGeometry.ts :130 techniqueId → TechniqueId.IndexedEdge。
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::IndexedEdge; }
    // Ported from: :131 getPass → computeEdgePass（MeshGeometry.ts:58-69——
    // DanQing 边缘家族既有形态：translucent/wireframe 细分未移植，pass 恒
    // OpaqueLinear，"none" 门在 SceneCompositorImpl dispatch 侧）。
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    // Ported from: :132 renderOrder → isPlanar ? PlanarEdge : Edge。
    RenderOrder getRenderOrder() const noexcept override
    {
        return isPlanar() ? RenderOrder::PlanarEdge : RenderOrder::Edge;
    }
    // 量化 LUT 几何（参考 LUTGeometry.usesQuantizedPositions ← mesh.lut——
    // CachedGeometry.ts:207-209；imdl 顶点表恒量化）。
    bool usesQuantizedPositions() const noexcept override { return true; }

    /// Issue the draw call through the Driver. Ported from: :109-114 _draw——
    /// buffers.bind() + drawArrays(Triangles, 0, numIndices)。
    void draw(rhi::Driver& driver) override;

    /// Collect GPU memory statistics. Ported from: :104-107——
    /// addIndexedEdges(indices.bytesUsed) + addEdgeTable(edgeLut.bytesUsed)。
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    /// Accessors
    EdgeLUT const& getEdgeLut() const { return m_edgeLut; }
    /// a_pos 24-bit 索引流（array buffer——参考 :64 _indices: BufferHandle）。
    rhi::BufferObjectHandle getIndices() const { return m_indices; }
    ColorInfo const& getColorInfo() const { return m_colorInfo; }

    /// 创建点注入（VAO 本体 + 其 RHI 顶点缓冲句柄——析构清单成员，
    /// EdgeGeometry LUT 形态同款）。
    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }
    void setVbhResources(rhi::VertexBufferHandle vbh, rhi::VertexBufferInfoHandle vbih)
    {
        m_vbh = vbh;
        m_vbih = vbih;
    }

private:
    rhi::Driver* m_driver = nullptr;
    EdgeLUT m_edgeLut;
    rhi::BufferObjectHandle m_indices;
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::VertexBufferHandle m_vbh;
    rhi::VertexBufferInfoHandle m_vbih;
    ColorInfo m_colorInfo;
};

END_DQ_RENDER_NAMESPACE
