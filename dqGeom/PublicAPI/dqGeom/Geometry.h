// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Geometry constants & scalar helpers
//
// Ported from: itwinjs-core core/geometry/src/Geometry.ts
//
// Subset: the constants and free scalar functions used by the Ellipsoid port and
// the section-arc clip chain (BackgroundMapGeometry depth fitting). The full
// Geometry.ts surface is TODO — add functions here as their consumers are ported.
//
// kSmallMetricDistance (Geometry.smallMetricDistance) lives in Point3d.h;
// kSmallAngleRadians (Geometry.smallAngleRadians) lives in Angle.h as
// Angle::kSmallAngleRadians — both predate this header and are reused, not
// redefined.
#pragma once

#include <cmath>
#include <optional>

namespace dqGeom {

// --- Constants (Geometry.ts:256-284) ---
inline constexpr double kSmallFraction = 1.0e-10;          // Geometry.smallFraction (:268)
inline constexpr double kSmallNewtonStep = 1.0e-11;        // Geometry.smallNewtonStep (:270)
inline constexpr double kSmallFloatingPoint = 1.0e-15;     // Geometry.smallFloatingPoint (:272)
inline constexpr double kLargeFractionResult = 1.0e10;     // Geometry.largeFractionResult (:279)
inline constexpr double kLargeCoordinateResult = 1.0e13;   // Geometry.largeCoordinateResult (:284)

// --- Scalar helpers (free functions; names 1:1 with Geometry statics, §3) ---

/// Ported from: Geometry.isSameCoordinate (Geometry.ts:353-358).
inline bool isSameCoordinate(double x, double y, double tolerance = 1.0e-6) noexcept {
    double d = x - y;
    if (d < 0.0)
        d = -d;
    return d <= tolerance;
}

/// Ported from: Geometry.conditionalDivideFraction (Geometry.ts:1121-1127).
/// Returns numerator/denominator, or nullopt if denominator == 0 or the ratio
/// would exceed kLargeFractionResult.
inline std::optional<double> conditionalDivideFraction(double numerator, double denominator) noexcept {
    if (0.0 == denominator)
        return std::nullopt;
    if (std::abs(denominator) * kLargeFractionResult >= std::abs(numerator))
        return numerator / denominator;
    return std::nullopt;
}

/// Ported from: Geometry.conditionalDivideCoordinate (Geometry.ts:1148-1156).
inline std::optional<double> conditionalDivideCoordinate(
    double numerator, double denominator, double largestResult = kLargeCoordinateResult) noexcept {
    if (0.0 == denominator)
        return std::nullopt;
    if (std::abs(denominator * largestResult) >= std::abs(numerator))
        return numerator / denominator;
    return std::nullopt;
}

/// Ported from: Geometry.isIn01 (Geometry.ts:1298-1300).
inline bool isIn01(double x, bool apply01 = true) noexcept {
    return apply01 ? (x >= 0.0 && x <= 1.0) : true;
}

/// Ported from: Geometry.interpolate (Geometry.ts:1040-1042).
inline double interpolate(double a, double f, double b) noexcept {
    return f <= 0.5 ? a + f * (b - a) : b - (1.0 - f) * (b - a);
}

/// Ported from: Geometry.hypotenuseXY (Geometry.ts:756-758).
inline double hypotenuseXY(double x, double y) noexcept {
    return std::sqrt(x * x + y * y);
}

/// Ported from: Geometry.hypotenuseXYZ (Geometry.ts:767-769).
inline double hypotenuseXYZ(double x, double y, double z) noexcept {
    return std::sqrt(x * x + y * y + z * z);
}

/// Ported from: Geometry.hypotenuseSquaredXYZ (Geometry.ts:771-773).
inline double hypotenuseSquaredXYZ(double x, double y, double z) noexcept {
    return x * x + y * y + z * z;
}

/// Ported from: Geometry.dotProductXYZXYZ (Geometry.ts:934-936).
inline double dotProductXYZXYZ(double ux, double uy, double uz, double vx, double vy, double vz) noexcept {
    return ux * vx + uy * vy + uz * vz;
}

/// Ported from: Geometry.crossProductXYXY (Geometry.ts:891-893).
inline double crossProductXYXY(double ux, double uy, double vx, double vy) noexcept {
    return ux * vy - uy * vx;
}

/// Ported from: Geometry.maxAbsXY (Geometry.ts:715-717).
inline double maxAbsXY(double x, double y) noexcept {
    return std::fmax(std::abs(x), std::abs(y));
}

} // namespace dqGeom
