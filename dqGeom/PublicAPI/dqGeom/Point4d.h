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
};

END_DQ_GEOM_NAMESPACE
