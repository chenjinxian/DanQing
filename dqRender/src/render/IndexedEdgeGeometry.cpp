// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Indexed edge geometry for silhouette / feature edges
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/IndexedEdgeGeometry.ts
//
// Integration point: itwinjs drew via System.instance.drawArrays().  This is
// redirected to Driver.draw2() with the appropriate primitive type and range.
#include "IndexedEdgeGeometry.h"

#include "dqRender/rhi/Driver.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// EdgeLUT
// ---------------------------------------------------------------------------

EdgeLUT::EdgeLUT(rhi::TextureHandle texture, uint32_t numSegments, uint32_t silhouettePadding)
    : m_texture(texture)
    , m_numSegments(numSegments)
    , m_silhouettePadding(silhouettePadding)
    , m_bytesUsed(0)
{
    // Approximate bytes used — actual value comes from the TextureHandle in
    // the itwinjs source (texture.bytesUsed).  For now we report 0 until
    // TextureHandle exposes a size query.
}

void EdgeLUT::dispose()
{
    m_texture = rhi::TextureHandle{};
}

// ---------------------------------------------------------------------------
// IndexedEdgeGeometry
// ---------------------------------------------------------------------------

IndexedEdgeGeometry::IndexedEdgeGeometry(EdgeLUT edgeLut,
                                         rhi::IndexBufferHandle indexBuffer,
                                         uint32_t numIndices,
                                         ColorInfo colorInfo,
                                         float width,
                                         uint32_t lineCode)
    : m_edgeLut(std::move(edgeLut))
    , m_indexBuffer(indexBuffer)
    , m_numIndices(numIndices)
    , m_colorInfo(std::move(colorInfo))
    , m_width(width)
    , m_lineCode(lineCode)
{
}

void IndexedEdgeGeometry::draw(rhi::Driver& driver)
{
    if (!m_indexBuffer || 0 == m_numIndices)
        return;

    // itwinjs: System.instance.drawArrays(GL.PrimitiveType.Triangles, 0, this._numIndices, numInstances);
    // Redirected to Driver.draw2() — range [0, m_numIndices), 1 instance.
    driver.draw2(0, m_numIndices, 1);
}

void IndexedEdgeGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core IndexedEdgeGeometry.collectStatistics()
    stats.addIndexedEdges(
                    static_cast<uint64_t>(m_numIndices * 4));
    stats.addEdgeTable(
                    static_cast<uint64_t>(m_edgeLut.getBytesUsed()));
}

void IndexedEdgeGeometry::dispose()
{
    m_edgeLut.dispose();
    m_indexBuffer = rhi::IndexBufferHandle{};
    m_numIndices = 0;
}

END_DQ_RENDER_NAMESPACE
