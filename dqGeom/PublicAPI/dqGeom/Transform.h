// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Transform
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Transform.ts
//
// A Transform consists of an origin (Point3d) and a Matrix3d.
// The math for a Transform T consisting of a Matrix3d M and a Point3d o on a Vector3d p is: Tp = M*p + o.
#pragma once

#include "Export.h"
#include "Matrix3d.h"
#include "Point3d.h"
#include "Point4d.h"
#include "Range3d.h"
#include "Vector3d.h"

#include <optional>
#include <vector>

namespace dqGeom {

/// Transform = Matrix3d * point + origin.
/// Ported from: itwinjs-core Transform
struct DQ_GEOM_EXPORT Transform {
    Point3d origin;
    Matrix3d matrix;

    /// Construct identity transform.
    Transform()
        : origin(Point3d::FromZero())
        , matrix(Matrix3d::CreateIdentity())
    {}

    /// Construct from origin and matrix.
    Transform(const Point3d& origin_, const Matrix3d& matrix_)
        : origin(origin_)
        , matrix(matrix_)
    {}

    // --- Static factories ---

    /// Create identity transform.
    /// Ported from: itwinjs-core Transform.createIdentity
    static Transform CreateIdentity()
    {
        return Transform(Point3d::FromZero(), Matrix3d::CreateIdentity());
    }

    /// Create translation transform.
    /// Ported from: itwinjs-core Transform.createTranslation
    static Transform CreateTranslation(const Vector3d& translation)
    {
        return Transform(Point3d::From(translation.x, translation.y, translation.z), Matrix3d::CreateIdentity());
    }

    /// Create translation transform.
    static Transform CreateTranslation(double tx, double ty, double tz)
    {
        return Transform(Point3d::From(tx, ty, tz), Matrix3d::CreateIdentity());
    }

    /// Create scale about origin.
    /// Ported from: itwinjs-core Transform.createScaleAboutPoint
    static Transform CreateScaleAboutPoint(const Point3d& point, double scale)
    {
        Matrix3d mat = Matrix3d::CreateUniformScale(scale);
        // origin = point - scale * point = point * (1 - scale)
        Point3d origin = Point3d::From(
            point.x * (1.0 - scale),
            point.y * (1.0 - scale),
            point.z * (1.0 - scale)
        );
        return Transform(origin, mat);
    }

    /// Create rotation about axis through point.
    /// Ported from: itwinjs-core Transform.createFixedPointAndMatrix
    static Transform CreateFixedPointAndMatrix(const Point3d& point, const Matrix3d& matrix)
    {
        // origin = point - matrix * point
        Vector3d pv = Vector3d::From(point.x, point.y, point.z);
        Vector3d mpv = matrix.MultiplyVector(pv);
        Point3d origin = Point3d::From(
            point.x - mpv.x,
            point.y - mpv.y,
            point.z - mpv.z
        );
        return Transform(origin, matrix);
    }

    /// Create rotation about arbitrary axis.
    static Transform CreateRotationAboutAxis(const Point3d& origin_, const Vector3d& axis, double radians)
    {
        Matrix3d rot = Matrix3d::CreateRotationAroundAxis(axis, radians);
        return CreateFixedPointAndMatrix(origin_, rot);
    }

    /// Create a transform from an origin and 3 matrix columns (vectorX/Y/Z).
    /// Ported from: itwinjs-core Transform.createOriginAndMatrixColumns
    static Transform CreateOriginAndMatrixColumns(const Point3d& origin_,
                                                  const Vector3d& vectorX,
                                                  const Vector3d& vectorY,
                                                  const Vector3d& vectorZ)
    {
        // Matrix columns are vectorX/Y/Z; Matrix3d is row-major.
        Matrix3d m(
            vectorX.x, vectorY.x, vectorZ.x,
            vectorX.y, vectorY.y, vectorZ.y,
            vectorX.z, vectorY.z, vectorZ.z);
        return Transform(origin_, m);
    }

    /// Create a transform from an origin and a Matrix3d.
    /// Ported from: itwinjs-core Transform.createOriginAndMatrix (Transform.ts:266-279)
    static Transform CreateOriginAndMatrix(Point3d const& origin, Matrix3d const& matrix)
    {
        return Transform(origin, matrix);
    }

    /// Initialize npcToGlobal / globalToNpc maps between the unit NPC box [0,0,0]-[1,1,1]
    /// and the range [min,max]. Zero-size axes are clamped to 1 (no NaN). Either out-param
    /// may be null to skip that direction.
    /// Ported from: itwinjs-core Transform.initFromRange (Transform.ts:821-855)
    static void InitFromRange(Point3d const& min, Point3d const& max,
                              Transform* npcToGlobal = nullptr,
                              Transform* globalToNpc = nullptr)
    {
        double dx = max.x - min.x, dy = max.y - min.y, dz = max.z - min.z;
        if (dx == 0.0) dx = 1.0;
        if (dy == 0.0) dy = 1.0;
        if (dz == 0.0) dz = 1.0;
        if (npcToGlobal != nullptr) {
            // npcToGlobal * 0 = min; npcToGlobal * 1 = diag + min = max.
            Matrix3d scale(dx, 0, 0,  0, dy, 0,  0, 0, dz);
            *npcToGlobal = Transform(min, scale);
        }
        if (globalToNpc != nullptr) {
            Point3d origin = Point3d::From(-min.x / dx, -min.y / dy, -min.z / dz);
            Matrix3d scale(1.0 / dx, 0, 0,  0, 1.0 / dy, 0,  0, 0, 1.0 / dz);
            *globalToNpc = Transform(origin, scale);
        }
    }

    // --- State queries ---

    /// Set to identity.
    /// Ported from: itwinjs-core Transform.setIdentity
    void SetIdentity()
    {
        origin.Zero();
        matrix.SetIdentity();
    }

    /// Copy from other transform.
    /// Ported from: itwinjs-core Transform.setFrom
    void SetFrom(const Transform& other)
    {
        origin = other.origin;
        matrix = other.matrix;
    }

    /// clone this transform.
    /// Ported from: itwinjs-core Transform.clone
    Transform clone() const { return *this; }

    /// Test approximate equality.
    /// Ported from: itwinjs-core Transform.isAlmostEqual
    bool IsAlmostEqual(const Transform& other, double tol = kSmallMetricDistance) const
    {
        return origin.AlmostEqual(other.origin, tol) && matrix.IsAlmostEqual(other.matrix, tol);
    }

    /// Test if this is identity.
    bool IsIdentity() const
    {
        return IsAlmostEqual(CreateIdentity());
    }

    // --- Access ---

    /// Get origin.
    const Point3d& GetOrigin() const { return origin; }

    /// Get matrix.
    const Matrix3d& GetMatrix() const { return matrix; }

    /// Set origin.
    void SetOrigin(const Point3d& newOrigin) { origin = newOrigin; }

    /// Set matrix.
    void SetMatrix(const Matrix3d& newMatrix) { matrix = newMatrix; }

    // --- Transform operations ---

    /// Transform a point: result = matrix * point + origin.
    /// Ported from: itwinjs-core Transform.multiplyPoint3d
    Point3d MultiplyPoint3d(const Point3d& point) const
    {
        Vector3d pv = Vector3d::From(point.x, point.y, point.z);
        Vector3d result = matrix.MultiplyVector(pv);
        return Point3d::From(
            result.x + origin.x,
            result.y + origin.y,
            result.z + origin.z
        );
    }

    /// Transform a coordinate: result = matrix * (x,y,z) + origin.
    /// Ported from: itwinjs-core Transform.multiplyXYZ
    Point3d MultiplyXYZ(double x, double y, double z) const
    {
        return MultiplyPoint3d(Point3d::From(x, y, z));
    }

    /// Transform a vector (no translation): result = matrix * vector.
    /// Ported from: itwinjs-core Transform.multiplyVector
    Vector3d MultiplyVector(const Vector3d& vector) const
    {
        return matrix.MultiplyVector(vector);
    }

    /// Transform a point by inverse.
    /// Ported from: itwinjs-core Transform.multiplyInversePoint3d
    bool MultiplyInversePoint3d(const Point3d& point, Point3d& result) const
    {
        Matrix3d inv;
        if (!matrix.Inverse(inv))
            return false;

        Vector3d delta = Vector3d::From(
            point.x - origin.x,
            point.y - origin.y,
            point.z - origin.z
        );
        Vector3d r = inv.MultiplyVector(delta);
        result = Point3d::From(r.x, r.y, r.z);
        return true;
    }

    /// Transform a vector by inverse.
    bool MultiplyInverseVector(const Vector3d& vector, Vector3d& result) const
    {
        Matrix3d inv;
        if (!matrix.Inverse(inv))
            return false;
        result = inv.MultiplyVector(vector);
        return true;
    }

    /// Multiply two transforms: result = this * other.
    /// Ported from: itwinjs-core Transform.multiplyTransformTransform
    Transform MultiplyTransform(const Transform& other) const
    {
        Matrix3d newMatrix = matrix.MultiplyMatrix(other.matrix);
        Point3d newOrigin = MultiplyPoint3d(other.origin);
        return Transform(newOrigin, newMatrix);
    }

    /// Compute inverse transform.
    /// Ported from: itwinjs-core Transform.inverse
    bool Inverse(Transform& result) const
    {
        Matrix3d inv;
        if (!matrix.Inverse(inv))
            return false;

        // result.origin = -inv * origin
        Vector3d originV = Vector3d::From(origin.x, origin.y, origin.z);
        Vector3d negInvOrigin = inv.MultiplyVector(originV);
        result.origin = Point3d::From(-negInvOrigin.x, -negInvOrigin.y, -negInvOrigin.z);
        result.matrix = inv;
        return true;
    }

    /// Transform a range.
    /// Ported from: itwinjs-core Range3d.createTransformed
    Range3d MultiplyRange(const Range3d& range) const
    {
        if (range.isNull())
            return Range3d::CreateNull();

        // Transform all 8 corners and take union
        Range3d result;
        for (int i = 0; i < 8; ++i) {
            double fx = (i & 1) ? 1.0 : 0.0;
            double fy = (i & 2) ? 1.0 : 0.0;
            double fz = (i & 4) ? 1.0 : 0.0;
            Point3d corner = range.FractionToPoint(fx, fy, fz);
            result.ExtendPoint(MultiplyPoint3d(corner));
        }
        return result;
    }

    /// Transform a range into `result` (may alias `range` for in-place).
    /// Ported from: itwinjs-core Transform.multiplyRange(range, result).
    void MultiplyRange(const Range3d& range, Range3d& result) const
    {
        result = MultiplyRange(range);
    }

    /// Transform an array of points in place.
    /// Ported from: itwinjs-core Transform.multiplyPoint3dArrayInPlace.
    void MultiplyPoint3dArrayInPlace(std::vector<Point3d>& points) const
    {
        for (auto& p : points)
            p = MultiplyPoint3d(p);
    }

    /// Multiply the homogeneous point by the inverse Transform: the xyz part has
    /// w*origin subtracted, then the matrix inverse is applied to (x,y,z) keeping
    /// the weight w. Returns nullopt if the matrix part is singular.
    /// Ported from: itwinjs-core Transform.multiplyInversePoint4d (Transform.ts:539-548)
    std::optional<Point4d> multiplyInversePoint4d(const Point4d& weightedPoint) const noexcept
    {
        double const w = weightedPoint.w;
        return matrix.multiplyInverseXYZW(
            weightedPoint.x - w * origin.x,
            weightedPoint.y - w * origin.y,
            weightedPoint.z - w * origin.z,
            w
        );
    }
};

} // namespace dqGeom
