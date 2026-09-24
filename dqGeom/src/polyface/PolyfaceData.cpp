// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceData implementation
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceData.ts
#include "dqGeom/PolyfaceData.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs PolyfaceData.compress
// Cluster duplicate vertices and remap indices.
void PolyfaceData::Compress(double tolerance)
{
    if (points.empty()) return;

    // Build a map from old point index to new (compressed) point index
    std::vector<int32_t> indexMap(points.size());
    std::vector<Point3d> compressedPoints;
    double tolSq = tolerance * tolerance;

    for (size_t i = 0; i < points.size(); ++i) {
        // Check if this point is a duplicate of an existing compressed point
        int32_t newIndex = -1;
        for (size_t j = 0; j < compressedPoints.size(); ++j) {
            if (Vector3d::FromStartEnd(points[i], compressedPoints[j]).MagnitudeSquared() < tolSq) {
                newIndex = static_cast<int32_t>(j) + 1;  // 1-based
                break;
            }
        }
        if (newIndex < 0) {
            compressedPoints.push_back(points[i]);
            newIndex = static_cast<int32_t>(compressedPoints.size());  // 1-based
        }
        indexMap[i] = newIndex;
    }

    // Only compress if we actually removed duplicates
    if (compressedPoints.size() == PointCount()) return;

    // Remap point indices
    for (size_t i = 0; i < pointIndex.size(); ++i) {
        int32_t idx = pointIndex[i];
        if (idx > 0 && static_cast<size_t>(idx) <= indexMap.size()) {
            pointIndex[i] = indexMap[static_cast<size_t>(idx - 1)];
        } else if (idx < 0 && static_cast<size_t>(-idx) <= indexMap.size()) {
            pointIndex[i] = -indexMap[static_cast<size_t>(-idx - 1)];
        }
    }

    points = compressedPoints;
}

END_DQ_GEOM_NAMESPACE
