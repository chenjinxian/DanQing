// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts (IndexedPolyfaceVisitor concrete, L635+)
// DanQing dqGeom — IndexedPolyfaceVisitor implementation (minimal subset for MeshBuilder)
#include "dqGeom/PolyfaceVisitor.h"

#include <cstdlib>

BEGIN_DQ_GEOM_NAMESPACE

// 1:1 IndexedPolyfaceVisitor.create(polyface, numWrap).
dqBase::RefPtr<IndexedPolyfaceVisitor> IndexedPolyfaceVisitor::create(const IndexedPolyface& polyface, int numWrap)
{
    return dqBase::RefPtr<IndexedPolyfaceVisitor>(new IndexedPolyfaceVisitor(polyface, numWrap));
}

IndexedPolyfaceVisitor::IndexedPolyfaceVisitor(const IndexedPolyface& polyface, int numWrap)
    : m_polyface(&polyface)
    , m_numWrap(numWrap)
    , m_facetOrdinal(kBeforeFirst)
    , m_requireNormals(!polyface.Data().normalIndex.empty())
{
    // 1:1 IndexedPolyfaceVisitor ctor :34-35——polyface 有 auxData 则建 visitor 空骨架
    // （createForVisitor：值数组留空、indices 空——AuxData.ts:233-245）。
    if (polyface.Data().auxData.has_value())
        m_facetAuxData = polyface.Data().auxData->createForVisitor();
}

void IndexedPolyfaceVisitor::LoadCurrentFacet()
{
    m_facetPointIndex.clear();
    m_facetNormalIndex.clear();
    m_facetEdgeVisible.clear();

    PolyfaceData const& data = m_polyface->Data();
    const size_t facetStart = m_polyface->FacetIndex0(m_facetOrdinal);
    const size_t facetEnd = m_polyface->FacetIndex1(m_facetOrdinal);
    m_readIndex = facetStart;

    const bool haveNormals = m_requireNormals;
    for (size_t edge = facetStart; edge < facetEnd; ++edge) {
        if (edge < data.pointIndex.size())
            m_facetPointIndex.push_back(data.pointIndex[edge]);
        if (haveNormals && edge < data.normalIndex.size())
            m_facetNormalIndex.push_back(data.normalIndex[edge]);
        if (edge < data.edgeVisible.size())
            m_facetEdgeVisible.push_back(data.edgeVisible[edge]);
    }

    m_pointCount = m_facetPointIndex.size();
    m_normalCount = m_facetNormalIndex.size();

    // 1:1 numWrap: replicate the first numWrap vertices at the end (reference pads visitor arrays so
    // adjacent-edge lookups wrap without modular indexing). MeshBuilder uses numWrap=0.
    for (int w = 0; w < m_numWrap && m_pointCount > 0; ++w) {
        const size_t src = static_cast<size_t>(w) % m_pointCount;
        m_facetPointIndex.push_back(m_facetPointIndex[src]);
        if (haveNormals && src < m_facetNormalIndex.size())
            m_facetNormalIndex.push_back(m_facetNormalIndex[src]);
        if (src < m_facetEdgeVisible.size())
            m_facetEdgeVisible.push_back(m_facetEdgeVisible[src]);
    }

    // Ported from: PolyfaceData.ts:386-408 gatherIndexedData 的 auxData 腿——逐通道逐帧把
    // facet 顶点的通道值搬进 facet-local 数组（copyValues 块拷贝），indices 直拷贝父段。
    // DanQing 父 auxData.indices 为 signed 1-based → 数据块下标 = abs(idx)-1（AuxData.h
    // 文件头 EQUIVALENCE——参考为 0-based 直读）。
    if (m_facetAuxData.has_value() && m_polyface->Data().auxData.has_value()) {
        auto& localAux = *m_facetAuxData;
        auto const& parentAux = *m_polyface->Data().auxData;
        if (localAux.channels.size() == parentAux.channels.size()) {
            for (size_t iChannel = 0; iChannel < localAux.channels.size(); iChannel++) {
                auto& thisChannel = localAux.channels[iChannel];
                auto const& otherChannel = parentAux.channels[iChannel];
                size_t const blockSize = thisChannel.entriesPerValue();
                if (thisChannel.data.size() == otherChannel.data.size()) {
                    for (size_t iData = 0; iData < thisChannel.data.size(); iData++) {
                        auto& thisData = thisChannel.data[iData];
                        auto const& otherData = otherChannel.data[iData];
                        thisData.values.resize(m_pointCount * blockSize);
                        for (size_t i = 0; i < m_pointCount; i++) {
                            size_t const edgePos = facetStart + i;
                            if (edgePos >= parentAux.indices.size())
                                break;
                            size_t const srcBlock =
                                static_cast<size_t>(std::abs(parentAux.indices[edgePos])) - 1;
                            thisData.copyValues(otherData, i, srcBlock, blockSize);
                        }
                        // copy wrapped auxData（:398-399——numWrap 复制品回写 facet-local 槽；
                        // 消费面 MeshBuilder 恒 numWrap=0，本腿形态随参考原样）
                        for (int i = 0; i < m_numWrap && m_pointCount > 0; ++i) {
                            size_t const src = static_cast<size_t>(i) % m_pointCount;
                            size_t const edgePos = facetStart + src;
                            if (edgePos >= parentAux.indices.size())
                                break;
                            size_t const dstBlock =
                                static_cast<size_t>(std::abs(parentAux.indices[edgePos])) - 1;
                            if (dstBlock * blockSize < thisData.values.size())
                                thisData.copyValues(thisData, dstBlock, src, blockSize);
                        }
                    }
                }
            }
            // copy auxData index（:403-407——父段直拷贝 + wrap 复制）
            localAux.indices.clear();
            for (size_t i = 0; i < m_pointCount; i++) {
                size_t const edgePos = facetStart + i;
                if (edgePos < parentAux.indices.size())
                    localAux.indices.push_back(parentAux.indices[edgePos]);
            }
            for (int i = 0; i < m_numWrap && m_pointCount > 0; ++i)
                localAux.indices.push_back(localAux.indices[static_cast<size_t>(i) % m_pointCount]);
        }
    }
}

// 1:1 PolyfaceVisitor.reset — restart before the first facet.
void IndexedPolyfaceVisitor::Reset()
{
    m_facetOrdinal = kBeforeFirst;
    m_readIndex = 0;
    m_pointCount = 0;
    m_normalCount = 0;
    m_facetPointIndex.clear();
    m_facetNormalIndex.clear();
    m_facetEdgeVisible.clear();
}

// 1:1 PolyfaceVisitor.moveToNextFacet — advance to next facet and load it.
bool IndexedPolyfaceVisitor::MoveToNextFacet()
{
    const size_t facetCount = m_polyface->FacetCount();
    if (facetCount == 0)
        return false;

    // First call after Reset(): load facet 0. Subsequent calls advance.
    const size_t next = (m_facetOrdinal == kBeforeFirst) ? 0 : m_facetOrdinal + 1;
    if (next >= facetCount)
        return false;

    m_facetOrdinal = next;
    LoadCurrentFacet();
    return true;
}

// 1:1 PolyfaceVisitor.moveToReadIndex — load the facet beginning at `index` (an index-array position).
bool IndexedPolyfaceVisitor::MoveToReadIndex(size_t index)
{
    auto const& starts = m_polyface->FacetStart();
    for (size_t f = 0; f + 1 < starts.size(); ++f) {
        if (starts[f] == index) {
            m_facetOrdinal = f;
            LoadCurrentFacet();
            return true;
        }
    }
    return false;
}

// 1:1 point.getPoint3dAtUncheckedPointIndex(i) — facet-vertex i's position.
Point3d IndexedPolyfaceVisitor::GetPoint(size_t i) const
{
    if (i >= m_facetPointIndex.size())
        return Point3d::FromZero();
    // pointIndex is 1-based signed (+visible / -hidden); GetPoint expects 1-based.
    const int32_t idx = m_facetPointIndex[i];
    return m_polyface->Data().GetPoint(static_cast<int32_t>(std::abs(idx)));
}

// 1:1 visitor.getNormal(i) — facet-vertex i's normal.
Vector3d IndexedPolyfaceVisitor::GetNormal(size_t i) const
{
    if (i >= m_facetNormalIndex.size() || !m_requireNormals)
        return Vector3d::FromZero();
    return m_polyface->Data().GetNormal(m_facetNormalIndex[i]);
}

// 1:1 visitor.clientPointIndex(i) — the polyface's 1-based point index for facet-vertex i.
size_t IndexedPolyfaceVisitor::ClientPointIndex(size_t i) const
{
    if (i >= m_facetPointIndex.size())
        return 0;
    return static_cast<size_t>(std::abs(m_facetPointIndex[i]));
}

// 1:1 visitor.edgeVisible[i].
bool IndexedPolyfaceVisitor::EdgeVisible(size_t i) const
{
    if (i >= m_facetEdgeVisible.size())
        return true;
    return m_facetEdgeVisible[i];
}

END_DQ_GEOM_NAMESPACE
