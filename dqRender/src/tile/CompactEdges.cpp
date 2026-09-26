// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — compact imdl edges → indexed edge params expansion
// Ported from: itwinjs-core core/frontend/src/common/imdl/CompactEdges.ts
//              (:84-129 indexedEdgeParamsFromCompactEdges)
//              + core/frontend/src/common/internal/render/EdgeParams.ts:114-149
//              (calculateEdgeTableParams)
#include "CompactEdges.h"

#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

// Ported from: EdgeParams.ts:114-149 calculateEdgeTableParams（数值逐行 1:1）。
EdgeTableInfo calculateEdgeTableParams(uint32_t numSegmentEdges, uint32_t numSilhouettes,
                                       uint32_t maxSize)
{
    // Each segment edge requires 2 24-bit indices = 6 bytes = 1.5 RGBA values.
    // Each silhouette requires the same as segment edge plus 2 16-bit
    // oct-encoded normals = 10 bytes = 2.5 RGBA values. (:115-116)
    double const nRgbaRequiredF = std::ceil(1.5 * static_cast<double>(numSegmentEdges)
                                            + 2.5 * static_cast<double>(numSilhouettes));  // :117
    uint32_t nRgbaRequired = static_cast<uint32_t>(nRgbaRequiredF);

    uint32_t const silhouetteStartByteIndex = numSegmentEdges * 6;  // :118
    uint32_t silhouettePadding = 0;                                 // :119
    uint32_t width = nRgbaRequired;                                 // :120
    uint32_t height = 1;                                            // :121
    if (nRgbaRequired >= maxSize) {                                 // :122
        // Make roughly square to reduce unused space in last row. (:123-124)
        width = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nRgbaRequired))));
        // Each entry's data must fit on the same row. 15 RGBA = 60 bytes =
        // lowest common multiple of 6, 10, and 4. (:125-126)
        uint32_t const remainder = width % 15;
        if (0 != remainder)
            width += 15 - remainder;

        // If the table contains both segments and silhouettes, there may be one
        // row containing a mix of the two where padding is required between
        // them. (:130-133)
        if (numSilhouettes > 0 && numSegmentEdges > 0) {
            uint32_t const silOffset = silhouetteStartByteIndex % 60;  // some multiple of 6.
            silhouettePadding = (60 - silOffset) % 10;
            nRgbaRequired += static_cast<uint32_t>(std::ceil(static_cast<double>(silhouettePadding) / 4.0));
        }

        height = (nRgbaRequired + width - 1) / width;  // :138 Math.ceil(nRgbaRequired / width)
        if (width * height < nRgbaRequired)            // :139-140
            height++;
    }

    return {
        width,
        height,
        silhouettePadding,
        silhouetteStartByteIndex,
    };
}

namespace {

// Ported from: CompactEdges.ts:75-79 setUint24（小端 3 字节写入）。
void setUint24(std::vector<uint8_t>& edgeTable, size_t byteIndex, uint32_t value)
{
    edgeTable[byteIndex + 0] = static_cast<uint8_t>(value & 0x0000ffu);
    edgeTable[byteIndex + 1] = static_cast<uint8_t>((value & 0x00ff00u) >> 8);
    edgeTable[byteIndex + 2] = static_cast<uint8_t>((value & 0xff0000u) >> 16);
}

}  // namespace

// Ported from: CompactEdges.ts:84-129 indexedEdgeParamsFromCompactEdges。
std::optional<ImdlIndexedEdgeParams> indexedEdgeParamsFromCompactEdges(CompactEdgeParams const& compact)
{
    uint32_t const numSilhouettes =
        compact.normalPairs ? static_cast<uint32_t>(compact.normalPairs->byteLength / 4) : 0;  // :85
    uint32_t const numTotalEdges = compact.numVisibleEdges + numSilhouettes;                   // :86
    if (numTotalEdges <= 0)                                                                    // :87-88
        return std::nullopt;

    // Each edge is a quad consisting of six vertices. Each vertex is an
    // identical 24-bit index into the lookup table. (:90-94)
    VertexIndices indices(std::vector<uint8_t>(numTotalEdges * 6 * 3, 0));
    for (uint32_t i = 0; i < numTotalEdges; i++)
        for (uint32_t j = 0; j < 6; j++)
            indices.setNthIndex(i * 6 + j, i);

    EdgeTableInfo const table = calculateEdgeTableParams(
        compact.numVisibleEdges, numSilhouettes, compact.maxEdgeTableDimension);  // :96
    std::vector<uint8_t> edgeTable(static_cast<size_t>(table.width) * table.height * 4, 0);  // :97

    uint32_t curVisibleIndex = 0;   // :99
    uint32_t curSilhouetteIndex = 0;  // :100
    // :101-117——迭代展开；visible 边写下分区（6B/边），silhouette 写上分区
    //（10B/边 = 两端点 24-bit + 法线对 u32）。
    compactEdgeIterator(
        compact.visibility, compact.normalPairs, static_cast<uint32_t>(compact.vertexIndices.length()),
        [&compact](uint32_t vertIdx) { return compact.vertexIndices.decodeIndex(vertIdx); },
        [&](CompactEdge const& edge) {
            if (!edge.normals.has_value()) {
                uint32_t const index = curVisibleIndex++;
                size_t const byteIndex = static_cast<size_t>(index) * 6;
                setUint24(edgeTable, byteIndex, edge.index0);
                setUint24(edgeTable, byteIndex + 3, edge.index1);
            } else {
                uint32_t const index = curSilhouetteIndex++;
                size_t const byteIndex = static_cast<size_t>(table.silhouetteStartByteIndex)
                    + table.silhouettePadding + static_cast<size_t>(index) * 10;
                setUint24(edgeTable, byteIndex, edge.index0);
                setUint24(edgeTable, byteIndex + 3, edge.index1);
                edgeTable[byteIndex + 6] = static_cast<uint8_t>(*edge.normals & 0xffu);
                edgeTable[byteIndex + 7] = static_cast<uint8_t>((*edge.normals & 0xff00u) >> 8);
                edgeTable[byteIndex + 8] = static_cast<uint8_t>((*edge.normals & 0xff0000u) >> 16);
                edgeTable[byteIndex + 9] = static_cast<uint8_t>((*edge.normals & 0xff000000u) >> 24);
            }
        });

    // :119-128——返回 indexed 形态（C++：字节归 ownedIndices/ownedEdgeTable 持有，
    // 视图指入；vector move 转移堆缓冲不改指针）。
    ImdlIndexedEdgeParams out;
    out.ownedIndices = std::move(indices.data);
    out.ownedEdgeTable = std::move(edgeTable);
    out.indices = ImdlByteView{out.ownedIndices.data(), out.ownedIndices.size()};
    out.edges.data = ImdlByteView{out.ownedEdgeTable.data(), out.ownedEdgeTable.size()};
    out.edges.width = table.width;
    out.edges.height = table.height;
    out.edges.numSegments = compact.numVisibleEdges;  // :125
    out.edges.silhouettePadding = table.silhouettePadding;
    return out;
}

END_DQ_RENDER_NAMESPACE
