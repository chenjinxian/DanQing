// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Polynomials (analytic roots, implicit sphere, trig forms)
//
// Ported from: itwinjs-core core/geometry/src/numerics/Polynomials.ts
//
// Subset consumed by the Ellipsoid port and the section-arc clip chain:
//   - Degree2PowerPolynomial.solveQuadratic     (SphereImplicit.intersectSphereRay)
//   - SphereImplicit                             (Ellipsoid.intersectRay, closest-point seed)
//   - AnalyticRoots.appendImplicitLineUnitCircleIntersections
//                                               (ClipPlane.appendIntersectionRadians)
//   - SineCosinePolynomial                       (Arc3d.extendRangeInSweep)
// The rest of Polynomials.ts is TODO.
#pragma once

#include "Angle.h"
#include "AngleSweep.h"
#include "Export.h"
#include "Geometry.h"
#include "LongitudeLatitudeAltitude.h"
#include "Point3d.h"
#include "Range3d.h"
#include "Ray3d.h"
#include "Vector3d.h"

#include <cmath>
#include <optional>
#include <vector>

namespace dqGeom {

/// Ported from: itwinjs-core Degree2PowerPolynomial (Polynomials.ts)
struct DQ_GEOM_EXPORT Degree2PowerPolynomial {
    /// Solve a*x*x + b*x + c = 0. Returns the real roots (ascending), a duplicated
    /// root for tangency, a single root for the degenerate (a≈0) linear case, or
    /// nullopt when there are no real roots.
    /// Ported from: Degree2PowerPolynomial.solveQuadratic (Polynomials.ts:41-62)
    static std::optional<std::vector<double>> solveQuadratic(double a, double b, double c) noexcept {
        auto const b1 = conditionalDivideFraction(b, a);
        auto const c1 = conditionalDivideFraction(c, a);
        if (b1.has_value() && c1.has_value()) {
            // now solving xx + b1*x + c1 = 0 -- i.e. implied "a" coefficient is 1 . .
            double const q = (*b1) * (*b1) - 4.0 * (*c1);
            if (q > 0.0) {
                double const e = std::sqrt(q);
                // e is positive, so this sorts algebraically
                return std::vector<double>{0.5 * (-(*b1) - e), 0.5 * (-(*b1) + e)};
            }
            if (q < 0.0)
                return std::nullopt;
            double const root = -0.5 * (*b1);
            return std::vector<double>{root, root};
        }
        // "divide by a" failed.  solve bx + c = 0
        auto const x = conditionalDivideFraction(-c, b);
        if (x.has_value())
            return std::vector<double>{*x};
        return std::nullopt;
    }
};

/// Sphere as implicit function x*x + y*y + z*z - r*r = 0.
/// Ported from: itwinjs-core SphereImplicit (Polynomials.ts:364-560, subset)
class DQ_GEOM_EXPORT SphereImplicit {
public:
    double radius;
    explicit SphereImplicit(double r) noexcept : radius(r) {}

    /// Result of xyzToThetaPhiR (:385 return object).
    struct ThetaPhiR {
        double thetaRadians = 0.0;
        double phiRadians = 0.0;
        double r = 0.0;
        bool valid = false;
    };

    /// Given an xyz coordinate in the local system of the sphere, compute the sphere
    /// parametrization (theta = angular coordinate in xy plane, phi = rotation from
    /// xy plane towards z axis, r = distance from origin).
    /// Ported from: SphereImplicit.xyzToThetaPhiR (Polynomials.ts:385-410)
    ThetaPhiR xyzToThetaPhiR(Point3d const& xyz) const noexcept {
        double const rhoSquared = xyz.x * xyz.x + xyz.y * xyz.y;
        double const rho = std::sqrt(rhoSquared);
        double const r = std::sqrt(rhoSquared + xyz.z * xyz.z);
        ThetaPhiR out;
        out.r = r;
        if (r == 0.0) {
            out.thetaRadians = 0.0;
            out.phiRadians = 0.0;
            out.valid = false;
        } else {
            out.phiRadians = std::atan2(xyz.z, rho);  // At least one of these is nonzero
            if (rhoSquared != 0.0) {
                out.thetaRadians = std::atan2(xyz.y, xyz.x);
                out.valid = true;
            } else {
                out.thetaRadians = 0.0;
                out.valid = false;
            }
        }
        return out;
    }

    /// Compute intersections with a ray. Returns the number of intersections and
    /// fills any combination of rayFractions / xyz / thetaPhiRadians (each optional,
    /// cleared on entry). Ported from: SphereImplicit.intersectSphereRay
    /// (Polynomials.ts:481-524).
    static size_t intersectSphereRay(
        Point3d const& center, double radius, Ray3d const& ray,
        std::vector<double>* rayFractions,
        std::vector<Point3d>* xyz,
        std::vector<LongitudeLatitudeNumber>* thetaPhiRadians) {
        double const vx = ray.origin.x - center.x;
        double const vy = ray.origin.y - center.y;
        double const vz = ray.origin.z - center.z;
        double const ux = ray.direction.x;
        double const uy = ray.direction.y;
        double const uz = ray.direction.z;
        double const a0 = hypotenuseSquaredXYZ(vx, vy, vz) - radius * radius;
        double const a1 = 2.0 * dotProductXYZXYZ(ux, uy, uz, vx, vy, vz);
        double const a2 = hypotenuseSquaredXYZ(ux, uy, uz);
        auto const parameters = Degree2PowerPolynomial::solveQuadratic(a2, a1, a0);
        if (rayFractions != nullptr)
            rayFractions->clear();
        if (xyz != nullptr)
            xyz->clear();
        if (thetaPhiRadians != nullptr)
            thetaPhiRadians->clear();

        if (!parameters.has_value())
            return 0;
        SphereImplicit const sphere(radius);
        if (rayFractions != nullptr)
            for (double f : *parameters)
                rayFractions->push_back(f);
        if (xyz != nullptr || thetaPhiRadians != nullptr) {
            for (double f : *parameters) {
                Point3d const point = ray.FractionToPoint(f);
                if (xyz != nullptr)
                    xyz->push_back(point);
                if (thetaPhiRadians != nullptr) {
                    auto const data = sphere.xyzToThetaPhiR(point);
                    thetaPhiRadians->push_back(
                        LongitudeLatitudeNumber::createRadians(data.thetaRadians, data.phiRadians));
                }
            }
        }
        return parameters->size();
    }

    /// Convert radians to xyz on the unit sphere (radius is implicitly 1).
    /// Ported from: SphereImplicit.radiansToUnitSphereXYZ (Polynomials.ts:554-562)
    static void radiansToUnitSphereXYZ(double thetaRadians, double phiRadians, Vector3d& xyz) noexcept {
        double const cosTheta = std::cos(thetaRadians);
        double const sinTheta = std::sin(thetaRadians);
        double const cosPhi = std::cos(phiRadians);
        double const sinPhi = std::sin(phiRadians);
        xyz.x = cosTheta * cosPhi;
        xyz.y = sinTheta * cosPhi;
        xyz.z = sinPhi;
    }
};

/// Ported from: itwinjs-core AnalyticRoots (Polynomials.ts, subset)
struct DQ_GEOM_EXPORT AnalyticRoots {
    /// Solve the simultaneous equations in variables c and s:
    ///   line: alpha + beta*c + gamma*s = 0
    ///   unit circle: c*c + s*s = 1
    /// Solution values are appended (as (c,s) pairs and/or radians) to the arrays.
    /// Returns the solution state: -2 (all coeffs 0 — whole circle is solution),
    /// -1 (no line defined, no solutions), 0 (line outside circle — 2 closest-approach
    /// points appended), 1 (tangent — 2 near-coincident points appended),
    /// 2 (two intersections).
    /// Ported from: AnalyticRoots.appendImplicitLineUnitCircleIntersections
    /// (Polynomials.ts:1023-1071)
    static int appendImplicitLineUnitCircleIntersections(
        double alpha, double beta, double gamma,
        std::vector<double>* cosValues,
        std::vector<double>* sinValues,
        std::vector<double>* radiansValues,
        double relTol = 1.0e-14) {
        double const twoTol = (relTol < 0.0) ? 0.0 : 2.0 * relTol;
        double const delta2 = beta * beta + gamma * gamma;
        double const alpha2 = alpha * alpha;
        int solutionType = 0;
        if (delta2 <= 0.0) {
            solutionType = (alpha == 0.0) ? -2 : -1;
        } else {
            double const lambda = -alpha / delta2;
            double const a2 = alpha2 / delta2;
            double const D2 = 1.0 - a2;
            if (D2 < -twoTol) {
                double const delta = std::sqrt(delta2);
                double const iota = (alpha < 0.0) ? (1.0 / delta) : (-1.0 / delta);
                appendCosSinRadians(beta * iota, gamma * iota, cosValues, sinValues, radiansValues);
                appendCosSinRadians(lambda * beta, lambda * gamma, cosValues, sinValues, radiansValues);
                solutionType = 0;
            } else if (D2 < twoTol) {
                double const delta = std::sqrt(delta2);
                double const iota = (alpha < 0.0) ? (1.0 / delta) : (-1.0 / delta);
                appendCosSinRadians(beta * iota, gamma * iota, cosValues, sinValues, radiansValues);
                appendCosSinRadians(lambda * beta, lambda * gamma, cosValues, sinValues, radiansValues);
                solutionType = 1;
            } else {
                double const mu = std::sqrt(D2 / delta2);
                // c0,s0 = closest approach of line to origin
                double const c0 = lambda * beta;
                double const s0 = lambda * gamma;
                appendCosSinRadians(c0 - mu * gamma, s0 + mu * beta, cosValues, sinValues, radiansValues);
                appendCosSinRadians(c0 + mu * gamma, s0 - mu * beta, cosValues, sinValues, radiansValues);
                solutionType = 2;
            }
        }
        return solutionType;
    }

private:
    /// Ported from: AnalyticRoots.appendCosSinRadians (Polynomials.ts:993-998)
    static void appendCosSinRadians(
        double c, double s,
        std::vector<double>* cosValues,
        std::vector<double>* sinValues,
        std::vector<double>* radiansValues) {
        if (cosValues != nullptr)
            cosValues->push_back(c);
        if (sinValues != nullptr)
            sinValues->push_back(s);
        if (radiansValues != nullptr)
            radiansValues->push_back(std::atan2(s, c));
    }
};

/// Trigonometric function a + cosineCoff*cos(theta) + sineCoff*sin(theta).
/// Ported from: itwinjs-core SineCosinePolynomial (Polynomials.ts:1428-1488, subset)
struct DQ_GEOM_EXPORT SineCosinePolynomial {
    double a;           ///< constant coefficient
    double cosineCoff;  ///< cos(theta) coefficient
    double sineCoff;    ///< sin(theta) coefficient

    SineCosinePolynomial(double a_, double cosCoff, double sinCoff) noexcept
        : a(a_), cosineCoff(cosCoff), sineCoff(sinCoff) {}

    /// set all coefficients (:1447-1451)
    void set(double a_, double cosCoff, double sinCoff) noexcept {
        a = a_;
        cosineCoff = cosCoff;
        sineCoff = sinCoff;
    }

    /// Return the function value at given angle in radians (:1453-1455)
    double evaluateRadians(double theta) const noexcept {
        return a + cosineCoff * std::cos(theta) + sineCoff * std::sin(theta);
    }

    /// Return the range of function values over the entire angle range (:1457-1460).
    Range1d range() const noexcept {
        double const q = hypotenuseXY(cosineCoff, sineCoff);
        return Range1d::CreateXX(a - q, a + q);
    }

    /// Return the min and max values of the function over theta range from radians0
    /// to radians1 inclusive (:1462-1475).
    Range1d rangeInStartEndRadians(double radians0, double radians1) const noexcept {
        if (Angle::IsFullCircleRadians(radians1 - radians0))
            return range();
        Range1d result = Range1d::CreateXX(evaluateRadians(radians0), evaluateRadians(radians1));
        // angles of min and max of the sine wave . ..
        double const alphaA = std::atan2(sineCoff, cosineCoff);
        double const alphaB = alphaA + Angle::kPi;
        if (AngleSweep::isRadiansInStartEnd(alphaA, radians0, radians1))
            result.Extend(evaluateRadians(alphaA));
        if (AngleSweep::isRadiansInStartEnd(alphaB, radians0, radians1))
            result.Extend(evaluateRadians(alphaB));
        return result;
    }

    /// Return the min and max values of the function over the sweep (:1477-1479).
    Range1d rangeInSweep(AngleSweep const& sweep) const noexcept {
        return rangeInStartEndRadians(sweep.StartRadians(), sweep.EndRadians());
    }
};

} // namespace dqGeom
