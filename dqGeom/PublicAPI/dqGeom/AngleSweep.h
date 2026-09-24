// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — AngleSweep value type
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/AngleSweep.ts
//
// Stores start and end angles in radians. Provides fraction-to-radians
// conversion critical for Arc3d parameterization.
#pragma once

#include "Angle.h"

#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// AngleSweep — angular interval [radians0, radians1]
// ---------------------------------------------------------------------------
struct DQ_GEOM_EXPORT AngleSweep {
private:
    double m_radians0 = 0.0;
    double m_radians1 = 0.0;

public:
    AngleSweep() noexcept = default;

    // --- Factories (itwinjs AngleSweep.create*) ---
    static AngleSweep FromStartEndRadians(double r0, double r1) noexcept
    {
        AngleSweep s;
        s.m_radians0 = r0;
        s.m_radians1 = r1;
        return s;
    }

    static AngleSweep FromStartSweepRadians(double r0, double sweep) noexcept
    {
        return FromStartEndRadians(r0, r0 + sweep);
    }

    static AngleSweep FromStartEndDegrees(double d0, double d1) noexcept
    {
        return FromStartEndRadians(Angle::DegreesToRadians(d0), Angle::DegreesToRadians(d1));
    }

    static AngleSweep FromStartSweepDegrees(double d0, double sweepDeg) noexcept
    {
        return FromStartSweepRadians(Angle::DegreesToRadians(d0), Angle::DegreesToRadians(sweepDeg));
    }

    static AngleSweep FullCircle() noexcept
    {
        return FromStartEndRadians(0.0, Angle::k2Pi);
    }

    // --- Accessors ---
    double StartRadians() const noexcept { return m_radians0; }
    double EndRadians() const noexcept { return m_radians1; }
    double SweepRadians() const noexcept { return m_radians1 - m_radians0; }
    double StartDegrees() const noexcept { return m_radians0 * Angle::kRadiansToDegrees; }
    double EndDegrees() const noexcept { return m_radians1 * Angle::kRadiansToDegrees; }
    double SweepDegrees() const noexcept { return SweepRadians() * Angle::kRadiansToDegrees; }

    // --- Queries ---
    bool isEmpty() const noexcept { return m_radians0 == m_radians1; }
    bool IsCCW() const noexcept { return m_radians1 >= m_radians0; }
    bool IsFullCircle() const noexcept
    {
        return std::abs(SweepRadians()) >= Angle::k2Pi - Angle::kSmallAngleRadians;
    }

    // --- Fraction ↔ Radians (critical for Arc3d) ---
    // Ported from: itwinjs AngleSweep.fractionToRadians
    double FractionToRadians(double fraction) const noexcept
    {
        return m_radians0 + fraction * (m_radians1 - m_radians0);
    }

    // Ported from: itwinjs AngleSweep.radiansToPositivePeriodicFraction
    double RadiansToFraction(double radians) const noexcept
    {
        double sweep = m_radians1 - m_radians0;
        if (std::abs(sweep) < Angle::kSmallAngleRadians) return 0.0;
        return (radians - m_radians0) / sweep;
    }

    // --- Modification ---
    void SetStartEndRadians(double r0, double r1) noexcept
    {
        m_radians0 = r0;
        m_radians1 = r1;
    }

    void SetStartSweepRadians(double r0, double sweep) noexcept
    {
        m_radians0 = r0;
        m_radians1 = r0 + sweep;
    }

    void ReverseInPlace() noexcept
    {
        std::swap(m_radians0, m_radians1);
    }

    AngleSweep CloneReversed() const noexcept
    {
        return FromStartEndRadians(m_radians1, m_radians0);
    }

    // --- Comparison ---
    bool IsAlmostEqual(AngleSweep const& other, double tol = Angle::kSmallAngleRadians) const noexcept
    {
        return std::abs(m_radians0 - other.m_radians0) < tol &&
               std::abs(m_radians1 - other.m_radians1) < tol;
    }

    // Check if a given angle is within this sweep
    bool IsAngleInSweep(double radians, bool allowPeriodShift = false) const noexcept
    {
        double sweep = m_radians1 - m_radians0;
        if (std::abs(sweep) < Angle::kSmallAngleRadians) return false;

        double fraction = (radians - m_radians0) / sweep;
        if (allowPeriodShift) {
            fraction = fraction - std::floor(fraction);
        }
        return fraction >= -Angle::kSmallAngleRadians &&
               fraction <= 1.0 + Angle::kSmallAngleRadians;
    }

    // --- itwinjs AngleSweep.ts periodic fraction conversions (camelCase, 1:1) ---

    /// Convert a sweep fraction to the equivalent period-shifted fraction inside
    /// [0,1] when possible, with direction control for exterior fractions.
    /// Ported from: itwinjs-core AngleSweep.fractionToSignedPeriodicFractionStartEnd
    /// (AngleSweep.ts:285-305)
    static double fractionToSignedPeriodicFractionStartEnd(
        double fraction, double radians0, double radians1, bool toNegativeFraction) noexcept
    {
        double const sweep = radians1 - radians0;
        if (Angle::isAlmostEqualRadiansNoPeriodShift(0.0, sweep))
            return fraction;  // empty sweep
        if (fraction >= 0.0 && fraction <= 1.0)   // Geometry.isIn01
            return fraction;
        double const period = Angle::k2Pi / std::abs(sweep);
        fraction = std::fmod(fraction, period);  // period-shifted equivalent fraction closest to 0 with same sign
        if (fraction + period < 1.0)
            fraction += period;  // it's really an interior fraction
        if (fraction >= 0.0 && fraction <= 1.0)   // Geometry.isIn01
            return fraction;
        if (toNegativeFraction)
            return fraction < 0.0 ? fraction : fraction - period;
        return fraction > 1.0 ? fraction : fraction + period;
        // (reference's toNegativeFraction-undefined branch — pick the closer
        // period-shift — is unreachable from the ported call sites, which always
        // pass an explicit bool; AngleSweep.ts:301-304 TODO if ever needed.)
    }

    /// Return the fractionalized position of `radians` within (radians0, radians1),
    /// considering the 2PI period and the sweep direction (CW or CCW).
    /// Ported from: itwinjs-core AngleSweep.radiansToSignedPeriodicFractionStartEnd
    /// (AngleSweep.ts:404-419)
    static double radiansToSignedPeriodicFractionStartEnd(
        double radians, double radians0, double radians1, double zeroSweepDefault = 0.0) noexcept
    {
        double const sweep = radians1 - radians0;
        if (Angle::isAlmostEqualRadiansNoPeriodShift(0.0, sweep))
            return zeroSweepDefault;
        if (Angle::isAlmostEqualRadiansAllowPeriodShift(radians0, radians1)) {
            // for sweep = 2nPi !== 0, allow matching without period shift, else we never return 1.0
            if (Angle::isAlmostEqualRadiansNoPeriodShift(radians, radians0))
                return 0.0;
            if (Angle::isAlmostEqualRadiansNoPeriodShift(radians, radians1))
                return 1.0;
        } else {
            if (Angle::isAlmostEqualRadiansAllowPeriodShift(radians, radians0))
                return 0.0;
            if (Angle::isAlmostEqualRadiansAllowPeriodShift(radians, radians1))
                return 1.0;
        }
        double const fraction = (radians - radians0) / sweep;
        return fractionToSignedPeriodicFractionStartEnd(fraction, radians0, radians1, fraction < 0.0);
    }

    /// Return the fractionalized position of `radians` within (radians0, radians1),
    /// considering the 2PI period; exterior angles are at fractions > 1.
    /// Ported from: itwinjs-core AngleSweep.radiansToPositivePeriodicFractionStartEnd
    /// (AngleSweep.ts:335-345)
    static double radiansToPositivePeriodicFractionStartEnd(
        double radians, double radians0, double radians1, double zeroSweepDefault = 0.0) noexcept
    {
        double constexpr zeroSweepMarker = 1.0e13;  // Geometry.largeCoordinateResult
        double fraction = radiansToSignedPeriodicFractionStartEnd(radians, radians0, radians1, zeroSweepMarker);
        if (fraction == zeroSweepMarker)
            return zeroSweepDefault;
        if (fraction < 0.0) {
            double const period = Angle::k2Pi / std::abs(radians1 - radians0);
            fraction += period;
        }
        return fraction;
    }

    /// Ported from: itwinjs-core AngleSweep.radiansToSignedPeriodicFraction (AngleSweep.ts:436-438)
    double radiansToSignedPeriodicFraction(double radians, double zeroSweepDefault = 0.0) const noexcept
    {
        return radiansToSignedPeriodicFractionStartEnd(radians, m_radians0, m_radians1, zeroSweepDefault);
    }

    /// Ported from: itwinjs-core AngleSweep.radiansToPositivePeriodicFraction (AngleSweep.ts:357-359)
    double radiansToPositivePeriodicFraction(double radians, double zeroSweepDefault = 0.0) const noexcept
    {
        return radiansToPositivePeriodicFractionStartEnd(radians, m_radians0, m_radians1, zeroSweepDefault);
    }

    /// Fractionalize an array of angles (radians) with consideration of the 2PI
    /// period; each entry is replaced in place by its positive periodic fraction.
    /// Ported from: itwinjs-core AngleSweep.radiansArrayToPositivePeriodicFractions
    /// (AngleSweep.ts:383-388)
    void radiansArrayToPositivePeriodicFractions(std::vector<double>& data) const noexcept
    {
        for (double& d : data)
            d = radiansToPositivePeriodicFraction(d);
    }

    /// Test if the given angle (as radians) is within the sweep (start/end bounds).
    /// Ported from: itwinjs-core AngleSweep.isRadiansInStartEnd (AngleSweep.ts:515-523)
    static bool isRadiansInStartEnd(double radians, double radians0, double radians1,
                                    bool allowPeriodShift = true) noexcept
    {
        double const delta0 = radians - radians0;
        double const delta1 = radians - radians1;
        if (delta0 * delta1 <= 0.0)
            return true;
        if (radians0 == radians1)
            return allowPeriodShift
                ? Angle::isAlmostEqualRadiansAllowPeriodShift(radians, radians0)
                : Angle::isAlmostEqualRadiansNoPeriodShift(radians, radians0);
        return allowPeriodShift
            ? radiansToPositivePeriodicFractionStartEnd(radians, radians0, radians1, 1000.0) <= 1.0
            : false;
    }

    /// Test if the given angle (as radians) is within the sweep.
    /// Ported from: itwinjs-core AngleSweep.isRadiansInSweep (AngleSweep.ts:525-527)
    bool isRadiansInSweep(double radians, bool allowPeriodShift = true) const noexcept
    {
        return isRadiansInStartEnd(radians, m_radians0, m_radians1, allowPeriodShift);
    }
};

END_DQ_GEOM_NAMESPACE
