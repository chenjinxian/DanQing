// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Segment3d value type
//
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dsegment3d.h
//              itwinjs-core core/geometry/src/geometry3d/Segment3d.ts
//
// Line segment data carrier (point0 → point1). Used by LineSegment3d.
#pragma once

#include "Point3d.h"
#include "Vector3d.h"

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Segment3d — line segment (two endpoints)
// ---------------------------------------------------------------------------
struct DQ_GEOM_EXPORT Segment3d {
    Point3d point0;
    Point3d point1;

    // --- Factories ---
    static Segment3d From(Point3d const& p0, Point3d const& p1) noexcept
    {
        return Segment3d{p0, p1};
    }

    static Segment3d From(double x0, double y0, double z0,
                          double x1, double y1, double z1) noexcept
    {
        return Segment3d{Point3d::From(x0, y0, z0), Point3d::From(x1, y1, z1)};
    }

    // Ported from: imodel-native DSegment3d::FromFractionInterval
    static Segment3d FromFractionInterval(Segment3d const& parent,
                                          double startFraction, double endFraction) noexcept
    {
        return Segment3d{
            parent.FractionToPoint(startFraction),
            parent.FractionToPoint(endFraction)
        };
    }

    // --- Evaluation ---
    // Ported from: imodel-native DSegment3d::FractionToPoint
    Point3d FractionToPoint(double fraction) const noexcept
    {
        return Point3d::FromInterpolate(point0, fraction, point1);
    }

    // --- Length ---
    double Length() const noexcept
    {
        return Vector3d::FromStartEnd(point0, point1).Magnitude();
    }

    double LengthSquared() const noexcept
    {
        return Vector3d::FromStartEnd(point0, point1).MagnitudeSquared();
    }

    // --- Projection ---
    // Ported from: imodel-native DSegment3d::ProjectPointBounded
    double ProjectionFraction(Point3d const& spacePoint) const noexcept
    {
        Vector3d v = Vector3d::FromStartEnd(point0, point1);
        double dd = v.MagnitudeSquared();
        if (dd < kSmallMetricDistanceSquared) return 0.0;
        Vector3d originToSpace = Vector3d::FromStartEnd(point0, spacePoint);
        return originToSpace.DotProduct(v) / dd;
    }

    bool TryProjectToSegment(Point3d const& spacePoint, double& fraction,
                             Point3d& closest) const noexcept
    {
        fraction = ProjectionFraction(spacePoint);
        // Clamp to [0, 1] for bounded projection
        if (fraction < 0.0) fraction = 0.0;
        if (fraction > 1.0) fraction = 1.0;
        closest = FractionToPoint(fraction);
        return true;
    }
};

END_DQ_GEOM_NAMESPACE
