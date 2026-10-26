// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Frustum planes for culling
// Ported from: itwinjs-core core/common/src/geometry/FrustumPlanes.ts
//
// Extracts 6 frustum planes from a model-view-projection matrix and
// tests axis-aligned bounding boxes for containment.
#pragma once

#include <dqGeom/Range3d.h>
#include <dqCommon/Frustum.h>  // world-frustum ctor（fromFrustum）

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

    /// Construct from a WORLD-space frustum's 8 corner points.
    /// Ported from: itwinjs-core FrustumPlanes.fromFrustum
    ///              (FrustumPlanes.ts:100-103 + computeFrustumPlanes :46-92——
    ///              5 面指数表 planePointIndices[1,5,3]/[0,2,4]/[2,3,6]/[0,4,1]/[0,1,2]
    ///              + front 由 back 反推、锚 points[4]；外扩 1e-6)。
    explicit FrustumPlanes(dqCommon::Frustum const& world);

    /// Test if an AABB is at least partially inside the frustum.
    /// Returns true if the box intersects or is inside the frustum.
    /// Ported from: itwinjs-core FrustumPlanes.computeBoxContainment()
    bool computeBoxContainment(dqGeom::Range3d const& box) const;

    /// World-frustum planes version: containment of an AABB (world space)
    /// against the 6 world planes. Ported from: itwinjs-core
    /// FrustumPlanes.computeContainment (:165-200——逐面 8 角 altitude+1e-8<0
    /// 全出者剔；tolerance 1e-8 同参考默认)。
    bool computeRangeContainmentWorld(dqGeom::Range3d const& box) const;

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

inline FrustumPlanes::FrustumPlanes(dqCommon::Frustum const& world)
{
    // Ported from: itwinjs-core FrustumPlanes.computeFrustumPlanes
    //              (FrustumPlanes.ts:46-92)。角点序（FrustumPlanes.ts:17-33）：
    //   0=left bottom rear, 1=right bottom rear, 2=left top rear, 3=right top rear,
    //   4=left bottom front, 5=right bottom front, 6=left top front, 7=right top front
    // = dqCommon::Npc 枚举序（LeftBottomRear..RightTopFront）。
    auto const& pts = world.points;
    static constexpr int kPlanePointIndices[5][3] = {
        {1, 5, 3},  // right
        {0, 2, 4},  // left
        {2, 3, 6},  // top
        {0, 4, 1},  // bottom
        {0, 1, 2},  // back（front 由 back 反推——front 面过小时精度致零面，
                    // FrustumPlanes.ts:41-43 + :73-85）
    };
    double backNormal[3] = {0.0, 0.0, 0.0};
    // 内点参考（8 角均值——面法线定向校正基准；见下方 ADAPTATION 注）。
    double interior[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < 8; ++i) {
        interior[0] += pts[i].x;
        interior[1] += pts[i].y;
        interior[2] += pts[i].z;
    }
    interior[0] /= 8.0;
    interior[1] /= 8.0;
    interior[2] /= 8.0;
    for (int pi = 0; pi < 5; ++pi) {
        auto const& idx = kPlanePointIndices[pi];
        auto const& p0 = pts[idx[0]];
        auto const& p1 = pts[idx[1]];
        auto const& p2 = pts[idx[2]];
        // normal = createCrossProductToPoints(p0, p1, p2) = (p1-p0) × (p2-p0)，归一。
        double e1[3] = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        double e2[3] = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        double n[3] = {e1[1] * e2[2] - e1[2] * e2[1],
                       e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0]};
        double const len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len < 1.0e-12)
            return;  // degenerate → 零面（m_valid 保持 false，参考返回 []）
        for (int k = 0; k < 3; ++k) n[k] /= len;
        // ADAPTATION（§11.10 登记）：参考表隐含其 Npc 角点绕序；DQ planFrustum
        // 的实际绕序与之相反（2026-10-26 底面法线朝内实录）——面质心对 interiors
        // 定向校正：法线必须背向内部（altitude(centroid) > 0）。
        double centroid[3] = {(p0.x + p1.x + p2.x) / 3.0,
                              (p0.y + p1.y + p2.y) / 3.0,
                              (p0.z + p1.z + p2.z) / 3.0};
        double const outward = n[0] * (centroid[0] - interior[0])
                             + n[1] * (centroid[1] - interior[1])
                             + n[2] * (centroid[2] - interior[2]);
        int const flip = outward < 0.0 ? -1 : 1;
        for (int k = 0; k < 3; ++k) m_planes[pi][k] = static_cast<float>(flip * n[k]);
        // 平面方程 a·x+b·y+c·z+d=0（altitude = n·X + d）；参考 distance = n·p0
        // 且 altitude = n·X - dist ⇒ d = -(n·p0) + expand。expandPlaneDistance
        // 外扩 = 内侧更宽松 ⇒ d = -(n·p0) + 1e-6（首版符号反——自家角全判外）。
        double const d = -(flip * (n[0] * p0.x + n[1] * p0.y + n[2] * p0.z)) + 1.0e-6;
        m_planes[pi][3] = static_cast<float>(d);
        if (pi == 4) {  // back 面法线留档——front 反推用（已定向）
            backNormal[0] = flip * n[0];
            backNormal[1] = flip * n[1];
            backNormal[2] = flip * n[2];
        }
    }
    // front = back 反推（法线取反；距离锚 points[4]=left bottom front——
    // FrustumPlanes.ts:75-85 "based on a point on the front rect"）。
    m_planes[5][0] = static_cast<float>(-backNormal[0]);
    m_planes[5][1] = static_cast<float>(-backNormal[1]);
    m_planes[5][2] = static_cast<float>(-backNormal[2]);
    m_planes[5][3] = static_cast<float>(
        -(-(backNormal[0] * pts[4].x + backNormal[1] * pts[4].y + backNormal[2] * pts[4].z))
        + 1.0e-6);  // d = -(n_front·anchor) + expand（符号约定同上）
    m_valid = true;
}

inline bool FrustumPlanes::computeRangeContainmentWorld(dqGeom::Range3d const& box) const
{
    if (!m_valid)
        return true;  // No culling — always visible
    if (box.isNull())
        return false;

    // 8 角点逐面 altitude 测试（参考 computeContainment :180-197——
    // altitude(point)+tolerance<0 计外；全部 8 点在某面外 → Outside）。
    double const corners[8][3] = {
        {box.low.x, box.low.y, box.low.z},   {box.high.x, box.low.y, box.low.z},
        {box.low.x, box.high.y, box.low.z},  {box.high.x, box.high.y, box.low.z},
        {box.low.x, box.low.y, box.high.z},  {box.high.x, box.low.y, box.high.z},
        {box.low.x, box.high.y, box.high.z}, {box.high.x, box.high.y, box.high.z},
    };
    double const tolerance = 1.0e-8;  // 参考默认 tolerance
    for (int i = 0; i < 6; ++i) {
        auto const& p = m_planes[i];
        int nOutside = 0;
        for (int c = 0; c < 8; ++c) {
            double const altitude = static_cast<double>(p[0]) * corners[c][0]
                                  + static_cast<double>(p[1]) * corners[c][1]
                                  + static_cast<double>(p[2]) * corners[c][2]
                                  + static_cast<double>(p[3]);
            if (altitude + tolerance < 0.0)
                ++nOutside;
        }
        if (nOutside == 8)
            return false;  // Outside
    }
    return true;  // Intersected 或 Contained
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
