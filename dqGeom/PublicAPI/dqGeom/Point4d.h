// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — 4D point (homogeneous coordinates)
// Ported from: itwinjs-core core/geometry/src/geometry4d/Point4d.ts
//
// 单向 include（本头 → Point3d/Vector3d/Geometry）；跨类型方法
// toPlane3dByOriginAndUnitNormal 在此声明、在 Plane3dByOriginAndUnitNormal.h
// 内联定义（对齐 Point3d↔Vector3d 模式，避免 Point4d→plane→Transform→
// Matrix3d→Point4d 循环包含）。
#pragma once

#include "Geometry.h"
#include "Point3d.h"
#include "Vector3d.h"

#include <cmath>
#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

struct Plane3dByOriginAndUnitNormal;

// ---------------------------------------------------------------------------
// Point4d — homogeneous 4D point
// Ported from: itwinjs-core Point4d.ts
// ---------------------------------------------------------------------------
class Point4d {
public:
    double x = 0.0, y = 0.0, z = 0.0, w = 0.0;

    Point4d() noexcept = default;
    Point4d(double xx, double yy, double zz, double ww) noexcept
        : x(xx), y(yy), z(zz), w(ww) {}

    static Point4d From(double x, double y, double z, double w) noexcept {
        return Point4d(x, y, z, w);
    }

    /// Ported from: itwinjs-core Point4d.create (Point4d.ts factory)
    static Point4d create(double x, double y, double z, double w) noexcept {
        return Point4d(x, y, z, w);
    }

    static Point4d CreateZero() noexcept { return Point4d(); }

    void Set(double xx, double yy, double zz, double ww) noexcept {
        x = xx; y = yy; z = zz; w = ww;
    }

    /// Normalize to w=1 (if w != 0).
    Point4d NormalizedXYZ() const noexcept {
        if (std::abs(w) > 1.0e-15) {
            double invW = 1.0 / w;
            return Point4d(x * invW, y * invW, z * invW, 1.0);
        }
        return *this;
    }

    // --- itwinjs Point4d.ts additions (camelCase, 1:1) ---

    /// Create a Point4d with weight w and xyz from the given point.
    /// Ported from: Point4d.createFromPointAndWeight (Point4d.ts:262-264)
    static Point4d createFromPointAndWeight(Point3d const& xyz, double w) noexcept {
        return Point4d(xyz.x, xyz.y, xyz.z, w);
    }

    /// Altitude of `point` above this (homogeneous) plane: x*px + y*py + z*pz + w.
    /// Ported from: Point4d.altitude (Point4d.ts:336-338)
    double altitude(Point3d const& point) const noexcept {
        return x * point.x + y * point.y + z * point.z + w;
    }

    /// Dot product with (vector.x, vector.y, vector.z, 0).
    /// Ported from: Point4d.velocity (Point4d.ts:348-350)
    double velocity(Vector3d const& vector) const noexcept {
        return x * vector.x + y * vector.y + z * vector.z;
    }

    /// Return (this.xyz - this.w * other) as a Vector3d.
    /// Ported from: Point4d.crossWeightedMinusPoint3d (Point4d.ts:210-213)
    Vector3d crossWeightedMinusPoint3d(Point3d const& other) const noexcept {
        return Vector3d::From(x - w * other.x, y - w * other.y, z - w * other.z);
    }

    /// Project `spacePoint` onto this (homogeneous) plane.
    /// Ported from: Point4d.projectXYZToPlane (Point4d.ts:388-399) applied to the
    /// point's coordinates. On divide failure the unprojected point is returned.
    Point3d projectPointToPlane(Point3d const& spacePoint) const noexcept {
        double const h = altitude(spacePoint);
        double const nn = x * x + y * y + z * z;
        // this unusual tol is needed so that toPlane3dByOriginAndUnitNormal agrees
        // with its original implementation (Point4d.ts:394).
        auto const alpha = conditionalDivideCoordinate(-h, nn, 1.0e10 * 1.0e10);
        if (!alpha.has_value())
            return spacePoint;
        return Point3d::From(spacePoint.x + (*alpha) * x,
                             spacePoint.y + (*alpha) * y,
                             spacePoint.z + (*alpha) * z);
    }

    /// Convert this homogeneous plane (xyz = normal, w = -distance scale) to a
    /// Plane3dByOriginAndUnitNormal. Returns nullopt if the normal is (near) zero.
    /// Ported from: Point4d.toPlane3dByOriginAndUnitNormal (Point4d.ts:560-562) —
    /// defined inline at the bottom of Plane3dByOriginAndUnitNormal.h.
    std::optional<Plane3dByOriginAndUnitNormal> toPlane3dByOriginAndUnitNormal() const noexcept;

    // ── 四元数插值原语（RenderSchedule 动画 transform 插值消费面——
    //    RenderSchedule.ts getAnimationTransform :663-696 经
    //    Point4d.interpolateQuaternions + Matrix3d.createFromQuaternion） ──

    /// Component-wise sum, *this + other.
    /// Ported from: itwinjs-core Point4d.plus (Point4d.ts:215-217)
    Point4d plus(Point4d const& other) const noexcept {
        return Point4d(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    /// *this + other * scale.
    /// Ported from: itwinjs-core Point4d.plusScaled (Point4d.ts:323-325)
    Point4d plusScaled(Point4d const& other, double scale) const noexcept {
        return Point4d(x + other.x * scale, y + other.y * scale,
                       z + other.z * scale, w + other.w * scale);
    }

    /// vectorA * scalarA + vectorB * scalarB.
    /// Ported from: itwinjs-core Point4d.createAdd2Scaled (Point4d.ts:313-315)
    static Point4d createAdd2Scaled(Point4d const& vectorA, double scalarA,
                                    Point4d const& vectorB, double scalarB) noexcept {
        return Point4d(vectorA.x * scalarA + vectorB.x * scalarB,
                       vectorA.y * scalarA + vectorB.y * scalarB,
                       vectorA.z * scalarA + vectorB.z * scalarB,
                       vectorA.w * scalarA + vectorB.w * scalarB);
    }

    /// 4D dot product.
    /// Ported from: itwinjs-core Point4d.dotProduct (Point4d.ts:328-330)
    double dotProduct(Point4d const& other) const noexcept {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    /// Component-wise negation.
    /// Ported from: itwinjs-core Point4d.negate (Point4d.ts:410-417)
    Point4d negate() const noexcept { return Point4d(-x, -y, -z, -w); }

    /// Component-wise linear interpolation: this + fraction * (other - this).
    /// Ported from: itwinjs-core Point4d.interpolate (Point4d.ts:300-303)
    Point4d interpolate(double fraction, Point4d const& other) const noexcept {
        return Point4d(x + fraction * (other.x - x),
                       y + fraction * (other.y - y),
                       z + fraction * (other.z - z),
                       w + fraction * (other.w - w));
    }

    /// (Toleranced) equality over all 4 components.
    /// Ported from: itwinjs-core Point4d.isAlmostEqual (Point4d.ts:125-130)
    bool isAlmostEqual(Point4d const& other,
                       double tolerance = kSmallMetricDistance) const noexcept {
        return std::abs(x - other.x) <= tolerance && std::abs(y - other.y) <= tolerance &&
               std::abs(z - other.z) <= tolerance && std::abs(w - other.w) <= tolerance;
    }

    /// Normalize in place, regarding (x,y,z,w) as a quaternion. Returns the
    /// magnitude before normalization.
    /// Ported from: itwinjs-core Point4d.normalizeQuaternion (Point4d.ts:564-575)
    double normalizeQuaternion() noexcept {
        double const magnitude = std::sqrt(x * x + y * y + z * z + w * w);
        if (magnitude > 0.0) {
            double const f = 1.0 / magnitude;
            x *= f; y *= f; z *= f; w *= f;
        }
        return magnitude;
    }

    /// Return a (normalized) quaternion interpolated between two quaternions.
    /// Ported from: itwinjs-core Point4d.interpolateQuaternions (Point4d.ts:577-629)
    static Point4d interpolateQuaternions(Point4d const& quaternion0,
                                          double fractionParameter,
                                          Point4d const& quaternion1) noexcept {
        double constexpr maxSafeCosine = 0.9995;

        // return exact quaternions for special values
        if (0.0 == fractionParameter)
            return quaternion0;
        if (1.0 == fractionParameter)
            return quaternion1;
        if (0.5 == fractionParameter) {
            Point4d result = quaternion0.plus(quaternion1);
            result.normalizeQuaternion();
            return result;
        }

        Point4d q0 = quaternion0;
        Point4d q1 = quaternion1;
        double dot = quaternion0.dotProduct(quaternion1);

        // prevent interpolation through the longer great arc
        if (dot < 0.0) {
            q1 = q1.negate();
            dot = -dot;
        }

        // if nearly parallel, use interpolate and renormalize .
        if (dot > maxSafeCosine) {
            Point4d result = q0.interpolate(fractionParameter, q1);
            result.normalizeQuaternion();
            return result;
        }

        // safety check
        if (dot < -1.0)
            dot = -1.0;
        else if (dot > 1.0)
            dot = 1.0;

        // create orthonormal basis {q0, q2}
        Point4d q2 = q1.plusScaled(q0, -dot);  // bsiDPoint4d_addScaledDPoint4d(&q2, &q1, &q0, -dot);
        q2.normalizeQuaternion();

        double const angle = std::acos(dot);
        double const angleOfInterpolation = angle * fractionParameter;
        return createAdd2Scaled(q0, std::cos(angleOfInterpolation),
                                q2, std::sin(angleOfInterpolation));
    }
};

END_DQ_GEOM_NAMESPACE
