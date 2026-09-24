// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceQuery implementation
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceQuery.ts
#include "dqGeom/PolyfaceQuery.h"

#include "BuildAverageNormalsContext.h"

#include <cmath>
#include <unordered_map>

BEGIN_DQ_GEOM_NAMESPACE

Range3d PolyfaceQuery::PointRange(IndexedPolyface const& polyface)
{
    return polyface.Data().ComputeRange();
}

// 1:1 PolyfaceQuery.buildAverageNormals → BuildAverageNormalsContext.buildFastAverageNormals.
void PolyfaceQuery::BuildAverageNormals(IndexedPolyface& polyface, double toleranceAngleRadians)
{
    BuildAverageNormalsContext::BuildFastAverageNormals(polyface, toleranceAngleRadians);
}

size_t PolyfaceQuery::GetNumFacet(IndexedPolyface const& polyface)
{
    return polyface.FacetCount();
}

size_t PolyfaceQuery::GetNumVertex(IndexedPolyface const& polyface)
{
    return polyface.Data().PointCount();
}

bool PolyfaceQuery::HasNormals(IndexedPolyface const& polyface)
{
    return polyface.Data().NormalCount() > 0;
}

bool PolyfaceQuery::HasColors(IndexedPolyface const& polyface)
{
    return polyface.Data().ColorCount() > 0;
}

bool PolyfaceQuery::HasFacets(IndexedPolyface const& polyface)
{
    return polyface.FacetCount() > 0;
}

double PolyfaceQuery::TriangleArea(Point3d const& a, Point3d const& b, Point3d const& c)
{
    Vector3d ab = Vector3d::FromStartEnd(a, b);
    Vector3d ac = Vector3d::FromStartEnd(a, c);
    Vector3d cross = Vector3d::FromCrossProduct(ab, ac);
    return 0.5 * cross.Magnitude();
}

// Ported from: itwinjs PolyfaceQuery.sumFacetAreas
// Fan-triangulate each facet and sum triangle areas.
double PolyfaceQuery::SumFacetAreas(IndexedPolyface const& polyface)
{
    auto const& data = polyface.Data();
    double totalArea = 0.0;

    for (size_t facet = 0; facet < polyface.FacetCount(); ++facet) {
        size_t i0 = polyface.FacetIndex0(facet);
        size_t i1 = polyface.FacetIndex1(facet);
        size_t numEdge = i1 - i0;
        if (numEdge < 3) continue;

        // Get the first point (fan apex)
        int32_t apexIdx = data.pointIndex[static_cast<int>(i0)];
        if (apexIdx == 0) continue;
        Point3d apex = data.GetPoint(apexIdx);

        // Fan triangulation: (apex, edge[i], edge[i+1])
        for (size_t i = 1; i + 1 < numEdge; ++i) {
            int32_t idx1 = data.pointIndex[static_cast<int>(i0 + i)];
            int32_t idx2 = data.pointIndex[static_cast<int>(i0 + i + 1)];
            if (idx1 == 0 || idx2 == 0) continue;

            Point3d p1 = data.GetPoint(idx1);
            Point3d p2 = data.GetPoint(idx2);
            totalArea += TriangleArea(apex, p1, p2);
        }
    }
    return totalArea;
}

// Ported from: itwinjs PolyfaceQuery.computeFacetUnitNormal
bool PolyfaceQuery::ComputeFacetUnitNormal(IndexedPolyface const& polyface,
                                            size_t facetIndex, Vector3d& normal)
{
    auto const& data = polyface.Data();
    size_t i0 = polyface.FacetIndex0(facetIndex);
    size_t i1 = polyface.FacetIndex1(facetIndex);
    size_t numEdge = i1 - i0;
    if (numEdge < 3) return false;

    int32_t idx0 = data.pointIndex[static_cast<int>(i0)];
    int32_t idx1 = data.pointIndex[static_cast<int>(i0 + 1)];
    int32_t idx2 = data.pointIndex[static_cast<int>(i0 + 2)];
    if (idx0 == 0 || idx1 == 0 || idx2 == 0) return false;

    Point3d p0 = data.GetPoint(idx0);
    Point3d p1 = data.GetPoint(idx1);
    Point3d p2 = data.GetPoint(idx2);

    Vector3d v01 = Vector3d::FromStartEnd(p0, p1);
    Vector3d v02 = Vector3d::FromStartEnd(p0, p2);
    normal = Vector3d::FromCrossProduct(v01, v02);
    double mag = normal.Magnitude();
    if (mag < kSmallMetricDistance) return false;
    normal.Scale(1.0 / mag);
    return true;
}

// Ported from: itwinjs PolyfaceQuery.isPolyfaceClosedByEdgePairing
// A mesh is closed if every edge (pair of consecutive point indices in a face)
// appears exactly twice (once in each direction).
bool PolyfaceQuery::IsClosedByEdgePairing(IndexedPolyface const& polyface)
{
    auto const& data = polyface.Data();

    // Count edge occurrences using a hash map
    // Edge key = (minIdx, maxIdx), value = count
    struct EdgeKey {
        int32_t a, b;
        bool operator==(EdgeKey const& o) const noexcept { return a == o.a && b == o.b; }
    };
    struct EdgeHash {
        size_t operator()(EdgeKey const& k) const noexcept
        {
            return std::hash<int32_t>{}(k.a) ^ (std::hash<int32_t>{}(k.b) << 16);
        }
    };

    std::unordered_map<EdgeKey, int, EdgeHash> edgeCount;

    for (size_t facet = 0; facet < polyface.FacetCount(); ++facet) {
        size_t i0 = polyface.FacetIndex0(facet);
        size_t i1 = polyface.FacetIndex1(facet);
        size_t numEdge = i1 - i0;
        if (numEdge < 3) continue;

        for (size_t i = 0; i < numEdge; ++i) {
            size_t next = (i + 1) % numEdge;
            int32_t a = data.pointIndex[static_cast<int>(i0 + i)];
            int32_t b = data.pointIndex[static_cast<int>(i0 + next)];
            if (a == 0 || b == 0) continue;

            // Normalize: use (min, max) as key
            EdgeKey key{std::min(a, b), std::max(a, b)};
            edgeCount[key]++;
        }
    }

    // Every edge must appear exactly twice
    for (auto const& [key, count] : edgeCount) {
        if (count != 2) return false;
    }
    return !edgeCount.empty();
}

END_DQ_GEOM_NAMESPACE
