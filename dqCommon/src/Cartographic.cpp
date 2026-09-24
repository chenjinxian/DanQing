// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Cartographic implementation
//
// Ported from: itwinjs-core core/common/src/geometry/Cartographic.ts
// Portions adapted from Cesium.js Copyright 2011-2017 Cesium Contributors
#include "dqCommon/Cartographic.h"

#include <cmath>
#include <sstream>

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqGeom;

// Ported from: itwinjs-core Cartographic._scaleToGeodeticSurface
Point3d Cartographic::scalePointToGeodeticSurface(const Point3d& point)
{
    const double positionX = point.x;
    const double positionY = point.y;
    const double positionZ = point.z;

    constexpr double oneOverRadiiX = 1.0 / kEquatorialRadius;
    constexpr double oneOverRadiiY = 1.0 / kEquatorialRadius;
    constexpr double oneOverRadiiZ = 1.0 / kPolarRadius;

    const double x2 = positionX * positionX * oneOverRadiiX * oneOverRadiiX;
    const double y2 = positionY * positionY * oneOverRadiiY * oneOverRadiiY;
    const double z2 = positionZ * positionZ * oneOverRadiiZ * oneOverRadiiZ;

    const double squaredNorm = x2 + y2 + z2;
    const double ratio = std::sqrt(1.0 / squaredNorm);

    // Initial approximation
    double intersectionX = positionX * ratio;
    double intersectionY = positionY * ratio;
    double intersectionZ = positionZ * ratio;

    if (squaredNorm < kCenterToleranceSquared) {
        if (!std::isfinite(ratio))
            return Point3d::FromZero();
        return Point3d::From(intersectionX, intersectionY, intersectionZ);
    }

    constexpr double oneOverRadiiSquaredX = kOneOverEquatorialSquared;
    constexpr double oneOverRadiiSquaredY = kOneOverEquatorialSquared;
    constexpr double oneOverRadiiSquaredZ = kOneOverPolarSquared;

    double gradientX = intersectionX * oneOverRadiiSquaredX * 2.0;
    double gradientY = intersectionY * oneOverRadiiSquaredY * 2.0;
    double gradientZ = intersectionZ * oneOverRadiiSquaredZ * 2.0;

    double lambda = (1.0 - ratio) * point.Magnitude() /
                    (0.5 * std::sqrt(gradientX * gradientX + gradientY * gradientY + gradientZ * gradientZ));
    double correction = 0.0;
    double func;
    double xMultiplier, yMultiplier, zMultiplier;
    double xMultiplier2, yMultiplier2, zMultiplier2;
    double xMultiplier3, yMultiplier3, zMultiplier3;

    do {
        lambda -= correction;

        xMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquaredX);
        yMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquaredY);
        zMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquaredZ);

        xMultiplier2 = xMultiplier * xMultiplier;
        yMultiplier2 = yMultiplier * yMultiplier;
        zMultiplier2 = zMultiplier * zMultiplier;

        xMultiplier3 = xMultiplier2 * xMultiplier;
        yMultiplier3 = yMultiplier2 * yMultiplier;
        zMultiplier3 = zMultiplier2 * zMultiplier;

        func = x2 * xMultiplier2 + y2 * yMultiplier2 + z2 * zMultiplier2 - 1.0;

        const double denominator = x2 * xMultiplier3 * oneOverRadiiSquaredX +
                                   y2 * yMultiplier3 * oneOverRadiiSquaredY +
                                   z2 * zMultiplier3 * oneOverRadiiSquaredZ;
        const double derivative = -2.0 * denominator;
        correction = func / derivative;
    } while (std::abs(func) > 0.01);

    return Point3d::From(positionX * xMultiplier, positionY * yMultiplier, positionZ * zMultiplier);
}

// Ported from: itwinjs-core Cartographic.fromEcef()
std::optional<Cartographic> Cartographic::fromEcef(const Point3d& cartesian)
{
    const Point3d p = scalePointToGeodeticSurface(cartesian);
    if (p.IsEqual(Point3d::FromZero()) && !cartesian.IsEqual(Point3d::FromZero()))
        return std::nullopt;

    // Compute normal vector
    double nx = p.x * kOneOverEquatorialSquared;
    double ny = p.y * kOneOverEquatorialSquared;
    double nz = p.z * kOneOverPolarSquared;
    const double mag = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (mag > 0.0) {
        nx /= mag;
        ny /= mag;
        nz /= mag;
    }

    const double hx = cartesian.x - p.x;
    const double hy = cartesian.y - p.y;
    const double hz = cartesian.z - p.z;

    const double longitude = std::atan2(ny, nx);
    const double latitude = std::asin(nz);
    const double dot = hx * cartesian.x + hy * cartesian.y + hz * cartesian.z;
    const double hMag = std::sqrt(hx * hx + hy * hy + hz * hz);
    const double height = (dot >= 0.0 ? 1.0 : -1.0) * hMag;

    return Cartographic(longitude, latitude, height);
}

// Ported from: itwinjs-core Cartographic.toEcef()
Point3d Cartographic::toEcef() const
{
    const double cosLatitude = std::cos(latitude);
    double nx = cosLatitude * std::cos(longitude);
    double ny = cosLatitude * std::sin(longitude);
    double nz = std::sin(latitude);
    // Normalize
    const double nMag = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nMag > 0.0) {
        nx /= nMag;
        ny /= nMag;
        nz /= nMag;
    }

    // multiply by radii squared
    double kx = kEquatorialSquared * nx;
    double ky = kEquatorialSquared * ny;
    double kz = kPolarSquared * nz;

    const double gamma = std::sqrt(nx * kx + ny * ky + nz * kz);
    kx /= gamma;
    ky /= gamma;
    kz /= gamma;

    // add height component
    return Point3d::From(kx + nx * height, ky + ny * height, kz + nz * height);
}

// Ported from: itwinjs-core Cartographic.geocentricLatitudeFromGeodeticLatitude()
double Cartographic::geocentricLatitudeFromGeodeticLatitude(double geodeticLatitude) noexcept
{
    return std::atan(kOneMinusF * kOneMinusF * std::tan(geodeticLatitude));
}

// Ported from: itwinjs-core Cartographic.parametricLatitudeFromGeodeticLatitude()
double Cartographic::parametricLatitudeFromGeodeticLatitude(double geodeticLatitude) noexcept
{
    return std::atan(kOneMinusF * kOneMinusF * kEquatorOverPolar * std::tan(geodeticLatitude));
}

// Ported from: itwinjs-core Cartographic.toString()
std::string Cartographic::toString() const
{
    std::ostringstream oss;
    oss << "(" << longitude << ", " << latitude << ", " << height << ")";
    return oss.str();
}

END_DQ_COMMON_NAMESPACE
