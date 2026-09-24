// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Indexed edge geometry for silhouette / feature edges
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/IndexedEdgeGeometry.ts
//
// IndexedEdgeGeometry renders edge primitives using an EdgeLUT texture that
// maps edge type bits to visual properties (color, width, etc.).  The index
// buffer packs per-edge vertex indices as uint8 triples.
#pragma once

#include "CachedGeometry.h"
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
// EdgeLUT — edge lookup texture wrapping an EdgeTable
// (Ported from: itwinjs-core IndexedEdgeGeometry.ts EdgeLUT)
//
// The EdgeTable defines a 2D texture whose texels encode per-edge rendering
// attributes (color, width, line code).  EdgeLUT owns the GPU texture handle.
// ---------------------------------------------------------------------------
class EdgeLUT {
public:
    /// Construct with an already-created GPU texture.
    EdgeLUT(rhi::TextureHandle texture, uint32_t numSegments, uint32_t silhouettePadding);

    /// The GPU texture handle for the edge lookup table.
    rhi::TextureHandle getTexture() const { return m_texture; }

    /// Number of discrete edge segments encoded in the table.
    uint32_t getNumSegments() const { return m_numSegments; }

    /// Extra padding (in texels) added for silhouette edges.
    uint32_t getSilhouettePadding() const { return m_silhouettePadding; }

    /// Approximate GPU memory used by the texture (bytes).
    uint32_t getBytesUsed() const { return m_bytesUsed; }

    /// True if the texture handle is still valid (not disposed).
    bool isValid() const { return static_cast<bool>(m_texture); }

    /// Release the GPU texture.
    void dispose();

private:
    rhi::TextureHandle m_texture;
    uint32_t m_numSegments;
    uint32_t m_silhouettePadding;
    uint32_t m_bytesUsed = 0;
};

// ---------------------------------------------------------------------------
// IndexedEdgeGeometry — GPU geometry for indexed edge rendering
// (Ported from: itwinjs-core IndexedEdgeGeometry.ts IndexedEdgeGeometry)
//
// Renders edge primitives (silhouettes, feature edges) via the
// TechniqueId::IndexedEdge technique.  The index buffer stores packed uint8
// vertex indices; the vertex positions come from the parent mesh geometry.
// ---------------------------------------------------------------------------
class IndexedEdgeGeometry : public CachedGeometry {
public:
    /// Construct with ownership of the edge LUT, index buffer, and appearance.
    IndexedEdgeGeometry(EdgeLUT edgeLut,
                        rhi::IndexBufferHandle indexBuffer,
                        uint32_t numIndices,
                        ColorInfo colorInfo,
                        float width,
                        uint32_t lineCode);

    /// Technique used to render this geometry.
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::IndexedEdge; }

    /// Edges render in the opaque pass.
    Pass getPass() const noexcept override { return Pass::Opaque; }

    /// Edge render order (planar edges get a higher z-fight order).
    RenderOrder getRenderOrder() const noexcept override { return m_isPlanar ? RenderOrder::Edge : RenderOrder::Edge; }

    /// Issue the indexed draw call through the Driver.
    void draw(rhi::Driver& driver) override;

    /// Collect GPU memory statistics for the index buffer and edge LUT.
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    /// Release GPU resources.
    void dispose();

    /// Accessors
    EdgeLUT const& getEdgeLut() const { return m_edgeLut; }
    rhi::IndexBufferHandle getIndexBuffer() const { return m_indexBuffer; }
    uint32_t getNumIndices() const { return m_numIndices; }
    ColorInfo const& getColorInfo() const { return m_colorInfo; }
    float getWidth() const { return m_width; }
    uint32_t lineCode() const { return m_lineCode; }

private:
    EdgeLUT m_edgeLut;
    rhi::IndexBufferHandle m_indexBuffer;
    uint32_t m_numIndices;
    ColorInfo m_colorInfo;
    float m_width;
    uint32_t m_lineCode;
    bool m_isPlanar = false;
};

END_DQ_RENDER_NAMESPACE
