// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Geographic coordinates (WGS84)
//
// Ported from: itwinjs-core core/common/src/geometry/Cartographic.ts
// A position on the earth defined by longitude, latitude, and height above the WGS84 ellipsoid.
#pragma once

#include "DqCommon.h"

#include <dqGeom/Point3d.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// JSON representation of a Cartographic object.
// Ported from: itwinjs-core CartographicProps
struct CartographicProps {
    double longitude = 0.0;  // radians
    double latitude = 0.0;   // radians
    double height = 0.0;     // meters above ellipsoid
};

// A position on the earth defined by longitude, latitude, and height above the WGS84 ellipsoid.
// Ported from: itwinjs-core core/common/src/geometry/Cartographic.ts
class DQ_COMMON_EXPORT Cartographic {
public:
    double longitude = 0.0;  // radians
    double latitude = 0.0;   // radians
    double height = 0.0;     // meters above ellipsoid

    constexpr Cartographic() noexcept = default;
    constexpr Cartographic(double lon, double lat, double h) noexcept
        : longitude(lon), latitude(lat), height(h)
    {
    }

    // Create with all zeros.
    // Ported from: itwinjs-core Cartographic.createZero()
    static Cartographic createZero() noexcept { return Cartographic(0.0, 0.0, 0.0); }

    // Create from radians.
    // Ported from: itwinjs-core Cartographic.fromRadians()
    static Cartographic fromRadians(double longitude, double latitude, double height = 0.0) noexcept
    {
        return Cartographic(longitude, latitude, height);
    }

    // Create from degrees.
    // Ported from: itwinjs-core Cartographic.fromDegrees()
    static Cartographic fromDegrees(double longitudeDeg, double latitudeDeg, double height = 0.0) noexcept
    {
        return Cartographic(longitudeDeg * kPi / 180.0, latitudeDeg * kPi / 180.0, height);
    }

    // To JSON.
    // Ported from: itwinjs-core Cartographic.toJSON()
    CartographicProps toJSON() const noexcept { return {longitude, latitude, height}; }

    // Longitude in degrees.
    // Ported from: itwinjs-core Cartographic.longitudeDegrees
    double getLongitudeDegrees() const noexcept { return longitude * 180.0 / kPi; }

    // Latitude in degrees.
    // Ported from: itwinjs-core Cartographic.latitudeDegrees
    double getLatitudeDegrees() const noexcept { return latitude * 180.0 / kPi; }

    // clone.
    // Ported from: itwinjs-core Cartographic.clone()
    Cartographic clone() const noexcept { return *this; }

    // Exact equality.
    // Ported from: itwinjs-core Cartographic.equals()
    bool equals(const Cartographic& right) const noexcept
    {
        return longitude == right.longitude && latitude == right.latitude && height == right.height;
    }

    // Approximate equality.
    // Ported from: itwinjs-core Cartographic.equalsEpsilon()
    bool equalsEpsilon(const Cartographic& right, double epsilon) const noexcept
    {
        return std::abs(longitude - right.longitude) <= epsilon &&
               std::abs(latitude - right.latitude) <= epsilon &&
               std::abs(height - right.height) <= epsilon;
    }

    // Convert to ECEF (Earth Centered Earth Fixed) coordinates.
    // Ported from: itwinjs-core Cartographic.toEcef()
    dqGeom::Point3d toEcef() const;

    // Create from ECEF coordinates.
    // Ported from: itwinjs-core Cartographic.fromEcef()
    static std::optional<Cartographic> fromEcef(const dqGeom::Point3d& cartesian);

    // Geocentric latitude from geodetic latitude.
    // Ported from: itwinjs-core Cartographic.geocentricLatitudeFromGeodeticLatitude()
    static double geocentricLatitudeFromGeodeticLatitude(double geodeticLatitude) noexcept;

    // Parametric latitude from geodetic latitude.
    // Ported from: itwinjs-core Cartographic.parametricLatitudeFromGeodeticLatitude()
    static double parametricLatitudeFromGeodeticLatitude(double geodeticLatitude) noexcept;

    // String representation.
    // Ported from: itwinjs-core Cartographic.toString()
    std::string toString() const;

private:
    static constexpr double kPi = 3.14159265358979323846;

    // WGS84 ellipsoid constants
    static constexpr double kEquatorialRadius = 6378137.0;
    static constexpr double kPolarRadius = 6356752.3142451793;
    static constexpr double kOneMinusF = 1.0 - (kEquatorialRadius - kPolarRadius) / kEquatorialRadius;
    static constexpr double kEquatorOverPolar = kEquatorialRadius / kPolarRadius;

    // WGS84 derived constants for ECEF conversion
    static constexpr double kOneOverEquatorialSquared = 1.0 / (kEquatorialRadius * kEquatorialRadius);
    static constexpr double kOneOverPolarSquared = 1.0 / (kPolarRadius * kPolarRadius);
    static constexpr double kEquatorialSquared = kEquatorialRadius * kEquatorialRadius;
    static constexpr double kPolarSquared = kPolarRadius * kPolarRadius;
    static constexpr double kCenterToleranceSquared = 0.1;

    // Scale a point to the geodetic surface.
    static dqGeom::Point3d scalePointToGeodeticSurface(const dqGeom::Point3d& point);
};

END_DQ_COMMON_NAMESPACE
