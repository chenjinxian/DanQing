// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — CurvePrimitive default implementations
// Ported from: itwinjs-core core/geometry/src/curve/CurvePrimitive.ts
#include "dqGeom/CurvePrimitive.h"
#include "dqGeom/LineSegment3d.h"
#include "dqGeom/LineString3d.h"

BEGIN_DQ_GEOM_NAMESPACE

Range3d CurvePrimitive::Range() const
{
    Range3d range;
    ExtendRange(range);
    return range;
}

void CurvePrimitive::ExtendRange(Range3d& range) const
{
    // Default: sample start and end points
    range.ExtendPoint(StartPoint());
    range.ExtendPoint(EndPoint());
}

bool CurvePrimitive::IsSameGeometryClass(GeometryQuery const& other) const noexcept
{
    return other.Category() == GeometryCategory::CurvePrimitive;
}

bool CurvePrimitive::IsAlmostEqual(GeometryQuery const& other, double tol) const
{
    if (!IsSameGeometryClass(other)) return false;
    auto const& otherCurve = static_cast<CurvePrimitive const&>(other);
    if (GetCurveType() != otherCurve.GetCurveType()) return false;

    // Compare start and end points
    Point3d s0 = StartPoint();
    Point3d e0 = EndPoint();
    Point3d s1 = otherCurve.StartPoint();
    Point3d e1 = otherCurve.EndPoint();
    return s0.AlmostEqual(s1, tol) && e0.AlmostEqual(e1, tol);
}

double CurvePrimitive::CurveLength() const
{
    // Default: stroke-based integration (8 segments)
    double totalLength = 0.0;
    int const numSegments = 8;
    Point3d prev = FractionToPoint(0.0);
    for (int i = 1; i <= numSegments; ++i) {
        double f = static_cast<double>(i) / static_cast<double>(numSegments);
        Point3d curr = FractionToPoint(f);
        totalLength += Vector3d::FromStartEnd(prev, curr).Magnitude();
        prev = curr;
    }
    return totalLength;
}

double CurvePrimitive::CurveLengthBetweenFractions(double f0, double f1) const
{
    double scale = GetFractionToDistanceScale();
    if (scale > 0.0) {
        return std::abs(f1 - f0) * scale;
    }
    // Fallback: stroke-based
    double totalLength = 0.0;
    int const numSegments = 8;
    Point3d prev = FractionToPoint(f0);
    for (int i = 1; i <= numSegments; ++i) {
        double f = f0 + (f1 - f0) * static_cast<double>(i) / static_cast<double>(numSegments);
        Point3d curr = FractionToPoint(f);
        totalLength += Vector3d::FromStartEnd(prev, curr).Magnitude();
        prev = curr;
    }
    return totalLength;
}

double CurvePrimitive::GetFractionToDistanceScale() const
{
    return 0.0;  // Override in subclasses where fraction is proportional to distance
}

dqBase::RefPtr<CurvePrimitive> CurvePrimitive::CreateLine(Point3d const& start, Point3d const& end)
{
    return LineSegment3d::create(start, end);
}

dqBase::RefPtr<CurvePrimitive> CurvePrimitive::CreateLineString(std::vector<Point3d> const& points)
{
    return LineString3d::create(points);
}

// Ported from: itwinjs CurvePrimitive.computeStrokeCountForOptions
int CurvePrimitive::ComputeStrokeCount(StrokeOptions const& options) const
{
    // Default: use 8 strokes
    int count = 8;
    if (options.minStrokesPerPrimitive > 0) {
        count = std::max(count, options.minStrokesPerPrimitive);
    }
    return count;
}

// Ported from: itwinjs CurvePrimitive.emitStrokes
void CurvePrimitive::EmitStrokes(LineString3d& dest, StrokeOptions const& options) const
{
    int numStrokes = ComputeStrokeCount(options);
    for (int i = 0; i <= numStrokes; ++i) {
        double f = static_cast<double>(i) / static_cast<double>(numStrokes);
        dest.AddPoint(FractionToPoint(f));
    }
}

END_DQ_GEOM_NAMESPACE
