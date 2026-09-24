// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts (Polyface abstract base, lines 31-103)
// DanQing dqGeom — Polyface (abstract mesh providing facet iteration)
//
// 保真依据：逐位移植 Polyface.ts 的抽象基类。Polyface 持有 PolyfaceData，提供 twoSided/
// expectedClosure 访问、抽象 createVisitor/isEmpty、facetCount（默认 0，IndexedPolyface 覆写）、
// 静态 areIndicesValid。具体类 IndexedPolyface derive 自本类。
// PolyfaceVisitor 接口（Polyface.ts:714）及 IndexedPolyfaceVisitor 暂 Phase-N（前向声明，
// addPolyface 路径不依赖 visitor——直接经 IndexedPolyface 的 FacetCount/FacetIndex/pointIndex 读 facet）。
#pragma once

#include "GeometryQuery.h"
#include "PolyfaceData.h"

#include <cstdint>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

class PolyfaceVisitor; // forward — minimal interface in PolyfaceVisitor.h (1:1 Polyface.ts:714 subset)

// Polyface — abstract mesh structure providing a PolyfaceVisitor for facet iteration.
// (Ported from: itwinjs-core Polyface.ts)
class DQ_GEOM_EXPORT Polyface : public GeometryQuery {
public:
    ~Polyface() override;

    // --- GeometryQuery ---
    GeometryCategory Category() const noexcept final { return GeometryCategory::Polyface; }

    // --- Underlying data (1:1 Polyface.data) ---
    PolyfaceData const& Data() const noexcept { return m_data; }
    PolyfaceData& Data() noexcept { return m_data; }

    // --- twoSided flag (1:1 Polyface.twoSided get/set) ---
    bool TwoSided() const noexcept { return m_data.twoSided; }
    void SetTwoSided(bool v) noexcept { m_data.twoSided = v; }

    // --- expectedClosure: 0=unknown, 1=open sheet, 2=closed solid (1:1 Polyface.expectedClosure) ---
    uint32_t ExpectedClosure() const noexcept { return m_data.expectedClosure; }
    void SetExpectedClosure(uint32_t closure) noexcept { m_data.expectedClosure = closure; }

    // --- Facet iteration (1:1 Polyface.createVisitor / isEmpty / facetCount) ---
    // Create a visitor over facets. Full PolyfaceVisitor port is Phase-N.
    virtual dqBase::RefPtr<PolyfaceVisitor> CreateVisitor(int numWrap) const = 0;
    // True if this polyface has no facets.
    virtual bool IsEmpty() const noexcept = 0;
    // Number of facets (default 0; overridden by IndexedPolyface).
    virtual size_t FacetCount() const noexcept { return 0; }

    // Validate indices [posA, posB) into a data array of length dataLength (1:1 Polyface.areIndicesValid).
    static bool AreIndicesValid(std::vector<int32_t> const& indices, size_t posA, size_t posB,
                                size_t dataLength) noexcept;

protected:
    // 1:1 Polyface protected ctor(data). m_data is protected (subclasses access directly, mirroring `this.data`).
    explicit Polyface(PolyfaceData data) : m_data(std::move(data)) {}

    PolyfaceData m_data;
};

using PolyfacePtr = dqBase::RefPtr<Polyface>;

END_DQ_GEOM_NAMESPACE
