// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/curve/CurveCollection.ts (CurveCollection + CurveChain)
// DanQing dqGeom — curve collection hierarchy (Path / Loop base classes)
//
// 保真依据（CLAUDE.md §7.2）：CurveCollection 与 CurveChain 逐位移植自 itwinjs-core
// CurveCollection.ts。类型层级：GeometryQuery → CurveCollection → CurveChain → {Path, Loop}。
// 方法名 PascalCase（对齐 CurvePrimitive::CreateLine / GeometryQuery::TryTransformInPlace）。
// 重查询方法（sumLengths/closestPoint/emitTangents/region 布尔——依赖 CurveLocationDetail 及各
// *Context）依赖类型尚未移植，按 CurvePrimitive.h 先例 phasing（声明 + TODO Phase-N）。
// 区域子类（ParityRegion/UnionRegion/BagOfCurves）由 GeometryHandler.h 前向声明，随几何补全。
#pragma once

#include "GeometryQuery.h"
#include "CurvePrimitive.h"
#include "Point3d.h"
#include "Range3d.h"
#include "StrokeOptions.h"

#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// CurveCollectionType — concrete type discriminator
// (Ported from: itwinjs-core CurveCollection.ts CurveCollectionType)
// ---------------------------------------------------------------------------
enum class CurveCollectionType : int {
    Loop,
    Path,
    // Region subtypes — ported when those classes land (forward-declared in GeometryHandler.h):
    UnionRegion,
    ParityRegion,
    BagOfCurves,
};

// ---------------------------------------------------------------------------
// CurveCollection — abstract base for sets of curves
// (Ported from: itwinjs-core CurveCollection.ts)
// Instantiable forms: CurveChain → {Path, Loop}; region types (future).
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT CurveCollection : public GeometryQuery {
public:
    ~CurveCollection() override = default;

    // --- GeometryQuery ---
    GeometryCategory Category() const noexcept final { return GeometryCategory::CurveCollection; }

    // --- Type discriminator (1:1 curveCollectionType) ---
    virtual CurveCollectionType GetCurveCollectionType() const noexcept = 0;

    // True for region-capable collection types (Loop/ParityRegion/UnionRegion).
    // Ported from: itwinjs-core CurveCollection.isAnyRegionType (= dgnBoundaryType 2/4/5).
    bool isAnyRegionType() const noexcept
    {
        const auto t = GetCurveCollectionType();
        return t == CurveCollectionType::Loop ||
               t == CurveCollectionType::ParityRegion ||
               t == CurveCollectionType::UnionRegion;
    }

    // --- isInner flag (Loop only; carried on base per reference) ---
    bool IsInner() const noexcept { return m_isInner; }
    void SetInner(bool inner) noexcept { m_isInner = inner; }

    // TODO Phase-N: sumLengths / closestPoint / closestPointXY / emitTangents / allTangents /
    //   closestTangent / checkGapBetweenAdjacentCurves / resolveIn/out region logic —
    //   ported from CurveCollection.ts:<method>; need CurveLocationDetail + Sum/Gap/Plane/Transform contexts.

protected:
    bool m_isInner = false; // 1:1 CurveCollection.isInner (Loop-only semantics)
};

using CurveCollectionPtr = dqBase::RefPtr<CurveCollection>;

// ---------------------------------------------------------------------------
// CurveChain — abstract intermediate: an ordered chain of CurvePrimitive joining head-to-tail
// (Ported from: itwinjs-core CurveCollection.ts CurveChain)
// Instantiable forms: Path (open), Loop (closed planar).
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT CurveChain : public CurveCollection {
public:
    ~CurveChain() override = default;

    // --- Child curve access (1:1 CurveChain.children / _curves) ---
    std::vector<CurvePrimitivePtr> const& Curves() const noexcept { return m_curves; }
    std::vector<CurvePrimitivePtr>& Curves() noexcept { return m_curves; }

    // Return child at index, or null if out of range (1:1 CurveChain.getChild).
    CurvePrimitivePtr GetChild(int index) const noexcept;

    // Add a child curve; returns true if added (1:1 CurveChain.tryAddChild — captures a CurvePrimitive).
    bool TryAddChild(CurvePrimitivePtr const& child) noexcept;

    // Start point of the chain = first child's fraction 0 (1:1 CurveChain.startPoint).
    std::optional<Point3d> StartPoint() const;
    // End point of the chain = last child's fraction 1 (1:1 CurveChain.endPoint).
    std::optional<Point3d> EndPoint() const;

    // Whether chain start/end are within tolerance (1:1 CurveChain.isPhysicallyClosedCurve).
    bool IsPhysicallyClosedCurve(double tolerance = kSmallMetricDistance, bool xyOnly = false) const;

    // Reverse each child in place and reverse child order (1:1 CurveChain.reverseChildrenInPlace).
    void ReverseChildrenInPlace();

    // Extend range by each child's range (1:1 CurveChain.extendRange).
    void ExtendRange(Range3d& range) const override;

    // Return a structural clone with each child stroked to a single LineString3d (1:1 CurveChain.cloneStroked).
    virtual dqBase::RefPtr<CurveChain> CloneStroked(StrokeOptions const& options) const = 0;

    // --- GeometryQuery generic overrides (driven by GetCurveCollectionType / children) ---

    // Return an empty peer of the same concrete type (1:1 Path/Loop.cloneEmptyPeer).
    virtual dqBase::RefPtr<CurveChain> CloneEmptyPeer() const = 0;

    // Bounding box over all children (1:1 CurveCollection.range).
    Range3d Range() const override;
    // Transform each child in place (1:1 CurveCollection.tryTransformInPlace).
    bool TryTransformInPlace(Transform const& transform) override;
    // Deep clone: empty peer + cloned children (1:1 GeometryQuery.clone).
    dqBase::RefPtr<GeometryQuery> clone() const override;
    // clone + transform (1:1 GeometryQuery.cloneTransformed).
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const override;
    // Same concrete class via discriminator (no RTTI; mirrors CurvePrimitive::IsSameGeometryClass).
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    // Equal within tol: same class + same child count + each child almost equal (1:1 CurveCollection.isAlmostEqual).
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

    // TODO Phase-N: cyclicCurvePrimitive / getPackedStrokes / startPointAndDerivative /
    //   endPointAndDerivative — ported from CurveCollection.ts CurveChain; need Ray3d derivative wiring + GrowableXYZArray.

protected:
    std::vector<CurvePrimitivePtr> m_curves; // 1:1 CurveChain._curves
};

using CurveChainPtr = dqBase::RefPtr<CurveChain>;

END_DQ_GEOM_NAMESPACE
