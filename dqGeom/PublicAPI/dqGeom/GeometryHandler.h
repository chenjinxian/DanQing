// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — GeometryHandler visitor interface
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/GeometryHandler.ts
//
// Double-dispatch visitor for geometry types.  Each concrete geometry class
// calls the appropriate Handle* method via DispatchToHandler().
// Default implementations are empty — override only the types you care about.
//
// CLAUDE.md §7.2: no RTTI; this is the dispatch mechanism.
#pragma once

#include "Export.h"
#include "Plane3dByOriginAndVectors.h"   // UVSurface::UVFractionToPointAndTangents 返回类型
#include "Point2d.h"                     // UVSurfaceIsoParametricDistance::MaxIsoParametricDistance 返回类型
#include "Point3d.h"                     // UVSurface::UVFractionToPoint 返回类型
#include "DqGeom.h"   // BEGIN_DQ_GEOM_NAMESPACE macro (self-contained: do not rely on include order)

BEGIN_DQ_GEOM_NAMESPACE

// Forward declarations (concrete curve types)
class LineSegment3d;
class LineString3d;
class Arc3d;
class PointString3d;
class Path;
class Loop;
class ParityRegion;
class UnionRegion;
class BagOfCurves;
class IndexedPolyface;
class SolidPrimitive;
class Box;
class Cone;
class Sphere;
class TorusPipe;
class LinearSweep;
class RotationalSweep;
class RuledSweep;

// ---------------------------------------------------------------------------
// GeometryHandler — visitor interface
// (Ported from: itwinjs-core GeometryHandler.ts)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT GeometryHandler {
public:
    virtual ~GeometryHandler() = default;

    // --- Curve primitives (Phase 0) ---
    virtual void HandleLineSegment3d(LineSegment3d&) {}
    virtual void HandleLineString3d(LineString3d&) {}
    virtual void HandleArc3d(Arc3d&) {}
    virtual void HandlePointString3d(PointString3d&) {}

    // --- Curve collections (Phase 1) ---
    virtual void HandlePath(Path&) {}
    virtual void HandleLoop(Loop&) {}
    virtual void HandleParityRegion(ParityRegion&) {}
    virtual void HandleUnionRegion(UnionRegion&) {}
    virtual void HandleBagOfCurves(BagOfCurves&) {}

    // --- Polyface (1:1 GeometryHandler.ts:75 handleIndexedPolyface) ---
    virtual void HandleIndexedPolyface(IndexedPolyface&) {}

    // --- Solids (1:1 GeometryHandler.ts handleSolidPrimitive / handleBox / handleCone / handleSphere) ---
    virtual void HandleSolidPrimitive(SolidPrimitive&) {}
    virtual void HandleBox(Box&) {}
    virtual void HandleCone(Cone&) {}
    virtual void HandleSphere(Sphere&) {}
    virtual void HandleTorusPipe(TorusPipe&) {}
    virtual void HandleLinearSweep(LinearSweep&) {}
    virtual void HandleRotationalSweep(RotationalSweep&) {}
    virtual void HandleRuledSweep(RuledSweep&) {}

    // Phase 1: HandleBSplineCurve3d, HandleBezierCurve3d, HandleTransitionSpiral3d
    // PR 3 commit 3+: HandleTorusPipe/HandleLinearSweep/HandleRotationalSweep/HandleRuledSweep
};

// ---------------------------------------------------------------------------
// UVSurface — (u,v) 分数坐标 → 曲面点 + 切向
// Ported from: itwinjs-core core/geometry/src/geometry3d/GeometryHandler.ts:509-528
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT UVSurface {
public:
    virtual ~UVSurface() = default;
    /// Convert fractional u and v coordinates to surface point.
    virtual Point3d UVFractionToPoint(double uFraction, double vFraction) const = 0;
    /// Convert fractional u and v coordinates to surface point and in-surface tangent
    /// directions. The vectors are non-zero tangents which can be crossed to get a
    /// normal (not necessarily partial derivatives or Frenet vectors).
    virtual Plane3dByOriginAndVectors UVFractionToPointAndTangents(double uFraction, double vFraction) const = 0;
};

// ---------------------------------------------------------------------------
// UVSurfaceIsoParametricDistance — u/v 等参线最大弧长查询
// Ported from: itwinjs-core GeometryHandler.ts:533-541
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT UVSurfaceIsoParametricDistance {
public:
    virtual ~UVSurfaceIsoParametricDistance() = default;
    /// Return a vector whose x and y parts are "size" of the surface in the u and v
    /// directions (approximate max curve length along u and v isoparameter lines —
    /// e.g. sphere: u = distance around the equator, v = south pole to north pole).
    virtual Point2d MaxIsoParametricDistance() const = 0;
};

END_DQ_GEOM_NAMESPACE
