// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — IndexedPolyface implementation
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Polyface.h
#include "dqGeom/IndexedPolyface.h"
#include "dqGeom/GeometryHandler.h"
#include "dqGeom/PolyfaceVisitor.h"

BEGIN_DQ_GEOM_NAMESPACE

dqBase::RefPtr<IndexedPolyface> IndexedPolyface::create(bool needNormals, bool needColors,
                                                         bool twoSided, bool needParams)
{
    auto* pf = new IndexedPolyface();
    pf->m_data.twoSided = twoSided;
    if (needNormals) pf->m_data.normals.reserve(64);
    if (needColors) pf->m_data.colors.reserve(64);
    if (needParams) pf->m_data.params.reserve(64);
    return dqBase::RefPtr<IndexedPolyface>(pf);
}

// --- GeometryQuery interface ---

Range3d IndexedPolyface::Range() const
{
    return m_data.ComputeRange();
}

void IndexedPolyface::ExtendRange(Range3d& range) const
{
    m_data.ExtendRange(range);
}

bool IndexedPolyface::TryTransformInPlace(Transform const& transform)
{
    for (auto& p : m_data.points) {
        p = transform.MultiplyPoint3d(p);
    }
    for (auto& n : m_data.normals) {
        n = transform.MultiplyVector(n);
    }
    return true;
}

dqBase::RefPtr<GeometryQuery> IndexedPolyface::clone() const
{
    auto* clone = new IndexedPolyface();
    clone->m_data = m_data;
    clone->m_facetStart = m_facetStart;
    return dqBase::RefPtr<IndexedPolyface>(clone);
}

dqBase::RefPtr<GeometryQuery> IndexedPolyface::CloneTransformed(Transform const& transform) const
{
    auto cloned = clone();
    cloned->TryTransformInPlace(transform);
    return cloned;
}

bool IndexedPolyface::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    return other.Category() == GeometryCategory::Polyface;
}

bool IndexedPolyface::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& otherPf = static_cast<IndexedPolyface const&>(other);

    // Channel coverage aligned with the reference PolyfaceData.isAlmostEqual
    // (PolyfaceData.ts:184-214): points/normals/params by tolerance, index and
    // color arrays exact, plus twoSided/expectedClosure/edgeVisible. (TD-13:
    // previously only points/pointIndex were compared.)
    auto const& a = m_data;
    auto const& b = otherPf.m_data;

    if (a.points.size() != b.points.size() ||
        a.pointIndex.size() != b.pointIndex.size() ||
        a.normals.size() != b.normals.size() ||
        a.normalIndex.size() != b.normalIndex.size() ||
        a.colors.size() != b.colors.size() ||
        a.colorIndex.size() != b.colorIndex.size() ||
        a.params.size() != b.params.size() ||
        a.paramIndex.size() != b.paramIndex.size() ||
        a.edgeVisible.size() != b.edgeVisible.size())
        return false;

    for (size_t i = 0; i < a.points.size(); ++i) {
        if (!a.points[i].AlmostEqual(b.points[i], tol)) return false;
    }
    for (size_t i = 0; i < a.pointIndex.size(); ++i) {
        if (a.pointIndex[i] != b.pointIndex[i]) return false;
    }
    for (size_t i = 0; i < a.normals.size(); ++i) {
        if (!a.normals[i].IsEqual(b.normals[i], tol)) return false;
    }
    for (size_t i = 0; i < a.normalIndex.size(); ++i) {
        if (a.normalIndex[i] != b.normalIndex[i]) return false;
    }
    for (size_t i = 0; i < a.colors.size(); ++i) {
        if (a.colors[i] != b.colors[i]) return false;
    }
    for (size_t i = 0; i < a.colorIndex.size(); ++i) {
        if (a.colorIndex[i] != b.colorIndex[i]) return false;
    }
    for (size_t i = 0; i < a.params.size(); ++i) {
        if (!a.params[i].IsEqual(b.params[i], tol)) return false;
    }
    for (size_t i = 0; i < a.paramIndex.size(); ++i) {
        if (a.paramIndex[i] != b.paramIndex[i]) return false;
    }
    for (size_t i = 0; i < a.edgeVisible.size(); ++i) {
        if (a.edgeVisible[i] != b.edgeVisible[i]) return false;
    }
    if (a.twoSided != b.twoSided) return false;
    if (a.expectedClosure != b.expectedClosure) return false;
    return true;
}

// Ported from: itwinjs-core IndexedPolyface.dispatchToGeometryHandler → handleIndexedPolyface.
void IndexedPolyface::DispatchToHandler(GeometryHandler& handler)
{
    handler.HandleIndexedPolyface(*this);
}

// 1:1 IndexedPolyface.createVisitor(numWrap) → IndexedPolyfaceVisitor.create(this, numWrap).
// Minimal PolyfaceVisitor subset ported (PolyfaceVisitor.h); consumed by MeshBuilder.addFromPolyface.
dqBase::RefPtr<PolyfaceVisitor> IndexedPolyface::CreateVisitor(int numWrap) const
{
    return IndexedPolyfaceVisitor::create(*this, numWrap);
}

// --- Building methods ---

int32_t IndexedPolyface::AddPoint(Point3d const& point)
{
    m_data.points.push_back(point);
    return static_cast<int32_t>(m_data.points.size());  // 1-based
}

int32_t IndexedPolyface::AddNormal(Vector3d const& normal)
{
    m_data.normals.push_back(normal);
    return static_cast<int32_t>(m_data.normals.size());
}

int32_t IndexedPolyface::AddColor(uint32_t color)
{
    m_data.colors.push_back(color);
    return static_cast<int32_t>(m_data.colors.size());
}

// Ported from: itwinjs-core IndexedPolyface addParam (Polyface.ts) — 1-based.
int32_t IndexedPolyface::AddParam(Point2d const& param)
{
    m_data.params.push_back(param);
    return static_cast<int32_t>(m_data.params.size());
}

void IndexedPolyface::AddParamIndex(int32_t index)
{
    m_data.paramIndex.push_back(index);
}

void IndexedPolyface::AddPointIndex(int32_t index, bool visible)
{
    // Sign convention: positive = visible, negative = hidden
    m_data.pointIndex.push_back(visible ? index : -index);
    m_data.edgeVisible.push_back(visible);
}

void IndexedPolyface::AddNormalIndex(int32_t index)
{
    m_data.normalIndex.push_back(index);
}

void IndexedPolyface::AddColorIndex(int32_t index)
{
    m_data.colorIndex.push_back(index);
}

void IndexedPolyface::TerminateFacet()
{
    // Push current index count as the start of the next facet
    m_facetStart.push_back(static_cast<size_t>(m_data.pointIndex.size()));
}

END_DQ_GEOM_NAMESPACE
