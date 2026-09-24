// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/curve/Path.ts
// DanQing dqGeom — Path (open chain of CurvePrimitive joining head-to-tail; does not bound a region)
//
// 保真依据：逐位移植 Path.ts。CurveChain 基类提供通用 GeometryQuery 覆写（clone/Range/
// IsAlmostEqual 等经 GetCurveCollectionType + children 驱动）；Path 仅覆写类型特化部分。
#pragma once

#include "CurveCollection.h"
#include "CurvePrimitive.h"
#include "GeometryHandler.h"
#include "Point3d.h"
#include "StrokeOptions.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Path — open curve chain (1:1 itwinjs-core Path).
class DQ_GEOM_EXPORT Path : public CurveChain {
public:
    ~Path() override = default;

    // --- Factories (1:1 Path.create / createArray) ---
    // Empty path (1:1 `new Path()`).
    static dqBase::RefPtr<Path> Create();
    // From a list of curve primitives (captured). 1:1 Path.create(...curves).
    static dqBase::RefPtr<Path> Create(std::vector<CurvePrimitivePtr> const& curves);
    // From a point array (wrapped as a single LineString3d child). 1:1 Path.create(Point3d[]).
    static dqBase::RefPtr<Path> Create(std::vector<Point3d> const& points);
    // Alias of Create(curves). 1:1 Path.createArray.
    static dqBase::RefPtr<Path> CreateArray(std::vector<CurvePrimitivePtr> const& curves) { return Create(curves); }

    // --- CurveChain overrides ---
    CurveCollectionType GetCurveCollectionType() const noexcept override { return CurveCollectionType::Path; }
    dqBase::RefPtr<CurveChain> CloneEmptyPeer() const override { return Create(); }
    dqBase::RefPtr<CurveChain> CloneStroked(StrokeOptions const& options) const override;

    // MicroStation CurveVector boundary type (1:1 Path.dgnBoundaryType).
    int DgnBoundaryType() const noexcept { return 1; }

    // --- GeometryQuery dispatch ---
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandlePath(*this); }
};

using PathPtr = dqBase::RefPtr<Path>;

END_DQ_GEOM_NAMESPACE
