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
