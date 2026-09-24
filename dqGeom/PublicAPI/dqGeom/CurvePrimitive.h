// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — CurvePrimitive abstract base
//
// Ported from: itwinjs-core core/geometry/src/curve/CurvePrimitive.ts
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/CurvePrimitive.h
//
// Fraction-based parametric curve (0 = start, 1 = end).  Concrete subclasses
// implement FractionToPoint and related evaluation methods.
// 替代 Qt QVector，参数用 std::vector。
#pragma once

#include "GeometryQuery.h"
#include "GeometryHandler.h"
#include "Point3d.h"
#include "Ray3d.h"
#include "StrokeOptions.h"

#include <functional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

class LineString3d;
class Arc3d;

// ---------------------------------------------------------------------------
// AnnounceNumberNumberCurvePrimitive — 区间 announce 回调
// Ported from: itwinjs-core AnnounceNumberNumberCurvePrimitive (CurvePrimitive.ts)
// §3.4 类型收窄：参考回调携带 CurvePrimitive 基类；当前已移植的生产者只有
// Arc3d（announceClippedArcIntervals 链，BackgroundMapGeometry 深度分支消费），
// 故别名取 Arc3d 形参。随更多曲线类型移植再放宽回基类。
// ---------------------------------------------------------------------------
using AnnounceNumberNumberCurvePrimitive = std::function<void(double, double, Arc3d const&)>;

// ---------------------------------------------------------------------------
// CurveType — type discriminator for curve primitives
// (Ported from: itwinjs-core CurvePrimitiveType)
// ---------------------------------------------------------------------------
enum class CurveType : int {
    LineSegment,
    LineString,
    Arc,
    PointString,
    BSplineCurve,      // Phase 1
    BezierCurve,       // Phase 1
    TransitionSpiral,  // Phase 1
    InterpolationCurve,// Phase 1
    AkimaCurve,        // Phase 1
};

// ---------------------------------------------------------------------------
// CurvePrimitive — abstract parametric curve
// (Ported from: itwinjs-core CurvePrimitive.ts)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT CurvePrimitive : public GeometryQuery {
public:
    ~CurvePrimitive() override = default;

    // --- GeometryQuery overrides ---
    GeometryCategory Category() const noexcept final { return GeometryCategory::CurvePrimitive; }
    Range3d Range() const override;
    void ExtendRange(Range3d& range) const override;
    bool IsSameGeometryClass(GeometryQuery const& other) const noexcept override;
    bool IsAlmostEqual(GeometryQuery const& other, double tol) const override;

    // --- Type discriminator ---
    virtual CurveType GetCurveType() const noexcept = 0;

    // --- Pure virtual evaluation ---
    virtual Point3d FractionToPoint(double fraction) const = 0;
    virtual Point3d FractionToPointAndDerivative(double fraction, Vector3d& derivative) const = 0;

    // --- Virtual with defaults ---
    virtual double QuickLength() const = 0;
    virtual double CurveLength() const;
    virtual double CurveLengthBetweenFractions(double f0, double f1) const;
    virtual double GetFractionToDistanceScale() const;

    virtual Point3d StartPoint() const { return FractionToPoint(0.0); }
    virtual Point3d EndPoint() const { return FractionToPoint(1.0); }

    // --- Tessellation ---
    virtual int ComputeStrokeCount(StrokeOptions const& options) const;
    virtual void EmitStrokes(LineString3d& dest, StrokeOptions const& options = {}) const;

    // --- Static factories ---
    static dqBase::RefPtr<CurvePrimitive> CreateLine(Point3d const& start, Point3d const& end);
    static dqBase::RefPtr<CurvePrimitive> CreateLineString(std::vector<Point3d> const& points);
};

using CurvePrimitivePtr = dqBase::RefPtr<CurvePrimitive>;

END_DQ_GEOM_NAMESPACE
