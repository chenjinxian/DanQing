// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceData implementation
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceData.ts
#include "dqGeom/PolyfaceData.h"

#include "../numerics/ClusterableArray.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: itwinjs PolyfaceData.compress
// Cluster duplicate vertices and remap indices.
void PolyfaceData::Compress(double tolerance)
{
    // Ported from: PolyfaceData.ts:617-629 auxData 压缩腿——恰 1 通道且该通道恰 1 帧时
    // 对通道值聚类打包并重映射 auxData.indices；聚类容差=参考默认 smallMetricDistance
    // （:620/:625 不传 tolerance——注意不用本函数的调用方容差）。
    // 位置注记：本腿置于点聚类早退门之前——DanQing 点压缩在无重复点时整函数早退
    // （存量局部优化），而参考 compress 无早退、auxData 腿恒达；为保 auxData 语义不随
    // 点重复性摆动，本腿先行（点/法线/参数/颜色聚类与 auxData 相互独立——数组域正交）。
    if (auxData && auxData->channels.size() == 1 && auxData->channels[0].data.size() == 1) {
        auto& channel = auxData->channels[0];
        size_t const dataSize = AuxChannel::entriesPerValue(channel.dataType);
        if (1 == dataSize) {
            auto packed = ClusterableArray::clusterNumberArray(channel.data[0].values,
                                                               kSmallMetricDistance);
            channel.data[0].values = std::move(packed.packedNumbers);
            packed.updateIndices(auxData->indices);
        } else if (3 == dataSize) {
            auto packed = ClusterableArray::clusterBlockedPoint3dArray(channel.data[0].values,
                                                                       kSmallMetricDistance);
            channel.data[0].values = std::move(packed.packedNumbers);
            packed.updateIndices(auxData->indices);
        }
    }

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
