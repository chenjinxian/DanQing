// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Angle value type
//
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Angle.h
//              itwinjs-core core/geometry/src/geometry3d/Angle.ts
//
// Stores radians internally. Factory methods enforce explicit radian/degree
// construction to prevent unit confusion.
//
// CLAUDE.md §7.2: no operator overloads for geometric operations.
#pragma once

#include "DqGeom.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Angle — angle value type (radians internally)
// ---------------------------------------------------------------------------
struct DQ_GEOM_EXPORT Angle {
private:
    double m_radians = 0.0;
    explicit Angle(double radians) noexcept : m_radians(radians) {}

public:
    Angle() noexcept = default;

    // --- Factories (imodel-native Angle::From*) ---
    static Angle FromRadians(double r) noexcept { return Angle(r); }
    static Angle FromDegrees(double d) noexcept { return Angle(d * kDegreesToRadians); }
    static Angle FromAtan2(double y, double x) noexcept { return Angle(std::atan2(y, x)); }
    static Angle Zero() noexcept { return Angle(0.0); }
    static Angle FullCircle() noexcept { return Angle(k2Pi); }

    // --- Accessors ---
    double Radians() const noexcept { return m_radians; }
    double Degrees() const noexcept { return m_radians * kRadiansToDegrees; }

    // --- Trig ---
    double Cos() const noexcept { return std::cos(m_radians); }
    double Sin() const noexcept { return std::sin(m_radians); }
    double Tan() const noexcept { return std::tan(m_radians); }

    // --- Queries ---
    bool IsAlmostZero(double tol = kSmallAngleRadians) const noexcept
    {
        return std::abs(m_radians) < tol;
    }
    bool IsFullCircle() const noexcept { return IsFullCircleRadians(m_radians); }
    bool IsHalfCircle() const noexcept { return IsHalfCircleRadians(m_radians); }

    // --- Modification ---
    void SetRadians(double r) noexcept { m_radians = r; }
    void SetDegrees(double d) noexcept { m_radians = d * kDegreesToRadians; }

    // --- Normalization ---
    Angle NormalizeTo02Pi() const noexcept { return Angle(AdjustRadians0To2Pi(m_radians)); }
    Angle NormalizeToPlusMinusPi() const noexcept { return Angle(AdjustRadiansMinusPiPlusPi(m_radians)); }

    // --- Comparison ---
    bool IsAlmostEqual(Angle const& other, double tol = kSmallAngleRadians) const noexcept
    {
        return std::abs(m_radians - other.m_radians) < tol;
    }

    // --- Constants ---
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double k2Pi = 2.0 * kPi;
    static constexpr double kPiOver2 = kPi / 2.0;
    static constexpr double kPiOver4 = kPi / 4.0;
    static constexpr double kPiOver12 = kPi / 12.0;
    static constexpr double kDegreesToRadians = kPi / 180.0;
    static constexpr double kRadiansToDegrees = 180.0 / kPi;
    static constexpr double kSmallAngleRadians = 1.0e-12;

    // --- Static utilities ---
    static double DegreesToRadians(double degrees) noexcept { return degrees * kDegreesToRadians; }
    static double RadiansToDegrees(double radians) noexcept { return radians * kRadiansToDegrees; }

    static double AdjustRadians0To2Pi(double radians) noexcept
    {
        double a = std::fmod(radians, k2Pi);
        if (a < 0.0) a += k2Pi;
        return a;
    }

    static double AdjustRadiansMinusPiPlusPi(double radians) noexcept
    {
        double a = std::fmod(radians + kPi, k2Pi);
        if (a < 0.0) a += k2Pi;
        return a - kPi;
    }

    static bool IsFullCircleRadians(double radians) noexcept
    {
        return std::abs(radians) >= k2Pi - kSmallAngleRadians;
    }

    static bool IsHalfCircleRadians(double radians) noexcept
    {
        return std::abs(std::abs(radians) - kPi) < kSmallAngleRadians;
    }

    static bool NearlyEqual(double radiansA, double radiansB, double tol = kSmallAngleRadians) noexcept
    {
        return std::abs(radiansA - radiansB) < tol;
    }

    // --- itwinjs Angle.ts periodic-equality statics (camelCase, 1:1) ---

    /// Test if two angles (in radians) are almost equal, allowing shift by full
    /// circle (i.e., multiples of 2*PI).
    /// Ported from: itwinjs-core Angle.isAlmostEqualRadiansAllowPeriodShift (Angle.ts:342-352)
    static bool isAlmostEqualRadiansAllowPeriodShift(double radiansA, double radiansB,
                                                     double radianTol = kSmallAngleRadians) noexcept
    {
        double const delta = std::abs(radiansA - radiansB);
        if (delta <= radianTol)
            return true;
        double const period = k2Pi;
        if (std::abs(delta - period) <= radianTol)
            return true;
        double const numPeriod = std::round(delta / period);
        double const delta1 = delta - numPeriod * period;
        return std::abs(delta1) <= radianTol;
    }

    /// Test if two angles (in radians) are almost equal, NOT allowing shift by full circle.
    /// Ported from: itwinjs-core Angle.isAlmostEqualRadiansNoPeriodShift (Angle.ts:376-379)
    static bool isAlmostEqualRadiansNoPeriodShift(double radiansA, double radiansB,
                                                  double radianTol = kSmallAngleRadians) noexcept
    {
        return std::abs(radiansA - radiansB) < radianTol;
    }

    /// Test if this angle is almost equal to a multiple of 90 degrees (north or south pole).
    /// Ported from: itwinjs-core Angle.isAlmostNorthOrSouthPole (Angle.ts:325-327)
    bool isAlmostNorthOrSouthPole() const noexcept
    {
        return IsHalfCircleRadians(m_radians * 2.0);
    }

    // Angle between two vectors (imodel-native Angle::RadiansBetweenVectorsXYZ)
    static double RadiansBetweenVectorsXYZ(double ux, double uy, double uz,
                                           double vx, double vy, double vz) noexcept
    {
        double dot = ux * vx + uy * vy + uz * vz;
        double crossX = uy * vz - uz * vy;
        double crossY = uz * vx - ux * vz;
        double crossZ = ux * vy - uy * vx;
        return std::atan2(std::sqrt(crossX * crossX + crossY * crossY + crossZ * crossZ), dot);
    }
};

END_DQ_GEOM_NAMESPACE
