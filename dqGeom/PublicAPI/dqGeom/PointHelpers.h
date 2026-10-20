// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Point3dArray packed-array helpers
// Ported from: itwinjs-core core/geometry/src/geometry3d/PointHelpers.ts
//
// 子集（M-W W-b2，AspectRatioSkew/BSpline 消费面）：Point3dArray 的
// multiplyInPlace（packed Float64 形态）/ isAlmostEqual（packed 形态）/
// sumEdgeLengths（packed 形态）/ unpackNumbersToNestedArrays。全文件余量
// TODO——随消费面扩展。
#pragma once

#include "DqGeom.h"
#include "Geometry.h"
#include "Point3d.h"
#include "Transform.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Point3dArray — 打包点数组助手（packed xyz doubles）
// Ported from: PointHelpers.ts Point3dArray
// ---------------------------------------------------------------------------
namespace Point3dArray {

// Ported from: Point3dArray.multiplyInPlace（packed Float64Array 形态，:809-819）。
inline void multiplyInPlace(Transform const& transform, std::vector<double>& xyz)
{
    size_t const numXYZ = xyz.size();
    for (size_t i = 0; i + 2 < numXYZ; i += 3) {
        Point3d const p = transform.MultiplyXYZ(xyz[i], xyz[i + 1], xyz[i + 2]);
        xyz[i] = p.x;
        xyz[i + 1] = p.y;
        xyz[i + 2] = p.z;
    }
}

// Ported from: Point3dArray.isAlmostEqual（packed Float64Array 形态，:821-830——
// 逐坐标 isSameCoordinate，容差默认 smallMetricDistance）。
inline bool isAlmostEqual(std::vector<double> const& dataA, std::vector<double> const& dataB,
                          double tolerance = 1.0e-6)
{
    if (dataA.size() != dataB.size())
        return false;
    for (size_t i = 0; i < dataA.size(); ++i) {
        if (!isSameCoordinate(dataA[i], dataB[i], tolerance))
            return false;
    }
    return true;
}

// Ported from: Point3dArray.sumEdgeLengths（packed Float64Array 形态，:947-968——
// addClosureEdge/maxPointsToUse 两参未在消费面，TODO）。
inline double sumEdgeLengths(std::vector<double> const& data)
{
    double sum = 0.0;
    size_t i = 0;
    for (; i + 5 < data.size(); i += 3) {  // final i points at final point x
        sum += hypotenuseXYZ(data[i + 3] - data[i],
                             data[i + 4] - data[i + 1],
                             data[i + 5] - data[i + 2]);
    }
    return sum;
}

// Ported from: Point3dArray.unpackNumbersToNestedArrays (:761-777)。
inline std::vector<std::vector<double>> unpackNumbersToNestedArrays(
    std::vector<double> const& data, size_t numPerBlock)
{
    std::vector<std::vector<double>> result;
    size_t const n = data.size();
    size_t i = 0;
    while (i < n) {
        size_t i1 = i + numPerBlock;
        if (i1 > n)
            i1 = n;
        std::vector<double> row;
        for (; i < i1; ++i)
            row.push_back(data[i]);
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace Point3dArray

END_DQ_GEOM_NAMESPACE
