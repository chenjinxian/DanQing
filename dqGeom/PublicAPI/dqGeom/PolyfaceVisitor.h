// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts (PolyfaceVisitor interface L714,
//              IndexedPolyfaceVisitor concrete)
// DanQing dqGeom — PolyfaceVisitor (minimal facet iterator consumed by MeshBuilder.addFromPolyface)
//
// 保真依据：移植 PolyfaceVisitor 接口的最小子集（moveToNextFacet/reset/pointCount/normalCount/
// paramCount/requireNormals/GetPoint/GetNormal/ClientPointIndex/EdgeVisible/currentReadIndex/
// moveToReadIndex）——即 itwinjs frontend MeshBuilder.addFromPolyface{,Visitor} 实际读取的成员
// （完整 PolyfaceVisitor 还含 createSubsetVisitor/pushDataFrom/pushInterpolatedDataFrom/clearArrays
// 等建图用 API，留 Phase-N）。IndexedPolyfaceVisitor 在 moveToNextFacet 时把当前 facet 的
// pointIndex/normalIndex/edgeVisible 切片物化到本地数组（1:1 参考 visitor 持本地 PolyfaceData）；
// GetPoint/GetNormal/ClientPointIndex/EdgeVisible 按当前 facet 的 0-based 顶点索引读取。
//
// 命名：访问器取 PascalCase（与同胞 IndexedPolyface.h 的 FacetCount/GetPoint/CreateVisitor 一致，
// §3.4 getter→PascalCase 在本 Polyface 家族的既有约定延续），保证 dqGeom Polyface 模块内一致。
//
// 生命周期：visitor 持 raw `const IndexedPolyface*`（参考持 `this._polyface` 引用）；visitor 不得
// 越过 polyface 存活——CreateVisitor 返回的 RefPtr 在 addFromPolyface 局部作用域内用毕即弃。
#pragma once

#include "IndexedPolyface.h"
#include "Point3d.h"
#include "Vector3d.h"

#include <dqBase/RefCounted.h>

#include <cstddef>
#include <cstdint>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// PolyfaceVisitor — abstract facet iterator (1:1 Polyface.ts:714, minimal subset).
// RefCounted<PolyfaceVisitor> so CreateVisitor can return RefPtr<PolyfaceVisitor> (RefCounted CRTP,
// §9; IRefCounted virtual dtor makes cross-type RefPtr<PolyfaceVisitor>↔RefPtr<IndexedPolyfaceVisitor> safe).
class DQ_GEOM_EXPORT PolyfaceVisitor : public dqBase::RefCounted<PolyfaceVisitor> {
public:
    ~PolyfaceVisitor() override = default;

    // Load data for the next facet. Returns false if no more facets.
    // 1:1 PolyfaceVisitor.moveToNextFacet.
    virtual bool MoveToNextFacet() = 0;
    // Restart the visitor before the first facet.
    // 1:1 PolyfaceVisitor.reset.
    virtual void Reset() = 0;
    // Load data for the facet whose index-array start is `index`. Returns false if out of range.
    // 1:1 PolyfaceVisitor.moveToReadIndex.
    virtual bool MoveToReadIndex(size_t index) = 0;
    // The index-array position at which the currently loaded facet begins.
    // 1:1 PolyfaceVisitor.currentReadIndex.
    virtual size_t CurrentReadIndex() const = 0;

    // Number of vertices in the currently loaded facet (1:1 PolyfaceVisitor.pointCount getter).
    virtual size_t PointCount() const = 0;
    // Number of normals available for the currently loaded facet (1:1 normalCount getter).
    virtual size_t NormalCount() const = 0;
    // Number of uv params for the currently loaded facet (1:1 paramCount getter). DanQing: always 0
    // (UV param tessellation deferred; texture-mapping computeUVParams is Phase-N).
    virtual size_t ParamCount() const noexcept { return 0; }
    // True if the polyface carries normals (1:1 PolyfaceData.requireNormals).
    virtual bool RequireNormals() const = 0;

    // Position of facet-vertex i (0-based within facet). 1:1 point.getPoint3dAtUncheckedPointIndex(i).
    virtual Point3d GetPoint(size_t i) const = 0;
    // Normal of facet-vertex i (0-based within facet). 1:1 visitor.getNormal(i).
    virtual Vector3d GetNormal(size_t i) const = 0;
    // The owning polyface's 1-based point index for facet-vertex i. 1:1 visitor.clientPointIndex(i).
    virtual size_t ClientPointIndex(size_t i) const = 0;
    // Edge visibility of facet-edge i. 1:1 visitor.edgeVisible[i].
    virtual bool EdgeVisible(size_t i) const = 0;
};

// IndexedPolyfaceVisitor — concrete visitor over an IndexedPolyface (1:1 Polyface.ts IndexedPolyfaceVisitor).
class DQ_GEOM_EXPORT IndexedPolyfaceVisitor : public PolyfaceVisitor {
public:
    // 1:1 IndexedPolyfaceVisitor.create(polyface, numWrap).
    static dqBase::RefPtr<IndexedPolyfaceVisitor> create(const IndexedPolyface& polyface, int numWrap = 0);

    bool MoveToNextFacet() override;
    void Reset() override;
    bool MoveToReadIndex(size_t index) override;
    size_t CurrentReadIndex() const noexcept override { return m_readIndex; }

    size_t PointCount() const noexcept override { return m_pointCount; }
    size_t NormalCount() const noexcept override { return m_normalCount; }
    bool RequireNormals() const noexcept override { return m_requireNormals; }

    Point3d GetPoint(size_t i) const override;
    Vector3d GetNormal(size_t i) const override;
    size_t ClientPointIndex(size_t i) const override;
    bool EdgeVisible(size_t i) const override;

private:
    IndexedPolyfaceVisitor(const IndexedPolyface& polyface, int numWrap);

    // Materialize the current facet's index/edge data from m_readIndex into the local slices.
    void LoadCurrentFacet();

    const IndexedPolyface* m_polyface;     // raw; visitor must not outlive polyface (1:1 this._polyface)
    int m_numWrap = 0;                       // 1:1 numWrap (replicated trailing vertices)
    size_t m_facetOrdinal = 0;              // 0-based index of currently-loaded facet (kBeforeFirst = none)
    size_t m_readIndex = 0;                  // index-array start of current facet (facetStart value)
    size_t m_pointCount = 0;                 // vertices in current facet (excluding numWrap replicas)
    size_t m_normalCount = 0;               // normals available in current facet
    bool m_requireNormals = false;          // polyface has a normal index array
    std::vector<int32_t> m_facetPointIndex;  // current facet's 1-based point indices (signed: +/-visible)
    std::vector<int32_t> m_facetNormalIndex; // current facet's 1-based normal indices (parallel)
    std::vector<bool> m_facetEdgeVisible;    // current facet's per-edge visibility (parallel)

    static constexpr size_t kBeforeFirst = static_cast<size_t>(-1);
};

END_DQ_GEOM_NAMESPACE
