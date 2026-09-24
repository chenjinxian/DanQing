// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/curve/Loop.ts
// DanQing dqGeom — Loop (closed planar curve chain bounding a planar region)
//
// 保真依据：逐位移植 Loop.ts。通用 GeometryQuery 覆写继承自 CurveChain；Loop 仅覆写类型特化部分
// （CreatePolygon / DgnBoundaryType=2 / DispatchToHandler→HandleLoop）+ LoopCurveLoopCurve/SignedLoops 载体。
#pragma once

#include "CurveCollection.h"
#include "CurvePrimitive.h"
#include "GeometryHandler.h"
#include "Point3d.h"
#include "StrokeOptions.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// Loop — closed planar curve chain (1:1 itwinjs-core Loop).
class DQ_GEOM_EXPORT Loop : public CurveChain {
public:
    ~Loop() override = default;

    // --- Factories (1:1 Loop.create / createArray / createPolygon) ---
    static dqBase::RefPtr<Loop> Create();
    // From curve primitives (assumed to form a closed planar loop). 1:1 Loop.create(...curves).
    static dqBase::RefPtr<Loop> Create(std::vector<CurvePrimitivePtr> const& curves);
    // Alias of Create(curves). 1:1 Loop.createArray.
    static dqBase::RefPtr<Loop> CreateArray(std::vector<CurvePrimitivePtr> const& curves) { return Create(curves); }
    // From coplanar polygon points (closure point optional). 1:1 Loop.createPolygon.
    static dqBase::RefPtr<Loop> CreatePolygon(std::vector<Point3d> const& points);

    // --- CurveChain overrides ---
    CurveCollectionType GetCurveCollectionType() const noexcept override { return CurveCollectionType::Loop; }
    dqBase::RefPtr<CurveChain> CloneEmptyPeer() const override { return Create(); }
    dqBase::RefPtr<CurveChain> CloneStroked(StrokeOptions const& options) const override;

    // MicroStation CurveVector boundary type = Outer (1:1 Loop.dgnBoundaryType).
    int DgnBoundaryType() const noexcept { return 2; }

    // --- GeometryQuery dispatch ---
    void DispatchToHandler(GeometryHandler& handler) override { handler.HandleLoop(*this); }
};

using LoopPtr = dqBase::RefPtr<Loop>;

// Pair of loops with curve geometry (1:1 Loop.ts LoopCurveLoopCurve).
struct LoopCurveLoopCurve {
    dqBase::RefPtr<Loop> loopA;
    CurvePrimitivePtr curveA;
    dqBase::RefPtr<Loop> loopB;
    CurvePrimitivePtr curveB;
};

// Loops characterized by area sign (1:1 Loop.ts SignedLoops interface).
struct SignedLoops {
    std::vector<dqBase::RefPtr<Loop>> positiveAreaLoops; // CCW loops
    std::vector<dqBase::RefPtr<Loop>> negativeAreaLoops; // CW loops
    std::vector<dqBase::RefPtr<Loop>> slivers;           // coincident-section slivers
    std::vector<LoopCurveLoopCurve> edges;               // optional; empty if absent
};

END_DQ_GEOM_NAMESPACE
