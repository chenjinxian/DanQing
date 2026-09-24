// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/VertexKey.ts
// DanQing dqRender — VertexKey / VertexMap implementation (compare/equals/comparePositions)
#include "VertexKey.h"

#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Authored: 1:1 core-bentley compareWithTolerance (dqBase does not expose it).
// Returns -1 if a < b-tol, 1 if a > b+tol, else 0 (i.e. |a-b| <= tol counts as equal).
int compareWithTolerance(double a, double b, double tol)
{
    if (a < b - tol) return -1;
    if (a > b + tol) return 1;
    return 0;
}

// 1:1 comparePositions (VertexKey.ts:22-31) — component-wise tolerant comparison.
int comparePositionsImpl(const dqGeom::Point3d& p0, const dqGeom::Point3d& p1, const dqGeom::Point3d& tol)
{
    int diff = compareWithTolerance(p0.x, p1.x, tol.x);
    if (0 == diff) {
        diff = compareWithTolerance(p0.y, p1.y, tol.y);
        if (0 == diff)
            diff = compareWithTolerance(p0.z, p1.z, tol.z);
    }
    return diff;
}

// Authored: 1:1 core-bentley comparePossiblyUndefined (lhs.compare(rhs) comparator, undefined-aware).
int compareFeatures(const std::optional<dqCommon::Feature>& f0, const std::optional<dqCommon::Feature>& f1)
{
    if (!f0 && !f1) return 0;
    if (!f0) return -1;
    if (!f1) return 1;
    return f0->compare(*f1);
}

}  // namespace

// 1:1 VertexKey.equals (VertexKey.ts:57-79).
bool VertexKey::equals(const VertexKey& rhs, const dqGeom::Point3d& tolerance) const
{
    if (m_fillColor != rhs.m_fillColor)
        return false;

    if (0 != compareFeatures(m_feature, rhs.m_feature))
        return false;

    if (m_normal) {
        if (!rhs.m_normal || m_normal->value != rhs.m_normal->value)
            return false;
    }

    if (0 != comparePositionsImpl(m_position, rhs.m_position, tolerance))
        return false;

    if (m_uvParam) {
        // 1:1 reference: this.uvParam.isAlmostEqual(rhs.uvParam, 0.0001). Point2d has no tolerance-taking
        // variant; compare component-wise within 0.0001. UV-param path is Phase-N (inert in D′).
        if (!rhs.m_uvParam)
            return false;
        constexpr double kUvTol = 0.0001;
        return std::abs(m_uvParam->x - rhs.m_uvParam->x) <= kUvTol &&
               std::abs(m_uvParam->y - rhs.m_uvParam->y) <= kUvTol;
    }

    return true;
}

// 1:1 VertexKey.compare (VertexKey.ts:81-107).
int VertexKey::compare(const VertexKey& rhs, const dqGeom::Point3d& tolerance) const
{
    if (this == &rhs)
        return 0;

    int diff = static_cast<int>(m_fillColor) - static_cast<int>(rhs.m_fillColor);
    if (0 == diff) {
        diff = comparePositionsImpl(m_position, rhs.m_position, tolerance);
        if (0 == diff) {
            diff = compareFeatures(m_feature, rhs.m_feature);
            if (0 == diff) {
                if (m_normal) {
                    // 1:1 reference asserts rhs.normal present; compare oct-encoded values.
                    diff = rhs.m_normal ? static_cast<int>(m_normal->value) - static_cast<int>(rhs.m_normal->value) : -1;
                }

                if (0 == diff && m_uvParam) {
                    // 1:1 reference (VertexKey.ts:98-100): compares x to x, then x to y (upstream typo
                    // ported verbatim). UV-param path is Phase-N for the mesh pipeline; branch is inert here.
                    diff = compareWithTolerance(m_uvParam->x, rhs.m_uvParam ? rhs.m_uvParam->x : 0.0, 0.0);
                    if (0 == diff && rhs.m_uvParam)
                        diff = compareWithTolerance(m_uvParam->x, rhs.m_uvParam->y, 0.0);
                }
            }
        }
    }

    return diff;
}

// 1:1 VertexMap.comparePositions (VertexKey.ts:127-129).
int VertexMap::comparePositions(const VertexKeyProps& p0, const VertexKeyProps& p1) const
{
    return comparePositionsImpl(p0.position, p1.position, m_tolerance);
}

END_DQ_RENDER_NAMESPACE
