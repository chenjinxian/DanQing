// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Frustum planes for culling
// Ported from: itwinjs-core core/common/src/geometry/FrustumPlanes.ts
//
// Extracts 6 frustum planes from a model-view-projection matrix and
// tests axis-aligned bounding boxes for containment.
#pragma once

#include <dqGeom/Range3d.h>

#include <array>
#include <cmath>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FrustumPlanes — frustum plane extraction and AABB containment test
// Ported from: itwinjs-core FrustumPlanes.ts
// ---------------------------------------------------------------------------
class FrustumPlanes {
public:
    FrustumPlanes() = default;

    /// Construct from a column-major 4×4 MVP matrix.
    /// Ported from: itwinjs-core FrustumPlanes constructor
    explicit FrustumPlanes(float const* mvp16);

    /// Test if an AABB is at least partially inside the frustum.
    /// Returns true if the box intersects or is inside the frustum.
    /// Ported from: itwinjs-core FrustumPlanes.computeBoxContainment()
    bool computeBoxContainment(dqGeom::Range3d const& box) const;

    /// Check if the planes are valid (non-zero).
    bool isValid() const noexcept { return m_valid; }

    /// invalidate the planes (used when no culling is desired).
    void invalidate() { m_valid = false; }

private:
    // 6 frustum planes: right, left, top, bottom, near, far
    // Each plane: ax + by + cz + d = 0, stored as [a, b, c, d]
    std::array<std::array<float, 4>, 6> m_planes = {};
    bool m_valid = false;
};

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

inline FrustumPlanes::FrustumPlanes(float const* m)
{
    // Extract frustum planes from the combined MVP matrix using the
    // Gribb/Hartmann method.
    // Ported from: itwinjs-core FrustumPlanes.computeFrustumPlanes()

    // Left plane: row3 + row0
    m_planes[0][0] = m[3] + m[0];
    m_planes[0][1] = m[7] + m[4];
    m_planes[0][2] = m[11] + m[8];
    m_planes[0][3] = m[15] + m[12];

    // Right plane: row3 - row0
    m_planes[1][0] = m[3] - m[0];
    m_planes[1][1] = m[7] - m[4];
    m_planes[1][2] = m[11] - m[8];
    m_planes[1][3] = m[15] - m[12];

    // Bottom plane: row3 + row1
    m_planes[2][0] = m[3] + m[1];
    m_planes[2][1] = m[7] + m[5];
    m_planes[2][2] = m[11] + m[9];
    m_planes[2][3] = m[15] + m[13];

    // Top plane: row3 - row1
    m_planes[3][0] = m[3] - m[1];
    m_planes[3][1] = m[7] - m[5];
    m_planes[3][2] = m[11] - m[9];
    m_planes[3][3] = m[15] - m[13];

    // Near plane: row3 + row2
    m_planes[4][0] = m[3] + m[2];
    m_planes[4][1] = m[7] + m[6];
    m_planes[4][2] = m[11] + m[10];
    m_planes[4][3] = m[15] + m[14];

    // Far plane: row3 - row2
    m_planes[5][0] = m[3] - m[2];
    m_planes[5][1] = m[7] - m[6];
    m_planes[5][2] = m[11] - m[10];
    m_planes[5][3] = m[15] - m[14];

    // Normalize each plane
    for (auto& plane : m_planes) {
        float len = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
        if (len > 1e-10f) {
            float invLen = 1.0f / len;
            plane[0] *= invLen;
            plane[1] *= invLen;
            plane[2] *= invLen;
            plane[3] *= invLen;
        }
    }

    m_valid = true;
}

inline bool FrustumPlanes::computeBoxContainment(dqGeom::Range3d const& box) const
{
    if (!m_valid)
        return true;  // No culling — always visible

    if (box.isNull())
        return false;

    // Get the AABB bounds
    float minX = static_cast<float>(box.low.x);
    float minY = static_cast<float>(box.low.y);
    float minZ = static_cast<float>(box.low.z);
    float maxX = static_cast<float>(box.high.x);
    float maxY = static_cast<float>(box.high.y);
    float maxZ = static_cast<float>(box.high.z);

    // Test each plane against the AABB.
    // For each plane, find the "positive vertex" (the corner most in the
    // direction of the plane normal). If the positive vertex is behind
    // the plane, the entire box is outside the frustum.
    for (int i = 0; i < 6; ++i) {
        auto const& p = m_planes[i];

        // Positive vertex: for each axis, choose min or max based on normal sign
        float px = (p[0] >= 0.0f) ? maxX : minX;
        float py = (p[1] >= 0.0f) ? maxY : minY;
        float pz = (p[2] >= 0.0f) ? maxZ : minZ;

        float dist = p[0] * px + p[1] * py + p[2] * pz + p[3];
        if (dist < 0.0f)
            return false;  // Entirely outside this plane
    }

    return true;  // At least partially inside
}

END_DQ_RENDER_NAMESPACE
