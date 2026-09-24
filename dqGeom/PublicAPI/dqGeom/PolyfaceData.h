// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceData (mesh data storage)
//
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceData.ts
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Polyface.h (PolyfaceVectors)
//
// Holds all parallel arrays for mesh geometry: points, normals, colors,
// and corresponding index arrays.  Indices use 0-terminated variable-size
// face loops (matching imodel-native convention).
// 替代 Qt QVector，底层用 std::vector。
#pragma once

#include "Point2d.h"
#include "Point3d.h"
#include "Vector3d.h"
#include "Range3d.h"

#include <cstddef>
#include <cstdint>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// PolyfaceData — flat array storage for mesh geometry
// ---------------------------------------------------------------------------
struct DQ_GEOM_EXPORT PolyfaceData {
    // --- Data arrays ---
    std::vector<Point3d> points;
    std::vector<Vector3d> normals;
    std::vector<uint32_t> colors;       // packed RGBA (0xRRGGBBAA)
    std::vector<Point2d> params;        // UV params (Ported from: PolyfaceData.param)

    // --- Index arrays (parallel, 0-terminated face loops) ---
    std::vector<int32_t> pointIndex;    // indices into points (1-based, 0=terminator)
    std::vector<int32_t> normalIndex;   // indices into normals (parallel to pointIndex)
    std::vector<int32_t> colorIndex;    // indices into colors (parallel to pointIndex)
    std::vector<int32_t> paramIndex;    // indices into params (Ported from: PolyfaceData.paramIndex)
    std::vector<bool> edgeVisible;      // per-edge visibility (parallel to pointIndex)

    // --- Properties ---
    bool twoSided = false;
    uint32_t expectedClosure = 0;   // 0=unknown, 1=sheet, 2=solid

    // --- Count accessors ---
    size_t PointCount() const noexcept { return points.size(); }
    size_t NormalCount() const noexcept { return normals.size(); }
    size_t ColorCount() const noexcept { return colors.size(); }
    size_t ParamCount() const noexcept { return params.size(); }
    size_t IndexCount() const noexcept { return pointIndex.size(); }

    // --- Data access (1-based index, matching imodel-native convention) ---
    Point3d GetPoint(int32_t index) const
    {
        if (index <= 0 || static_cast<size_t>(index) > points.size()) return Point3d::FromZero();
        return points[static_cast<size_t>(index - 1)];
    }

    Vector3d GetNormal(int32_t index) const
    {
        if (index <= 0 || static_cast<size_t>(index) > normals.size()) return Vector3d::FromZero();
        return normals[static_cast<size_t>(index - 1)];
    }

    uint32_t GetColor(int32_t index) const
    {
        if (index <= 0 || static_cast<size_t>(index) > colors.size()) return 0xFFFFFFFF;
        return colors[static_cast<size_t>(index - 1)];
    }

    Point2d GetParam(int32_t index) const
    {
        if (index <= 0 || static_cast<size_t>(index) > params.size()) return Point2d::From(0.0, 0.0);
        return params[static_cast<size_t>(index - 1)];
    }

    bool GetEdgeVisible(size_t readIndex) const
    {
        if (readIndex >= edgeVisible.size()) return true;
        return edgeVisible[readIndex];
    }

    // --- Range computation ---
    void ExtendRange(Range3d& range) const
    {
        for (auto const& p : points) {
            range.ExtendPoint(p);
        }
    }

    Range3d ComputeRange() const
    {
        Range3d range;
        ExtendRange(range);
        return range;
    }

    // --- Compression (cluster duplicate vertices) ---
    void Compress(double tolerance = kSmallMetricDistance);
};

END_DQ_GEOM_NAMESPACE
