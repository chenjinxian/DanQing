// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — IndexedPolyface (indexed mesh container)
//
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts (IndexedPolyface)
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Polyface.h (PolyfaceHeader)
//
// Concrete mesh class.  Owns PolyfaceData + facetStart array for splitting
// flat index arrays into per-facet loops.
//
// Index convention: 1-based signed indices (matching imodel-native).
// Positive = visible edge, negative = hidden edge, 0 = face loop terminator.
// The absolute value minus one gives the zero-based data array index.
// 替代 Qt QVector，底层用 std::vector。
#pragma once

#include "Polyface.h"   // GeometryQuery + PolyfaceData

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// IndexedPolyface — indexed face mesh (derives Polyface; 1:1 Polyface.ts:110)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT IndexedPolyface : public Polyface {
public:
    // --- Factory ---
    static dqBase::RefPtr<IndexedPolyface> create(bool needNormals = false,
                                                   bool needColors = false,
                                                   bool twoSided = false,
                                                   bool needParams = false);

    // --- GeometryQuery interface (Category inherited final from Polyface) ---
    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool TryTransformInPlace(Transform const& transform) override;
    dqBase::RefPtr<GeometryQuery> clone() const override;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;
    void DispatchToHandler(GeometryHandler& handler) override;

    // --- Data access (Data() inherited from Polyface) ---

    // --- Facet navigation ---
    size_t FacetCount() const noexcept override
    {
        return m_facetStart.size() > 1 ? m_facetStart.size() - 1 : 0;
    }

    size_t FacetIndex0(size_t facetIndex) const noexcept
    {
        if (facetIndex >= FacetCount()) return 0;
        return m_facetStart[facetIndex];
    }

    size_t FacetIndex1(size_t facetIndex) const noexcept
    {
        if (facetIndex >= FacetCount()) return 0;
        return m_facetStart[facetIndex + 1];
    }

    size_t NumEdgeInFacet(size_t facetIndex) const noexcept
    {
        return FacetIndex1(facetIndex) - FacetIndex0(facetIndex);
    }

    std::vector<size_t> const& FacetStart() const noexcept { return m_facetStart; }

    // --- Building methods ---
    int32_t AddPoint(Point3d const& point);
    int32_t AddNormal(Vector3d const& normal);
    int32_t AddColor(uint32_t color);
    int32_t AddParam(Point2d const& param);
    void AddPointIndex(int32_t index, bool visible = true);
    void AddNormalIndex(int32_t index);
    void AddColorIndex(int32_t index);
    void AddParamIndex(int32_t index);
    void TerminateFacet();

    // --- Polyface overrides (twoSided/expectedClosure accessors inherited from Polyface) ---
    // 1:1 IndexedPolyface.createVisitor(numWrap). Returns an IndexedPolyfaceVisitor (minimal subset,
    // PolyfaceVisitor.h) consumed by MeshBuilder.addFromPolyface.
    dqBase::RefPtr<PolyfaceVisitor> CreateVisitor(int numWrap) const override;
    bool IsEmpty() const noexcept override { return FacetCount() == 0; }

private:
    IndexedPolyface() : Polyface(PolyfaceData{}) {}

    std::vector<size_t> m_facetStart = {0};  // start indices into index arrays per facet
};

using IndexedPolyfacePtr = dqBase::RefPtr<IndexedPolyface>;

END_DQ_GEOM_NAMESPACE
