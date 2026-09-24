// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — LongitudeLatitudeNumber
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/LongitudeLatitudeAltitude.ts
//              (LongitudeLatitudeNumber, L21-125)
//
// Detailed data for a point on a 2-angle parameter space (longitude/latitude
// strongly-typed Angle pair + altitude). Subset: the angle/altitude accessors and
// factories used by the Ellipsoid port (intersectRay / surfaceNormalToAngles /
// projectPointToSurface) and its tests. JSON I/O and the optional `result`
// pre-allocation parameters are TODO (no ported consumer yet).
#pragma once

#include "Angle.h"
#include "Export.h"
#include "Geometry.h"

namespace dqGeom {

/// Ported from: itwinjs-core LongitudeLatitudeNumber (LongitudeLatitudeAltitude.ts:21-125)
class DQ_GEOM_EXPORT LongitudeLatitudeNumber {
public:
    /// Ported from: LongitudeLatitudeNumber.createRadians (:65-73)
    static LongitudeLatitudeNumber createRadians(double longitudeRadians, double latitudeRadians, double h = 0.0) noexcept {
        return LongitudeLatitudeNumber(Angle::FromRadians(longitudeRadians), Angle::FromRadians(latitudeRadians), h);
    }
    /// Ported from: LongitudeLatitudeNumber.createDegrees (:75-83, non-`result` branch)
    static LongitudeLatitudeNumber createDegrees(double longitudeDegrees, double latitudeDegrees, double h = 0.0) noexcept {
        return LongitudeLatitudeNumber(Angle::FromDegrees(longitudeDegrees), Angle::FromDegrees(latitudeDegrees), h);
    }
    /// Ported from: LongitudeLatitudeNumber.createZero (:53)
    static LongitudeLatitudeNumber createZero() noexcept {
        return LongitudeLatitudeNumber(Angle::FromDegrees(0.0), Angle::FromDegrees(0.0), 0.0);
    }

    /// longitude in radians (:26)
    double longitudeRadians() const noexcept { return m_longitude.Radians(); }
    /// longitude in degrees (:28)
    double longitudeDegrees() const noexcept { return m_longitude.Degrees(); }
    /// latitude in radians (:35)
    double latitudeRadians() const noexcept { return m_latitude.Radians(); }
    /// latitude in degrees (:37)
    double latitudeDegrees() const noexcept { return m_latitude.Degrees(); }
    /// altitude (:44-45)
    double altitude() const noexcept { return m_altitude; }
    void setAltitude(double value) noexcept { m_altitude = value; }

    /// Ported from: LongitudeLatitudeNumber.isAlmostEqual (:116-120)
    bool isAlmostEqual(LongitudeLatitudeNumber const& other) const noexcept {
        return m_latitude.IsAlmostEqual(other.m_latitude)
            && m_longitude.IsAlmostEqual(other.m_longitude)
            && isSameCoordinate(m_altitude, other.m_altitude);
    }
    /// Ported from: LongitudeLatitudeNumber.clone (:122-124)
    LongitudeLatitudeNumber clone() const noexcept { return *this; }

private:
    /// Constructor: capture angles and altitude (:47-51)
    LongitudeLatitudeNumber(Angle longitude, Angle latitude, double altitude) noexcept
        : m_longitude(longitude), m_latitude(latitude), m_altitude(altitude) {}

    Angle m_longitude;
    Angle m_latitude;
    double m_altitude = 0.0;
};

} // namespace dqGeom
