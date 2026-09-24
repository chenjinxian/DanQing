// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Arc3d
//
// Ported from: itwinjs-core core/geometry/src/curve/Arc3d.ts
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dellipse3d.h
//
// Circular / elliptic arc curve primitive.  Storage: center + Matrix3d
// (columns = vector0, vector90, unitNormal) + AngleSweep.
//
// Parameterization: X(theta) = center + cos(theta)*vector0 + sin(theta)*vector90
//                   where theta = sweep.FractionToRadians(fraction)
//
// When vector0 ⟂ vector90 and |vector0| = |vector90|, the arc is circular.
#pragma once

#include "AngleSweep.h"
#include "CurvePrimitive.h"
#include "Matrix3d.h"

BEGIN_DQ_GEOM_NAMESPACE

class ConvexClipPlaneSet;

// ---------------------------------------------------------------------------
// Arc3d — circular / elliptic arc
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT Arc3d : public CurvePrimitive {
public:
    // --- Factories ---
    // Ported from: itwinjs Arc3d.create
    static dqBase::RefPtr<Arc3d> create(Point3d const& center, Matrix3d const& axes,
                                        AngleSweep const& sweep);

    // Ported from: itwinjs Arc3d.createXY
    static dqBase::RefPtr<Arc3d> CreateXY(Point3d const& center, double radius,
                                          AngleSweep const& sweep = AngleSweep::FullCircle());

    // Ported from: itwinjs Arc3d.createCenterNormalRadius
    static dqBase::RefPtr<Arc3d> CreateCenterNormalRadius(Point3d const& center,
                                                          Vector3d const& normal, double radius,
                                                          AngleSweep const& sweep = AngleSweep::FullCircle());

    // Ported from: imodel-native DEllipse3d::FromVectors
    static dqBase::RefPtr<Arc3d> FromVectors(Point3d const& center, Vector3d const& vector0,
                                             Vector3d const& vector90,
                                             AngleSweep const& sweep);

    // --- CurvePrimitive interface ---
    CurveType GetCurveType() const noexcept final { return CurveType::Arc; }
    void DispatchToHandler(GeometryHandler& handler) final { handler.HandleArc3d(*this); }

    Point3d FractionToPoint(double fraction) const final;
    Point3d FractionToPointAndDerivative(double fraction, Vector3d& derivative) const final;

    double QuickLength() const final;
    double GetFractionToDistanceScale() const final;
    int ComputeStrokeCount(StrokeOptions const& options) const final;

    dqBase::RefPtr<GeometryQuery> clone() const final;
    dqBase::RefPtr<GeometryQuery> CloneTransformed(Transform const& transform) const final;
    bool TryTransformInPlace(Transform const& transform) final;

    void ExtendRange(Range3d& range) const final;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const final;

    // --- Accessors ---
    Point3d const& CenterRef() const noexcept { return m_center; }
    Matrix3d const& AxesRef() const noexcept { return m_matrix; }
    AngleSweep const& Sweep() const noexcept { return m_sweep; }

    Vector3d Vector0() const;
    Vector3d Vector90() const;
    Vector3d PerpendicularVector() const;

    // --- Queries ---
    bool IsCircular() const;
    double CircularRadius() const;

    // --- itwinjs Arc3d.ts additions（camelCase 1:1；截面弧区间裁剪链） ---

    /// Find intervals of this arc that are interior to the clipper (dispatches to
    /// clipper.announceClippedArcIntervals).
    /// Ported from: itwinjs-core Arc3d.announceClipIntervals (Arc3d.ts:1418-1420)
    bool announceClipIntervals(ConvexClipPlaneSet const& clipper,
                               AnnounceNumberNumberCurvePrimitive const& announce) const;

    /// Return (if possible) an arc which is a portion of this curve.
    /// Ported from: itwinjs-core Arc3d.clonePartialCurve (Arc3d.ts:1346-1357)
    dqBase::RefPtr<Arc3d> clonePartialCurve(double fractionA, double fractionB) const;

    /// Reverse the sweep in place.
    /// Ported from: itwinjs-core Arc3d.reverseInPlace (Arc3d.ts:1001-1003)
    void reverseInPlace() { m_sweep.ReverseInPlace(); }

    /// Extend a range to include the range of this arc (optionally transformed).
    /// Ported from: itwinjs-core Arc3d.extendRange (Arc3d.ts:1100-1102)
    void extendRange(Range3d& range, Transform const* transform = nullptr) const {
        extendRangeInSweep(range, m_sweep, transform);
    }

    /// Extend a range to include the range of the arc, using specified sweep in
    /// place of the arc sweep. Ported from: itwinjs-core Arc3d.extendRangeInSweep
    /// (Arc3d.ts:1108-1131)
    void extendRangeInSweep(Range3d& range, AngleSweep const& sweep,
                            Transform const* transform = nullptr) const;

private:
    Arc3d() = default;
    Arc3d(Point3d const& center, Matrix3d const& axes, AngleSweep const& sweep);

    Point3d m_center;
    Matrix3d m_matrix;    // columns = [vector0, vector90, unitNormal]
    AngleSweep m_sweep;
};

using Arc3dPtr = dqBase::RefPtr<Arc3d>;

END_DQ_GEOM_NAMESPACE
