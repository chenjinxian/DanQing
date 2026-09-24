// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RealityMeshGeometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RealityMesh.ts
#include "RealityMeshGeometry.h"
#include "dqRender/RenderMemory.h"

#include <algorithm>

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// RealityMeshGeometry
// (Ported from: itwinjs-core RealityMesh.ts, line 93-321)
// ===========================================================================

RealityMeshGeometry::RealityMeshGeometry(uint32_t numIndices, uint32_t vertexCount)
    : m_numIndices(numIndices)
    , m_vertexCount(vertexCount)
{
}

RealityMeshGeometry::~RealityMeshGeometry() = default;

// ---------------------------------------------------------------------------
// getPass — pass depends on transparency
// Ported from: itwinjs-core RealityMesh.ts line 308-313
// ---------------------------------------------------------------------------
Pass RealityMeshGeometry::getPass() const noexcept
{
    // Ported from: itwinjs-core RealityMesh.ts line 308-313
    // If base is transparent or thematic iso-lines are active, use translucent pass.
    if (m_baseIsTransparent)
        return Pass::Translucent;

    return Pass::Opaque;
}

// ---------------------------------------------------------------------------
// draw
// Ported from: itwinjs-core RealityMesh.ts line 316-320
// ---------------------------------------------------------------------------
void RealityMeshGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.draw2(0, m_numIndices, 0);
    }
}

// ---------------------------------------------------------------------------
// setQuantization
// Sets the quantization parameters for range computation.
// ---------------------------------------------------------------------------
void RealityMeshGeometry::setQuantization(
    float originX, float originY, float originZ,
    float scaleX, float scaleY, float scaleZ)
{
    m_qOrigin[0] = originX;
    m_qOrigin[1] = originY;
    m_qOrigin[2] = originZ;
    m_qScale[0] = scaleX;
    m_qScale[1] = scaleY;
    m_qScale[2] = scaleZ;
}

// ---------------------------------------------------------------------------
// getRange — compute bounding range from quantization parameters
// Ported from: itwinjs-core RealityMesh.ts line 214-216
// Uses Quantization.rangeScale16 = 0xFFFF
// ---------------------------------------------------------------------------
RealityMeshGeometry::Range3d RealityMeshGeometry::getRange() const
{
    constexpr float kRangeScale16 = 65535.0f;  // 0xFFFF

    Range3d range;
    range.minX = m_qOrigin[0];
    range.minY = m_qOrigin[1];
    range.minZ = m_qOrigin[2];
    range.maxX = m_qOrigin[0] + kRangeScale16 * m_qScale[0];
    range.maxY = m_qOrigin[1] + kRangeScale16 * m_qScale[1];
    range.maxZ = m_qOrigin[2] + kRangeScale16 * m_qScale[2];

    return range;
}

// ---------------------------------------------------------------------------
// collectStatistics
// Ported from: itwinjs-core RealityMesh.ts line 296-304
// ---------------------------------------------------------------------------
void RealityMeshGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core RealityMesh.ts line 296-304
    // Differentiate between terrain and reality mesh for statistics.
    uint64_t bytesUsed = static_cast<uint64_t>(m_vertexCount) * 32 +
                         static_cast<uint64_t>(m_numIndices) * 4;

    if (m_isTerrain) {
        stats.addRealityMesh( bytesUsed);
    } else {
        stats.addRealityMesh( bytesUsed);
    }

    // Texture memory
    if (m_drapeTexture) {
        stats.addTexture(0);
    }
}

END_DQ_RENDER_NAMESPACE
