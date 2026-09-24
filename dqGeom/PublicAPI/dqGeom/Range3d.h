// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Range3d
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Range.ts
//   RangeBase, Range3d
//
// Axis-aligned bounding box in 3D.
// Member `low` contains minimum coordinates, `high` contains maximum coordinates.
// The range is null (empty) if any low member is larger than its high counterpart.
#pragma once

#include "Export.h"
#include "Point3d.h"
#include "Vector3d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace dqGeom {

/// Constants for extreme values (impossibly large/small coordinates).
/// Ported from: itwinjs-core RangeBase._EXTREME_POSITIVE/NEGATIVE
inline constexpr double kExtremePositive = 1.0e200;
inline constexpr double kExtremeNegative = -1.0e200;

/// Axis-aligned bounding box in 3D.
/// Ported from: itwinjs-core Range3d
struct DQ_GEOM_EXPORT Range3d {
    Point3d low;
    Point3d high;

    /// Construct a null range (no content).
    Range3d()
        : low(Point3d::From(kExtremePositive, kExtremePositive, kExtremePositive))
        , high(Point3d::From(kExtremeNegative, kExtremeNegative, kExtremeNegative))
    {}

    /// Construct from explicit low/high points.
    Range3d(const Point3d& low_, const Point3d& high_)
        : low(low_)
        , high(high_)
    {}

    /// Construct from min/max coordinates.
    Range3d(double lowX, double lowY, double lowZ, double highX, double highY, double highZ)
        : low(Point3d::From(lowX, lowY, lowZ))
        , high(Point3d::From(highX, highY, highZ))
    {}

    /// create a null range (no content).
    /// Ported from: itwinjs-core Range3d.createNull
    static Range3d CreateNull() { return Range3d(); }

    /// create a single-point range.
    /// Ported from: itwinjs-core Range3d.createXYZ
    static Range3d CreateXYZ(double x, double y, double z)
    {
        return Range3d(x, y, z, x, y, z);
    }

    /// create from two corner points (auto-corrects min/max).
    /// Ported from: itwinjs-core Range3d.createXYZXYZ
    static Range3d CreateXYZXYZ(
        double xA, double yA, double zA,
        double xB, double yB, double zB
    )
    {
        return Range3d(
            std::min(xA, xB), std::min(yA, yB), std::min(zA, zB),
            std::max(xA, xB), std::max(yA, yB), std::max(zA, zB)
        );
    }

    /// create from two corner points. If any direction has order flip, create null.
    /// Ported from: itwinjs-core Range3d.createXYZXYZOrCorrectToNull
    static Range3d CreateXYZXYZOrCorrectToNull(
        double xA, double yA, double zA,
        double xB, double yB, double zB
    )
    {
        if (xA > xB || yA > yB || zA > zB)
            return CreateNull();
        return CreateXYZXYZ(xA, yA, zA, xB, yB, zB);
    }

    /// create a range enclosing multiple points.
    /// Ported from: itwinjs-core Range3d.create
    static Range3d create(std::initializer_list<Point3d> points)
    {
        Range3d result;
        for (const auto& p : points)
            result.ExtendPoint(p);
        return result;
    }

    /// Set this range to null (no content).
    /// Ported from: itwinjs-core Range3d.setNull
    void SetNull()
    {
        low.x = low.y = low.z = kExtremePositive;
        high.x = high.y = high.z = kExtremeNegative;
    }

    /// Test if range has high < low for any axis.
    /// Ported from: itwinjs-core Range3d.isNull
    bool isNull() const
    {
        return high.x < low.x || high.y < low.y || high.z < low.z;
    }

    /// Test if range is a single point.
    /// Ported from: itwinjs-core Range3d.isSinglePoint
    bool IsSinglePoint() const
    {
        return high.x == low.x && high.y == low.y && high.z == low.z;
    }

    /// Copy low and high from other.
    /// Ported from: itwinjs-core Range3d.setFrom
    void SetFrom(const Range3d& other)
    {
        low = other.low;
        high = other.high;
    }

    /// Set range to a single point.
    /// Ported from: itwinjs-core Range3d.setXYZ
    void SetXYZ(double x, double y, double z)
    {
        low.x = high.x = x;
        low.y = high.y = y;
        low.z = high.z = z;
    }

    /// clone this range.
    /// Ported from: itwinjs-core Range3d.clone
    Range3d clone() const { return *this; }

    /// clone with translation.
    /// Ported from: itwinjs-core Range3d.cloneTranslated
    Range3d CloneTranslated(const Vector3d& shift) const
    {
        if (isNull())
            return Range3d();
        return Range3d(
            low.x + shift.x, low.y + shift.y, low.z + shift.z,
            high.x + shift.x, high.y + shift.y, high.z + shift.z
        );
    }

    /// Test if ranges are approximately equal.
    /// Ported from: itwinjs-core Range3d.isAlmostEqual
    bool IsAlmostEqual(const Range3d& other, double tol = kSmallMetricDistance) const
    {
        return (low.AlmostEqual(other.low, tol) && high.AlmostEqual(other.high, tol))
            || (isNull() && other.isNull());
    }

    // --- Queries ---

    /// Return the center point.
    /// Ported from: itwinjs-core Range3d.center
    Point3d Center() const
    {
        return Point3d::FromInterpolate(low, 0.5, high);
    }

    /// Return the 8 corner points of the range.
    /// Ported from: itwinjs-core Range3d.corners (Range.ts:601-619)
    std::array<Point3d, 8> Corners() const noexcept
    {
        return {{
            Point3d::From(low.x, low.y, low.z),
            Point3d::From(high.x, low.y, low.z),
            Point3d::From(low.x, high.y, low.z),
            Point3d::From(high.x, high.y, low.z),
            Point3d::From(low.x, low.y, high.z),
            Point3d::From(high.x, low.y, high.z),
            Point3d::From(low.x, high.y, high.z),
            Point3d::From(high.x, high.y, high.z),
        }};
    }

    /// Return the diagonal vector (high - low).
    /// Ported from: itwinjs-core Range3d.diagonal
    Vector3d Diagonal() const
    {
        return Vector3d::From(high.x - low.x, high.y - low.y, high.z - low.z);
    }

    /// Scale the range about its center by `scale` (in place). No-op if null.
    /// Ported from: itwinjs-core Range3d.scaleAboutCenterInPlace
    void scaleAboutCenterInPlace(double scale)
    {
        if (isNull())
            return;
        Point3d const c = Center();
        // low/high = c + (corner - c) * scale  ==  c*(1-scale) + corner*scale
        low = Point3d::From(c.x * (1.0 - scale) + low.x * scale,
                            c.y * (1.0 - scale) + low.y * scale,
                            c.z * (1.0 - scale) + low.z * scale);
        high = Point3d::From(c.x * (1.0 - scale) + high.x * scale,
                             c.y * (1.0 - scale) + high.y * scale,
                             c.z * (1.0 - scale) + high.z * scale);
    }

    /// Expand the range by `delta` in all directions (low -= delta, high += delta).
    /// Ported from: itwinjs-core Range3d.expandInPlace (Range.ts:984-990)
    void expandInPlace(double delta)
    {
        if (isNull())
            return;
        low.x -= delta;  low.y -= delta;  low.z -= delta;
        high.x += delta; high.y += delta; high.z += delta;
    }

    /// Ensure each axis is at least `minLength` wide, growing symmetrically about center.
    /// Ported from: itwinjs-core Range3d.ensureMinLengths
    void ensureMinLengths(double minLength)
    {
        if (isNull())
            return;
        auto grow = [minLength](double& lo, double& hi) {
            double const delta = hi - lo;
            if (delta < minLength) {
                double const extend = (minLength - delta) * 0.5;
                lo -= extend;
                hi += extend;
            }
        };
        grow(low.x, high.x);
        grow(low.y, high.y);
        grow(low.z, high.z);
    }

    /// Return x length (0 if null).
    /// Ported from: itwinjs-core Range3d.xLength
    double XLength() const
    {
        double a = high.x - low.x;
        return a > 0.0 ? a : 0.0;
    }

    /// Return y length (0 if null).
    /// Ported from: itwinjs-core Range3d.yLength
    double YLength() const
    {
        double a = high.y - low.y;
        return a > 0.0 ? a : 0.0;
    }

    /// Return z length (0 if null).
    /// Ported from: itwinjs-core Range3d.zLength
    double ZLength() const
    {
        double a = high.z - low.z;
        return a > 0.0 ? a : 0.0;
    }

    /// True if the z-length is approximately zero (range is effectively planar in XY).
    /// Ported from: itwinjs-core Range3d.isAlmostZeroZ (= Geometry.isSmallMetricDistance(zLength)).
    bool isAlmostZeroZ() const
    {
        return ZLength() <= kSmallMetricDistance;
    }

    /// Return the largest of x,y,z lengths.
    /// Ported from: itwinjs-core Range3d.maxLength
    double MaxLength() const
    {
        return std::max({XLength(), YLength(), ZLength()});
    }

    /// Return the volume (0 if null).
    double Volume() const
    {
        return XLength() * YLength() * ZLength();
    }

    /// Return the diagonal fraction point.
    /// Ported from: itwinjs-core Range3d.diagonalFractionToPoint
    Point3d DiagonalFractionToPoint(double fraction) const
    {
        return Point3d::FromInterpolate(low, fraction, high);
    }

    /// Return a point by fractional positions on XYZ axes.
    /// Ported from: itwinjs-core Range3d.fractionToPoint
    Point3d FractionToPoint(double fractionX, double fractionY, double fractionZ = 0.0) const
    {
        return Point3d::From(
            low.x + fractionX * (high.x - low.x),
            low.y + fractionY * (high.y - low.y),
            low.z + fractionZ * (high.z - low.z)
        );
    }

    /// Return the largest absolute value among corner coordinates.
    /// Ported from: itwinjs-core Range3d.maxAbs
    double MaxAbs() const
    {
        if (isNull())
            return 0.0;
        return std::max(low.MaxAbs(), high.MaxAbs());
    }

    // --- Containment tests ---

    /// Test if point is within range.
    /// Ported from: itwinjs-core Range3d.containsPoint
    bool ContainsPoint(const Point3d& point) const
    {
        return ContainsXYZ(point.x, point.y, point.z);
    }

    /// Test if x,y coordinates are within range (ignoring z).
    /// Ported from: itwinjs-core Range3d.containsPointXY
    bool ContainsPointXY(const Point3d& point) const
    {
        return point.x >= low.x && point.y >= low.y
            && point.x <= high.x && point.y <= high.y;
    }

    /// Test if x,y,z are within range.
    /// Ported from: itwinjs-core Range3d.containsXYZ
    bool ContainsXYZ(double x, double y, double z) const
    {
        return x >= low.x && y >= low.y && z >= low.z
            && x <= high.x && y <= high.y && z <= high.z;
    }

    /// Test if x,y are within range (ignoring z).
    /// Ported from: itwinjs-core Range3d.containsXY
    bool ContainsXY(double x, double y) const
    {
        return x >= low.x && y >= low.y
            && x <= high.x && y <= high.y;
    }

    /// Test if other range is entirely within this range.
    /// Ported from: itwinjs-core Range3d.containsRange
    bool ContainsRange(const Range3d& other) const
    {
        return other.low.x >= low.x && other.low.y >= low.y && other.low.z >= low.z
            && other.high.x <= high.x && other.high.y <= high.y && other.high.z <= high.z;
    }

    // --- Intersection tests ---

    /// Test if there is any intersection with other range.
    /// Ported from: itwinjs-core Range3d.intersectsRange
    bool IntersectsRange(const Range3d& other, double margin = 0.0) const
    {
        return !(
            low.x > other.high.x + margin
            || low.y > other.high.y + margin
            || low.z > other.high.z + margin
            || other.low.x > high.x + margin
            || other.low.y > high.y + margin
            || other.low.z > high.z + margin
        );
    }

    /// Test if there is any XY intersection with other range (ignoring z).
    /// Ported from: itwinjs-core Range3d.intersectsRangeXY
    bool IntersectsRangeXY(const Range3d& other, double margin = 0.0) const
    {
        return !(
            low.x > other.high.x + margin
            || low.y > other.high.y + margin
            || other.low.x > high.x + margin
            || other.low.y > high.y + margin
        );
    }

    // --- Distance queries ---

    /// Return 0 if point is within range, otherwise distance to closest face/corner.
    /// Ported from: itwinjs-core Range3d.distanceToPoint
    double DistanceToPoint(const Point3d& point) const
    {
        if (isNull())
            return kExtremePositive;
        double dx = CoordinateToRangeAbsoluteDistance(point.x, low.x, high.x);
        double dy = CoordinateToRangeAbsoluteDistance(point.y, low.y, high.y);
        double dz = CoordinateToRangeAbsoluteDistance(point.z, low.z, high.z);
        return std::min(std::sqrt(dx * dx + dy * dy + dz * dz), kExtremePositive);
    }

    /// Return 0 if ranges overlap, otherwise shortest distance between them.
    /// Ported from: itwinjs-core Range3d.distanceToRange
    double DistanceToRange(const Range3d& other) const
    {
        double dx = RangeToRangeAbsoluteDistance(low.x, high.x, other.low.x, other.high.x);
        double dy = RangeToRangeAbsoluteDistance(low.y, high.y, other.low.y, other.high.y);
        double dz = RangeToRangeAbsoluteDistance(low.z, high.z, other.low.z, other.high.z);
        return std::min(std::sqrt(dx * dx + dy * dy + dz * dz), kExtremePositive);
    }

    // --- Extension methods ---

    /// Expand range to include point (x,y,z).
    /// Ported from: itwinjs-core Range3d.extendXYZ
    void ExtendXYZ(double x, double y, double z)
    {
        if (x < low.x) low.x = x;
        if (x > high.x) high.x = x;
        if (y < low.y) low.y = y;
        if (y > high.y) high.y = y;
        if (z < low.z) low.z = z;
        if (z > high.z) high.z = z;
    }

    /// Expand range to include a point.
    /// Ported from: itwinjs-core Range3d.extendPoint
    void ExtendPoint(const Point3d& point)
    {
        ExtendXYZ(point.x, point.y, point.z);
    }

    /// Expand range to include an array of points.
    /// Ported from: itwinjs-core Range3d.extendArray (array, no-transform branch — the form used by
    /// GeometryAccumulator.addLineString/addPointString; the optional-transform + GrowableXYZArray
    /// branches are omitted: DanQing has no GrowableXYZArray and the accumulator applies transform
    /// separately via calculateTransform).
    void extendArray(const std::vector<Point3d>& points)
    {
        for (const auto& p : points)
            ExtendXYZ(p.x, p.y, p.z);
    }

    /// Expand range to include another range.
    /// Ported from: itwinjs-core Range3d.extendRange
    void ExtendRange(const Range3d& other)
    {
        if (!other.isNull()) {
            ExtendXYZ(other.low.x, other.low.y, other.low.z);
            ExtendXYZ(other.high.x, other.high.y, other.high.z);
        }
    }

    /// Expand range by distances in all directions.
    /// Ported from: itwinjs-core Range3d.extendByDistance
    void ExtendByDistance(double dx, double dy, double dz)
    {
        low.x -= dx;  high.x += dx;
        low.y -= dy;  high.y += dy;
        low.z -= dz;  high.z += dz;
    }

    /// Expand range uniformly by distance.
    void ExtendByDistance(double distance)
    {
        ExtendByDistance(distance, distance, distance);
    }

    // --- Set operations ---

    /// Return the intersection of two ranges.
    /// Ported from: itwinjs-core Range3d.intersect
    Range3d Intersect(const Range3d& other) const
    {
        if (!IntersectsRange(other))
            return CreateNull();
        return CreateXYZXYZOrCorrectToNull(
            std::max(low.x, other.low.x), std::max(low.y, other.low.y), std::max(low.z, other.low.z),
            std::min(high.x, other.high.x), std::min(high.y, other.high.y), std::min(high.z, other.high.z)
        );
    }

    /// Return the union of two ranges (smallest range containing both).
    /// Ported from: itwinjs-core Range3d.union
    Range3d Union(const Range3d& other) const
    {
        if (isNull())
            return other;
        if (other.isNull())
            return *this;
        return Range3d(
            std::min(low.x, other.low.x), std::min(low.y, other.low.y), std::min(low.z, other.low.z),
            std::max(high.x, other.high.x), std::max(high.y, other.high.y), std::max(high.z, other.high.z)
        );
    }

    // --- Coordinate helpers ---

    /// Return 0 if x is within [low, high], otherwise distance to nearest endpoint.
    static double CoordinateToRangeAbsoluteDistance(double x, double low, double high)
    {
        if (high < low)
            return kExtremePositive;
        if (x < low)
            return low - x;
        if (x > high)
            return x - high;
        return 0.0;
    }

    /// Return 0 if intervals overlap, otherwise min distance between them.
    static double RangeToRangeAbsoluteDistance(double lowA, double highA, double lowB, double highB)
    {
        if (highA < lowA)
            return kExtremePositive;
        if (highB < lowB)
            return kExtremePositive;
        if (highB < lowA)
            return lowA - highB;
        if (lowB <= highA)
            return 0.0;
        return lowB - highA;
    }
};

/// 2D range (axis-aligned bounding box in 2D).
/// Uses Point3d with z=0 for compatibility.
/// Ported from: itwinjs-core Range2d
struct DQ_GEOM_EXPORT Range2d {
    Point3d low;
    Point3d high;

    Range2d()
        : low(Point3d::From(kExtremePositive, kExtremePositive, 0.0))
        , high(Point3d::From(kExtremeNegative, kExtremeNegative, 0.0))
    {}

    Range2d(const Point3d& low_, const Point3d& high_)
        : low(low_)
        , high(high_)
    {}

    Range2d(double lowX, double lowY, double highX, double highY)
        : low(Point3d::From(lowX, lowY, 0.0))
        , high(Point3d::From(highX, highY, 0.0))
    {}

    static Range2d CreateNull() { return Range2d(); }

    static Range2d CreateXY(double x, double y)
    {
        return Range2d(x, y, x, y);
    }

    static Range2d CreateXYXY(double xA, double yA, double xB, double yB)
    {
        return Range2d(std::min(xA, xB), std::min(yA, yB), std::max(xA, xB), std::max(yA, yB));
    }

    void SetNull()
    {
        low.x = low.y = kExtremePositive;
        high.x = high.y = kExtremeNegative;
    }

    bool isNull() const { return high.x < low.x || high.y < low.y; }

    void SetFrom(const Range2d& other)
    {
        low = other.low;
        high = other.high;
    }

    Range2d clone() const { return *this; }

    bool IsAlmostEqual(const Range2d& other, double tol = kSmallMetricDistance) const
    {
        return (low.AlmostEqual(other.low, tol) && high.AlmostEqual(other.high, tol))
            || (isNull() && other.isNull());
    }

    Point3d Center() const { return Point3d::FromInterpolate(low, 0.5, high); }

    double XLength() const
    {
        double a = high.x - low.x;
        return a > 0.0 ? a : 0.0;
    }

    double YLength() const
    {
        double a = high.y - low.y;
        return a > 0.0 ? a : 0.0;
    }

    double Area() const { return XLength() * YLength(); }

    bool ContainsXY(double x, double y) const
    {
        return x >= low.x && y >= low.y && x <= high.x && y <= high.y;
    }

    bool ContainsPoint(const Point3d& point) const
    {
        return ContainsXY(point.x, point.y);
    }

    bool ContainsRange(const Range2d& other) const
    {
        return other.low.x >= low.x && other.low.y >= low.y
            && other.high.x <= high.x && other.high.y <= high.y;
    }

    bool IntersectsRange(const Range2d& other) const
    {
        return !(
            low.x > other.high.x
            || low.y > other.high.y
            || other.low.x > high.x
            || other.low.y > high.y
        );
    }

    void ExtendXY(double x, double y)
    {
        if (x < low.x) low.x = x;
        if (x > high.x) high.x = x;
        if (y < low.y) low.y = y;
        if (y > high.y) high.y = y;
    }

    void ExtendPoint(const Point3d& point)
    {
        ExtendXY(point.x, point.y);
    }

    void ExtendRange(const Range2d& other)
    {
        if (!other.isNull()) {
            ExtendXY(other.low.x, other.low.y);
            ExtendXY(other.high.x, other.high.y);
        }
    }

    Range2d Intersect(const Range2d& other) const
    {
        if (!IntersectsRange(other))
            return CreateNull();
        return Range2d(
            std::max(low.x, other.low.x), std::max(low.y, other.low.y),
            std::min(high.x, other.high.x), std::min(high.y, other.high.y)
        );
    }

    Range2d Union(const Range2d& other) const
    {
        if (isNull())
            return other;
        if (other.isNull())
            return *this;
        return Range2d(
            std::min(low.x, other.low.x), std::min(low.y, other.low.y),
            std::max(high.x, other.high.x), std::max(high.y, other.high.y)
        );
    }
};

/// 1D range (interval).
/// Ported from: itwinjs-core Range1d
struct DQ_GEOM_EXPORT Range1d {
    double low;
    double high;

    Range1d()
        : low(kExtremePositive)
        , high(kExtremeNegative)
    {}

    Range1d(double low_, double high_)
        : low(low_)
        , high(high_)
    {}

    static Range1d CreateNull() { return Range1d(); }

    static Range1d create(double value)
    {
        return Range1d(value, value);
    }

    static Range1d CreateXX(double a, double b)
    {
        return Range1d(std::min(a, b), std::max(a, b));
    }

    void SetNull()
    {
        low = kExtremePositive;
        high = kExtremeNegative;
    }

    bool isNull() const { return high < low; }

    void SetFrom(const Range1d& other)
    {
        low = other.low;
        high = other.high;
    }

    Range1d clone() const { return *this; }

    bool IsAlmostEqual(const Range1d& other, double tol = kSmallMetricDistance) const
    {
        return (std::abs(low - other.low) <= tol && std::abs(high - other.high) <= tol)
            || (isNull() && other.isNull());
    }

    double Center() const { return 0.5 * (low + high); }

    double Length() const
    {
        double a = high - low;
        return a > 0.0 ? a : 0.0;
    }

    bool Contains(double value) const
    {
        return value >= low && value <= high;
    }

    bool ContainsRange(const Range1d& other) const
    {
        return other.low >= low && other.high <= high;
    }

    bool IntersectsRange(const Range1d& other) const
    {
        return !(low > other.high || other.low > high);
    }

    void Extend(double value)
    {
        if (value < low) low = value;
        if (value > high) high = value;
    }

    void ExtendRange(const Range1d& other)
    {
        if (!other.isNull()) {
            Extend(other.low);
            Extend(other.high);
        }
    }

    Range1d Intersect(const Range1d& other) const
    {
        if (!IntersectsRange(other))
            return CreateNull();
        return Range1d(std::max(low, other.low), std::min(high, other.high));
    }

    Range1d Union(const Range1d& other) const
    {
        if (isNull())
            return other;
        if (other.isNull())
            return *this;
        return Range1d(std::min(low, other.low), std::max(high, other.high));
    }

    double Fraction(double value) const
    {
        if (isNull())
            return 0.0;
        double len = high - low;
        if (len <= 0.0)
            return 0.0;
        return (value - low) / len;
    }

    double FractionToPoint(double fraction) const
    {
        return low + fraction * (high - low);
    }
};

} // namespace dqGeom
