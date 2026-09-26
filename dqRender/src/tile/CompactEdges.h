// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — compact imdl edges → indexed edge params expansion
// Ported from: itwinjs-core core/frontend/src/common/imdl/CompactEdges.ts（全文）
//              + core/frontend/src/common/internal/render/EdgeParams.ts:78-88
//              （EdgeTableInfo）+ :114-149（calculateEdgeTableParams——展开算法
//              的表参数半部，CompactEdges.ts:96 消费）。
//
// imdl `compact` 边缘形态：2-bit/边的可见性流（顺序与 surface.indices 的三角形
// 索引一致）+ silhouette 的 OctEncodedNormalPair u32 表。展开产物 = indexed 形态
// （每边 6 个相同 24-bit 索引构成的 quad + 边查找表）。
#pragma once

#include "VertexIndices.h"

#include "dqRender/tile/ImdlDocument.h"  // ImdlByteView / ImdlIndexedEdgeParams / ImdlEdgeTable

#include <cstdint>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ImdlEdgeVisibility（CompactEdges.ts:10 经 ImdlSchema.ts:237-248——2-bit 编码值
// Hidden=0/Silhouette=1/Visible=2/VisibleDuplicate=3）。DanQing 侧展开算法内联
// 判定（CompactEdges.ts:58 的两处比较），不引入独立枚举。
namespace compactedges {
constexpr uint8_t kHidden = 0;
constexpr uint8_t kSilhouette = 1;
constexpr uint8_t kVisible = 2;
constexpr uint8_t kVisibleDuplicate = 3;
}

// Parameters supplied to indexedEdgeParamsFromCompactEdges.
// Ported from: CompactEdges.ts:18-31 CompactEdgeParams（字段逐一 1:1）。
struct CompactEdgeParams {
    /// The number of always-visible edges（silhouette 数 = normalPairs 的对数，
    /// :19-20 注）。
    uint32_t numVisibleEdges = 0;
    /// 2 bit visibility of each edge, in the same order as the triangle indices
    /// in vertexIndices（:23-24）。C++ 形态：字节视图（TS Uint8Array）。
    ImdlByteView visibility;
    /// The indices describing the topology of the triangle mesh（:25-26）。
    VertexIndices vertexIndices;
    /// If any silhouettes are present, the [OctEncodedNormalPair]s associated
    /// with each（:27-28——u32 = normal1 | (normal2 << 16)，ImdlSchema.ts:263-264）。
    /// TS `Uint32Array | undefined` → std::optional（§3.4）。
    std::optional<ImdlByteView> normalPairs;
    /// The maximum width or height for the resultant EdgeTable（:29-30）。
    /// 来源 = ImdlReader.ts:98 `IModelApp.renderSystem.maxTextureSize`（生产侧
    /// maxVertexTableSize 选项）；DanQing 调用点暂以 2048 顶表上限同款占位
    /// （GL 上限接线 TODO，ImdlGraphics.cpp 同登记）。
    uint32_t maxEdgeTableDimension = 0;
};

/// One iterated compact edge. Ported from: CompactEdges.ts:34-38 CompactEdge
/// （normals `number | undefined` → std::optional，§3.4）。
struct CompactEdge {
    uint32_t index0 = 0;
    uint32_t index1 = 0;
    std::optional<uint32_t> normals;
};

/// Iterate over the compact edges.
/// Ported from: CompactEdges.ts:44-73 compactEdgeIterator。
///
/// TS generator → C++ 回调适配（§3.4）：参考返回 IterableIterator 并逐次原地
/// 突变同一 output 对象（:40-41 note）；C++ 以 `yield(同一 CompactEdge&)` 回调
/// 等价表达（对象仍每次原地复用）。decodeIndex 闭合参考的
/// `(index) => compact.vertexIndices.decodeIndex(vertIdx)` 形参（:101）。
/// 生产侧防御（参考 :65 assert(undefined !== normalPairs) 在 release 未定义）：
///   - Silhouette 位出现而 normalPairs 缺失 → 跳过该边；
///   - normalPairs 耗尽 → 终止迭代（不越界读）。
template <typename DecodeIndex, typename Yield>
void compactEdgeIterator(ImdlByteView const& visibilityFlags,
                         std::optional<ImdlByteView> const& normalPairs,
                         uint32_t numIndices,
                         DecodeIndex&& decodeIndex,
                         Yield&& yield)
{
    uint32_t bitIndex = 0;
    size_t flagsIndex = 0;
    uint32_t normalIndex = 0;

    CompactEdge output{};  // :49 同一对象原地复用
    for (uint32_t i = 0; i < numIndices; i++) {
        if (flagsIndex >= visibilityFlags.byteLength)
            break;  // 生产侧防御：可见性流耗尽（参考越界读返回 undefined→0）
        uint8_t const visibility =
            static_cast<uint8_t>((visibilityFlags.data[flagsIndex] >> bitIndex) & 3);  // :51
        bitIndex += 2;                                                                  // :52
        if (bitIndex == 8) {                                                            // :53
            bitIndex = 0;
            flagsIndex++;
        }

        if (compactedges::kHidden == visibility
            || compactedges::kVisibleDuplicate == visibility) {  // :58
            continue;
        }

        output.index0 = decodeIndex(i);                                     // :62
        output.index1 = decodeIndex(i % 3 == 2 ? i - 2 : i + 1);            // :63
        if (compactedges::kSilhouette == visibility) {                      // :64
            if (!normalPairs)
                continue;  // 生产侧防御（:65 assert 的 release 语义）
            size_t const pairBytes = normalPairs->byteLength;
            if (static_cast<size_t>(normalIndex) * 4u + 4u > pairBytes)
                break;     // 生产侧防御：normalPairs 耗尽，终止迭代
            // u32 = normal1 | (normal2 << 16)（ImdlSchema.ts:264）——LE 逐字节
            // 组装（TS Uint32Array 平台端序的显式化；imdl 线上为 LE）。
            uint8_t const* p = normalPairs->data + static_cast<size_t>(normalIndex) * 4u;
            output.normals = static_cast<uint32_t>(p[0])
                | (static_cast<uint32_t>(p[1]) << 8)
                | (static_cast<uint32_t>(p[2]) << 16)
                | (static_cast<uint32_t>(p[3]) << 24);
            normalIndex++;
        } else {
            output.normals.reset();  // :68 normals = undefined
        }

        yield(output);  // :71
    }
}

/// Compute the edge lookup table's dimensions/partition layout.
/// Ported from: EdgeParams.ts:114-149 calculateEdgeTableParams（数值逐行 1:1
/// ——段边 6B=1.5 RGBA、silhouette 10B=2.5 RGBA、宽度对齐 15 RGBA=60B=6/10/4
/// 的最小公倍数、分区对齐填充）。
struct EdgeTableInfo {
    /// Width of the table.（EdgeParams.ts:80-81）
    uint32_t width = 0;
    /// Height of the table.（EdgeParams.ts:82-83）
    uint32_t height = 0;
    /// The number of padding bytes inserted between the partitions.
    /// （EdgeParams.ts:84-85——参考注释文案与字段名的错位照原文保留。）
    uint32_t silhouettePadding = 0;
    /// The starting byte index of silhouettes（EdgeParams.ts:86-87）。
    uint32_t silhouetteStartByteIndex = 0;
};

EdgeTableInfo calculateEdgeTableParams(uint32_t numSegmentEdges, uint32_t numSilhouettes,
                                       uint32_t maxSize);

/// Convert an imdl compact edge set to indexed edge params.
/// Ported from: CompactEdges.ts:84-129 indexedEdgeParamsFromCompactEdges。
///
/// 返回 nullopt 当 numTotalEdges <= 0（:87-88 undefined）。C++ 所有权适配（§3.4
/// ——TS 产物由 GC 持有）：展开的索引流与查找表字节由返回值内的
/// ImdlIndexedEdgeParams::ownedIndices/ownedEdgeTable 持有，indices/edges.data
/// 两个视图指入其中（vector move 只转移堆缓冲，视图保持有效）。
std::optional<ImdlIndexedEdgeParams> indexedEdgeParamsFromCompactEdges(CompactEdgeParams const& compact);

END_DQ_RENDER_NAMESPACE
