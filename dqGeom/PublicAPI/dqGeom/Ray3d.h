// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Ray3d value type
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Ray3d.ts
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dray3d.h
//
// A ray with origin and direction. Direction is caller-normalized.
#pragma once

#include "Plane3dByOriginAndUnitNormal.h"
#include "Point3d.h"
#include "Vector3d.h"

#include <cmath>
#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Ray3d — ray (origin + direction)
// ---------------------------------------------------------------------------
struct DQ_GEOM_EXPORT Ray3d {
    Point3d origin;
    Vector3d direction;

    // --- Factories ---
    static Ray3d FromOriginAndDirection(Point3d const& o, Vector3d const& d) noexcept
    {
        return Ray3d{o, d};
    }

    static Ray3d FromStartEnd(Point3d const& start, Point3d const& end) noexcept
    {
        return Ray3d{start, Vector3d::FromStartEnd(start, end)};
    }

    /// Ported from: itwinjs-core Ray3d.create (Ray3d.ts factory, capture semantics —
    /// DanQing Ray3d is a value type, so "capture" is a copy of the two members).
    static Ray3d create(Point3d const& origin, Vector3d const& direction) noexcept
    {
        return Ray3d{origin, direction};
    }

    /// Ported from: itwinjs-core Ray3d.createXYZUVW (Ray3d.ts:124-127)
    static Ray3d createXYZUVW(double ax, double ay, double az,
                              double ux, double uy, double uz) noexcept
    {
        return Ray3d{Point3d::From(ax, ay, az), Vector3d::From(ux, uy, uz)};
    }

    /// Ported from: itwinjs-core Ray3d.createZAxis (Ray3d.ts:116-118)
    static Ray3d createZAxis() noexcept
    {
        return Ray3d{Point3d::FromZero(), Vector3d::UnitZ()};
    }

    /// Deep copy.
    /// Ported from: itwinjs-core Ray3d.clone
    Ray3d clone() const noexcept { return *this; }

    /// Return a ray with origin and direction transformed by the INVERSE of
    /// `transform`. Returns nullopt if the transform matrix is singular.
    /// Ported from: itwinjs-core Ray3d.cloneInverseTransformed (Ray3d.ts:235-244)
    std::optional<Ray3d> cloneInverseTransformed(Transform const& transform) const noexcept
    {
        Point3d newOrigin;
        if (!transform.MultiplyInversePoint3d(origin, newOrigin))
            return std::nullopt;
        auto newDirection = transform.matrix.multiplyInverse(direction);
        if (!newDirection.has_value())
            return std::nullopt;
        return Ray3d{newOrigin, *newDirection};
    }

    // --- Evaluation ---
    // Ported from: imodel-native DRay3d::FractionToPoint
    Point3d FractionToPoint(double fraction) const noexcept
    {
        return Point3d::From(origin.x + fraction * direction.x,
                             origin.y + fraction * direction.y,
                             origin.z + fraction * direction.z);
    }

    // --- Projection ---
    // Ported from: imodel-native DRay3d::ProjectPoint
    bool TryProjectToRay(Point3d const& spacePoint, double& fraction,
                         Point3d& closest) const noexcept
    {
        double dd = direction.MagnitudeSquared();
        if (dd < kSmallMetricDistanceSquared) {
            fraction = 0.0;
            closest = origin;
            return false;
        }
        Vector3d originToSpace = Vector3d::FromStartEnd(origin, spacePoint);
        fraction = originToSpace.DotProduct(direction) / dd;
        closest = FractionToPoint(fraction);
        return true;
    }

    // --- Plane intersection ---
    // Ported from: itwinjs-core Ray3d.intersectionWithPlane (Ray3d.ts:347-362).
    // Returns the parametric fraction along the ray at which it crosses `plane`,
    // writing the world-space intersection point to *outPoint (if non-null).
    // Returns nullopt if the ray is (near-)parallel to the plane.
    //
    // 参考的两道 conditionalDivideFraction（Geometry.ts:1121-1127）——任一超出
    // largeFractionResult(=1e10，Geometry.ts:279) 上限即 undefined：
    //   division  = conditionalDivideFraction(-aDotN, uDotN)   // 交点参数
    //   division1 = conditionalDivideFraction(nDotN, uDotN)    // 参考同样要求有界
    // 近平行光线（uDotN→0）的交点参数会冲到 1e12+，必须按参考挡回 undefined
    // （否则 BackgroundMapGeometry 的深度范围被污染到百万级——实例：Iso→Left 后
    // 后视锥深度 2.8M、天空伪相机退化、天空变平光）。
    std::optional<double> intersectionWithPlane(Plane3dByOriginAndUnitNormal const& plane,
                                                Point3d* outPoint = nullptr) const noexcept
    {
        // conditionalDivideFraction (Geometry.ts:1121-1127)：
        //   denominator == 0 → undefined；
        //   |denominator| * 1e10 >= |numerator| → numerator/denominator，否则 undefined。
        auto conditionalDivideFraction = [](double numerator, double denominator)
            -> std::optional<double>
        {
            if (denominator == 0.0)
                return std::nullopt;
            // Geometry.largeFractionResult (:279) — Geometry.h 全局常量
            if (std::abs(denominator) * kLargeFractionResult >= std::abs(numerator))
                return numerator / denominator;
            return std::nullopt;
        };

        Vector3d const vectorA = Vector3d::FromStartEnd(plane.getOriginRef(), origin);
        double const uDotN = direction.DotProduct(plane.getNormalRef());
        double const nDotN = direction.MagnitudeSquared();
        double const aDotN = vectorA.DotProduct(plane.getNormalRef());
        auto const division = conditionalDivideFraction(-aDotN, uDotN);   // :352
        if (!division.has_value())                                        // :353-354
            return std::nullopt;
        auto const division1 = conditionalDivideFraction(nDotN, uDotN);   // :355
        if (!division1.has_value())                                       // :356-357
            return std::nullopt;
        if (outPoint != nullptr)
            *outPoint = FractionToPoint(*division);                       // :359 — origin + division * direction
        return division;                                                  // :361
    }
};

END_DQ_GEOM_NAMESPACE
