// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClusterableArray（internal——参考标 @internal）
//
// Ported from: itwinjs-core core/geometry/src/numerics/ClusterableArray.ts
//
// 子集裁决：仅移植 PolyfaceData.compress 的两条消费面（clusterNumberArray /
// clusterGrowablePoint3dArray 的 flat-double 形态）+ 其支撑链（setupPrimaryClusterSort /
// sortIndicesLexical[compareLexicalBlock] / clusterIndicesLexical / distanceBetweenSubBlocks）。
// GrowableBlockedArray 基类折叠进本类（块=扁平 double 数组，blockSize=1+numCoord——
// 参考块布局 [sortKey, coords...] 原样；extraData 面恒 0 不移植）。
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dqGeom {

// PackedNumbersWithIndex（1:1 ClusterableArray.ts:519-546）——打包值 + oldToNew 映射。
struct PackedNumbersWithIndex {
    std::vector<double> packedNumbers;
    std::vector<uint32_t> oldToNew;  // 旧 0-based 索引 → 簇索引

    static constexpr uint32_t kInvalidIndex = 0xFFFFffff;

    explicit PackedNumbersWithIndex(size_t numOldIndexEntry)
        : oldToNew(numOldIndexEntry, kInvalidIndex)
    {}

    // Ported from: ClusterableArray.ts:435-443 updateIndices——value 为旧 0-based 索引时
    // 原样映射。DanQing auxData.indices 是 signed 1-based（AuxData.h EQUIVALENCE）：
    // v != 0 → sign(v) × (oldToNew[abs(v)-1] + 1)；符号保持、0（不应出现）不动。
    // 发散=基址/符号承载（同信息）；验证=PolyfaceAuxDataTest.Compress 钉值锁。
    bool updateIndices(std::vector<int32_t>& indices) const
    {
        size_t numErrors = 0;
        for (auto& value : indices) {
            if (value == 0)
                continue;
            int64_t const a = value < 0 ? -static_cast<int64_t>(value) : static_cast<int64_t>(value);
            size_t const oldIndex = static_cast<size_t>(a - 1);
            if (oldIndex < oldToNew.size()) {
                uint32_t const mapped = oldToNew[oldIndex];
                value = value < 0 ? -static_cast<int32_t>(mapped + 1) : static_cast<int32_t>(mapped + 1);
            } else {
                numErrors++;
            }
        }
        return numErrors == 0;
    }
};

// ClusterableArray（1:1 ClusterableArray.ts:22-430 子集——numExtraDataPerPoint 恒 0）。
class ClusterableArray {
public:
    // (This is pretty strange)
    // The sort vector is (1,c, c*c, ...)（ClusterableArray.ts:23-27 注释原样）。
    static constexpr double kVectorFactor = 0.8732;  // use 1.0 to rig easy tests.
    static constexpr uint32_t kClusterTerminator = 0xFFffFFff;

    ClusterableArray(size_t numCoordinatePerPoint, size_t initialBlockCapacity)
        : m_numCoordinatePerPoint(numCoordinatePerPoint)
        , m_blockSize(1 + numCoordinatePerPoint)
    {
        m_data.reserve(m_blockSize * initialBlockCapacity);
    }

    // addDirect（ClusterableArray.ts:57-65 的 1/3 坐标形态——块 [key=0, coords...]）。
    void addDirect(double x0)
    {
        m_data.push_back(0.0);
        m_data.push_back(x0);
        m_numBlocks++;
    }
    void addDirect(double x0, double x1, double x2)
    {
        m_data.push_back(0.0);
        m_data.push_back(x0);
        m_data.push_back(x1);
        m_data.push_back(x2);
        m_numBlocks++;
    }

    size_t numBlocks() const { return m_numBlocks; }
    size_t numPerBlock() const { return m_blockSize; }

    // component（GrowableBlockedArray.ts:161-163——块内无检查直读）。
    double component(size_t blockIndex, size_t componentIndex) const
    {
        return m_data[m_blockSize * blockIndex + componentIndex];
    }

    // distanceBetweenSubBlocks（GrowableBlockedArray.ts:212-223）——iBegin<=i<iEnd 的
    // 欧氏距离（hypotenuse=sqrt(summed squares)）。
    double distanceBetweenSubBlocks(size_t blockIndexA, size_t blockIndexB, size_t iBegin,
                                    size_t iEnd) const
    {
        double dd = 0.0;
        size_t const iA = m_blockSize * blockIndexA;
        size_t const iB = m_blockSize * blockIndexB;
        for (size_t i = iBegin; i < iEnd; i++) {
            double const a = m_data[iA + i] - m_data[iB + i];
            dd += a * a;
        }
        return std::sqrt(dd);
    }

    // setupPrimaryClusterSort（ClusterableArray.ts:182-197）——块 [0] 写入坐标与
    // 斜向量的点积（vector[c] = kVectorFactor^c）。
    void setupPrimaryClusterSort()
    {
        size_t const nc = m_numCoordinatePerPoint;
        std::vector<double> vec(nc);
        vec[0] = 1.0;
        for (size_t c = 1; c < nc; c++)
            vec[c] = vec[c - 1] * kVectorFactor;
        for (size_t b = 0; b < m_numBlocks; b++) {
            size_t const k = m_blockSize * b;
            double dot = 0.0;
            for (size_t c = 0; c < nc; c++)
                dot += vec[c] * m_data[k + 1 + c];
            m_data[k] = dot;
        }
    }

    // compareLexicalBlock（GrowableBlockedArray.ts:170-180）——块序字典序；
    // 全等时 ia-ib（原始序保持——"so original order is maintained among duplicates!!!!"）。
    static int compareLexicalBlock(std::vector<double> const& data, size_t blockSize, size_t ia,
                                   size_t ib)
    {
        for (size_t i = 0; i < blockSize; i++) {
            double const ax = data[ia + i];
            double const bx = data[ib + i];
            if (ax > bx) return 1;
            if (ax < bx) return -1;
        }
        return ia < ib ? -1 : (ia > ib ? 1 : 0);  // ia - ib 的 sign 语义（同序保持）
    }

    // sortIndicesLexical（GrowableBlockedArray.ts:182-196）——块索引按块内容字典序排序。
    std::vector<uint32_t> sortIndicesLexical() const
    {
        size_t const n = m_numBlocks;
        std::vector<uint32_t> result(n);
        for (size_t i = 0; i < n; i++)
            result[i] = static_cast<uint32_t>(i);
        std::sort(result.begin(), result.end(), [this](uint32_t a, uint32_t b) {
            return compareLexicalBlock(m_data, m_blockSize,
                                       static_cast<size_t>(a) * m_blockSize,
                                       static_cast<size_t>(b) * m_blockSize) < 0;
        });
        return result;
    }

    // clusterIndicesLexical（ClusterableArray.ts:140-178）——主排序键装好后，
    // 词法排序 + 候选窗（barrierU=leader.key+tol）内逐块距离门吸收；
    // 簇间以 kClusterTerminator 分隔。
    std::vector<uint32_t> clusterIndicesLexical(double clusterTolerance)
    {
        setupPrimaryClusterSort();
        auto firstSort = sortIndicesLexical();
        std::vector<uint32_t> clusterIndices;
        clusterIndices.reserve(2 * firstSort.size());
        size_t const n = m_numBlocks;
        size_t const k0 = 1;                          // beginning of active column for distance
        size_t const k1 = 1 + m_numCoordinatePerPoint;
        for (size_t i = 0; i < n; i++) {
            uint32_t const clusterStartBlockIndex = firstSort[i];
            if (kClusterTerminator != clusterStartBlockIndex) {
                // unused block, so it becomes a cluster...
                clusterIndices.push_back(clusterStartBlockIndex);
                double const barrierU = component(clusterStartBlockIndex, 0) + clusterTolerance;
                firstSort[i] = kClusterTerminator;
                for (size_t j = i + 1; j < n; j++) {
                    uint32_t const candidateBlockIndex = firstSort[j];
                    if (candidateBlockIndex == kClusterTerminator)
                        continue;  // nearby in sort direction but already in a cluster.
                    if (component(candidateBlockIndex, 0) > barrierU)
                        break;
                    if (distanceBetweenSubBlocks(clusterStartBlockIndex, candidateBlockIndex, k0, k1)
                        <= clusterTolerance) {
                        clusterIndices.push_back(candidateBlockIndex);  // The candidate is in the block
                        firstSort[j] = kClusterTerminator;  // and it will not be reused as future block base
                    }
                }
                clusterIndices.push_back(kClusterTerminator);
            }
        }
        return clusterIndices;
    }

    // clusterNumberArray（ClusterableArray.ts:347-366）——1D 值聚类打包。
    static PackedNumbersWithIndex clusterNumberArray(std::vector<double> const& data,
                                                     double tolerance)
    {
        ClusterableArray clusterArray(1, data.size());
        for (double const x : data)
            clusterArray.addDirect(x);
        auto const order = clusterArray.clusterIndicesLexical(tolerance);
        PackedNumbersWithIndex result(data.size());
        uint32_t currentClusterIndex = 0;
        size_t numThisCluster = 0;
        for (uint32_t const k : order) {
            if (k == kClusterTerminator) {
                currentClusterIndex++;
                numThisCluster = 0;
            } else {
                if (numThisCluster == 0)
                    result.packedNumbers.push_back(data[k]);
                result.oldToNew[k] = currentClusterIndex;
                numThisCluster++;
            }
        }
        return result;
    }

    // clusterGrowablePoint3dArray 的 flat-double 形态（ClusterableArray.ts:404-429）——
    // 源=阻塞 xyz double 数组（DanQing 无 GrowableXYZArray；打包产物同形态 flat xyz）。
    static PackedNumbersWithIndex clusterBlockedPoint3dArray(std::vector<double> const& data,
                                                             double tolerance)
    {
        size_t const numPoint = data.size() / 3;
        ClusterableArray clusterArray(3, numPoint);
        for (size_t i = 0; i < numPoint; i++)
            clusterArray.addDirect(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
        auto const order = clusterArray.clusterIndicesLexical(tolerance);
        PackedNumbersWithIndex result(numPoint);
        uint32_t currentClusterIndex = 0;
        size_t numThisCluster = 0;
        for (uint32_t const k : order) {
            if (k == kClusterTerminator) {
                currentClusterIndex++;
                numThisCluster = 0;
            } else {
                if (numThisCluster == 0) {
                    result.packedNumbers.push_back(data[k * 3]);
                    result.packedNumbers.push_back(data[k * 3 + 1]);
                    result.packedNumbers.push_back(data[k * 3 + 2]);
                }
                result.oldToNew[k] = currentClusterIndex;
                numThisCluster++;
            }
        }
        return result;
    }

private:
    size_t m_numCoordinatePerPoint;
    size_t m_blockSize;
    size_t m_numBlocks = 0;
    std::vector<double> m_data;
};

}  // namespace dqGeom
