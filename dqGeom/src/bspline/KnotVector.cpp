// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — KnotVector implementation
// Ported from: itwinjs-core core/geometry/src/bspline/KnotVector.ts
#include "dqGeom/KnotVector.h"

#include "dqGeom/Geometry.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// Ported from: KnotVector.setKnots（:230-242）。
void KnotVector::setKnots(std::vector<double> const& newKnots, bool skipFirstAndLast)
{
    size_t const numAllocate = skipFirstAndLast ? newKnots.size() - 2 : newKnots.size();
    if (numAllocate != knots.size())
        knots.assign(numAllocate, 0.0);
    if (skipFirstAndLast) {
        for (size_t i = 1; i + 1 < newKnots.size(); ++i)
            knots[i - 1] = newKnots[i];
    } else {
        for (size_t i = 0; i < newKnots.size(); ++i)
            knots[i] = newKnots[i];
    }
    setupFixedValues();
}

// Ported from: KnotVector.createUniformClamped（:255-267）。
KnotVector KnotVector::createUniformClamped(int numPoles, int degree, double a0, double a1)
{
    KnotVector out;
    out.degree = degree;
    out.knots.assign(static_cast<size_t>(numPoles + degree - 1), 0.0);
    size_t k = 0;
    for (int m = 0; m < degree; ++m)
        out.knots[k++] = a0;
    double const du = 1.0 / (numPoles - degree);
    for (int i = 1; i + degree < numPoles; ++i)
        out.knots[k++] = a0 + i * du * (a1 - a0);
    for (int m = 0; m < degree; ++m)
        out.knots[k++] = a1;
    out.setupFixedValues();
    return out;
}

// Ported from: KnotVector.createUniformWrapped（:278-285）。
KnotVector KnotVector::createUniformWrapped(int numInterval, int degree, double a0, double a1)
{
    KnotVector out;
    out.degree = degree;
    out.knots.assign(static_cast<size_t>(numInterval + 2 * degree - 1), 0.0);
    double const du = 1.0 / numInterval;
    size_t k = 0;
    for (int i = 1 - degree; i < numInterval + degree; ++i, ++k)
        out.knots[k] = interpolate(a0, i * du, a1);
    out.setupFixedValues();
    return out;
}

// Ported from: KnotVector.create（:292-297）。
KnotVector KnotVector::create(std::vector<double> const& knotArray, int degree, bool skipFirstAndLast)
{
    KnotVector out;
    out.degree = degree;
    out.setKnots(knotArray, skipFirstAndLast);
    return out;
}

// Ported from: KnotVector.grevilleKnot（:303-313）。
double KnotVector::grevilleKnot(int knotIndex) const
{
    if (knotIndex < 0)
        return leftKnot();
    if (knotIndex > rightKnotIndex())
        return rightKnot();
    double sum = 0.0;
    for (int i = knotIndex; i < knotIndex + degree; ++i)
        sum += knots[static_cast<size_t>(i)];
    return sum / degree;
}

// Ported from: KnotVector.baseKnotFractionToKnot（:319-323）。
double KnotVector::baseKnotFractionToKnot(int knotIndex0, double localFraction) const
{
    double const knot0 = knots[static_cast<size_t>(knotIndex0)];
    localFraction = clamp(localFraction, 0.0, 1.0);
    return knot0 + localFraction * (knots[static_cast<size_t>(knotIndex0) + 1] - knot0);
}

// Ported from: KnotVector.spanFractionToKnot（:325-329）。
double KnotVector::spanFractionToKnot(int spanIndex, double localFraction) const
{
    int const k = spanIndexToLeftKnotIndex(spanIndex);
    localFraction = clamp(localFraction, 0.0, 1.0);
    return knots[static_cast<size_t>(k)] + localFraction * (knots[static_cast<size_t>(k) + 1] - knots[static_cast<size_t>(k)]);
}

// Ported from: KnotVector.spanFractionToFraction（:331-334）。
double KnotVector::spanFractionToFraction(int spanIndex, double localFraction) const
{
    double const knot = spanFractionToKnot(spanIndex, localFraction);
    return (knot - leftKnot()) / (rightKnot() - leftKnot());
}

// Ported from: KnotVector.fractionToKnot（:336-339）。
double KnotVector::fractionToKnot(double fraction) const
{
    fraction = clamp(fraction, 0.0, 1.0);  // B-splines are not extendable
    return interpolate(knots[static_cast<size_t>(degree) - 1], fraction,
                       knots[knots.size() - static_cast<size_t>(degree)]);
}

// Ported from: KnotVector.evaluateBasisFunctions（:353-387——Mansfield-Cox-de Boor；
// 参考 isKnotInValidSpan 为 assert 调试面——DanQing 无异常，原样不落断言面）。
bool KnotVector::evaluateBasisFunctions(int knotIndex0, double u, std::vector<double>& f) const
{
    if (f.size() < static_cast<size_t>(degree) + 1)
        return false;
    f[0] = 1.0;
    if (degree < 1)
        return true;
    // direct compute for linear part
    double const u0 = knots[static_cast<size_t>(knotIndex0)];
    double const u1 = knots[static_cast<size_t>(knotIndex0) + 1];
    f[1] = (u - u0) / (u1 - u0);
    f[0] = 1.0 - f[1];
    if (degree < 2)
        return true;
    // each iteration of the outer loop evaluates the basis functions of degree depth+1
    // using one or two values of the basis functions of one less degree from the
    // preceding iteration
    for (int depth = 1; depth < degree; ++depth) {
        int kLeft = knotIndex0 - depth;
        int kRight = knotIndex0 + 1;
        double gCarry = 0.0;
        for (int step = 0; step <= depth; ++step) {
            double const tLeft = knots[static_cast<size_t>(kLeft++)];
            double const tRight = knots[static_cast<size_t>(kRight++)];
            double const fraction = (u - tLeft) / (tRight - tLeft);
            double const g1 = f[static_cast<size_t>(step)] * fraction;
            double const g0 = f[static_cast<size_t>(step)] * (1.0 - fraction);
            f[static_cast<size_t>(step)] = gCarry + g0;
            gCarry = g1;
        }
        f[static_cast<size_t>(depth) + 1] = gCarry;
        // at this point, the head of f[] contains the depth+2 values at u
        // of the basis functions of degree depth+1 with support over [u0,u1)
    }
    return true;
}

// Ported from: KnotVector.evaluateBasisFunctions1（:398-469——f/df/(ddf) 三数组）。
bool KnotVector::evaluateBasisFunctions1(int knotIndex0, double u, std::vector<double>& f,
                                         std::vector<double>& df, std::vector<double>* ddf) const
{
    if (f.size() < static_cast<size_t>(degree) + 1)
        return false;
    if (df.size() < static_cast<size_t>(degree) + 1)
        return false;
    if (ddf != nullptr && ddf->size() < static_cast<size_t>(degree) + 1)
        return false;
    f[0] = 1.0;
    df[0] = 0.0;
    if (degree < 1)
        return true;
    // direct compute for linear part
    double const u0 = knots[static_cast<size_t>(knotIndex0)];
    double const u1 = knots[static_cast<size_t>(knotIndex0) + 1];
    // ah = 1/(u1-u0) is the derivative of fraction0; -ah is the derivative of fraction1
    double ah = 1.0 / (u1 - u0);
    f[1] = (u - u0) * ah;
    f[0] = 1.0 - f[1];
    df[0] = -ah;
    df[1] = ah;
    if (ddf != nullptr) {  // first derivative started constant, second derivative started zero.
        (*ddf)[0] = 0.0;
        (*ddf)[1] = 0.0;
    }
    if (degree < 2)
        return true;
    for (int depth = 1; depth < degree; ++depth) {
        int kLeft = knotIndex0 - depth;
        int kRight = kLeft + depth + 1;
        double gCarry = 0.0;
        double dgCarry = 0.0;
        double ddgCarry = 0.0;
        // f, df, ddf, are each row vectors with product of `step` linear terms.
        // f is multiplied on the right by matrix V.  Each row has 2 nonzero entries
        // (which sum to 1) (0,0,1-fraction, fraction,0,0,0)
        //    Each row of the derivative dV is two entries (0,0, -1/h, 1/h,0,0,0)
        // Hence fnew = f * V
        //      dfnew = df * V + f * dV
        //      ddfnew = ddf * V + df*dV + df * dV + f * ddV
        // but ddV is zero so
        //      ddfnew = ddf * V + 2 * df * dV
        for (int step = 0; step <= depth; ++step) {
            double const tLeft = knots[static_cast<size_t>(kLeft++)];
            double const tRight = knots[static_cast<size_t>(kRight++)];
            ah = 1.0 / (tRight - tLeft);
            double const fraction = (u - tLeft) * ah;
            double const fraction1 = 1.0 - fraction;
            double const g1 = f[static_cast<size_t>(step)] * fraction;
            double const g0 = f[static_cast<size_t>(step)] * fraction1;
            double const dg1 = df[static_cast<size_t>(step)] * fraction + f[static_cast<size_t>(step)] * ah;
            double const dg0 = df[static_cast<size_t>(step)] * fraction1 - f[static_cast<size_t>(step)] * ah;
            double const dfSave = 2.0 * df[static_cast<size_t>(step)] * ah;
            f[static_cast<size_t>(step)] = gCarry + g0;
            df[static_cast<size_t>(step)] = dgCarry + dg0;
            gCarry = g1;
            dgCarry = dg1;
            if (ddf != nullptr) {  // do the backward reference to df before rewriting df
                double const ddg1 = (*ddf)[static_cast<size_t>(step)] * fraction + dfSave;
                double const ddg0 = (*ddf)[static_cast<size_t>(step)] * fraction1 - dfSave;
                (*ddf)[static_cast<size_t>(step)] = ddgCarry + ddg0;
                ddgCarry = ddg1;
            }
        }
        f[static_cast<size_t>(depth) + 1] = gCarry;
        df[static_cast<size_t>(depth) + 1] = dgCarry;
        if (ddf != nullptr)
            (*ddf)[static_cast<size_t>(depth) + 1] = ddgCarry;
    }
    return true;
}

// Ported from: KnotVector.knotToLeftKnotIndex（:475-486）。
int KnotVector::knotToLeftKnotIndex(double u) const
{
    for (int i = leftKnotIndex(); i < rightKnotIndex(); ++i) {
        if (u < knots[static_cast<size_t>(i) + 1])
            return i;
    }
    // for u >= rightKnot, return left index of last nontrivial span
    for (int i = rightKnotIndex(); i > leftKnotIndex(); --i) {
        if (knots[static_cast<size_t>(i)] - knots[static_cast<size_t>(i) - 1] >= KnotVector::knotTolerance)
            return i - 1;
    }
    return rightKnotIndex() - 1;  // shouldn't get here
}

// Ported from: KnotVector.spanIndexToSpanLength（:498-501）。
double KnotVector::spanIndexToSpanLength(int spanIndex) const
{
    int const k = spanIndexToLeftKnotIndex(spanIndex);
    return knots[static_cast<size_t>(k) + 1] - knots[static_cast<size_t>(k)];
}

// Ported from: KnotVector.isIndexOfRealSpan（:508-512）。
bool KnotVector::isIndexOfRealSpan(int spanIndex) const
{
    if (spanIndex >= 0 && spanIndex < numSpans())
        return !isSmallMetricDistance(spanIndexToSpanLength(spanIndex));
    return false;
}

// Ported from: KnotVector.reflectKnots（:514-521）。
void KnotVector::reflectKnots()
{
    double const a = leftKnot();
    double const b = rightKnot();
    size_t const numKnots = knots.size();
    for (size_t i = 0; i < numKnots; ++i)
        knots[i] = a + (b - knots[i]);
    std::reverse(knots.begin(), knots.end());
}

// Ported from: KnotVector.isAlmostEqual（:170-174）。
bool KnotVector::isAlmostEqual(KnotVector const& other) const
{
    if (degree != other.degree)
        return false;
    if (knots.size() != other.knots.size())
        return false;
    for (size_t i = 0; i < knots.size(); ++i) {
        if (std::fabs(knots[i] - other.knots[i]) >= KnotVector::knotTolerance)
            return false;
    }
    return true;
}

// Ported from: KnotVector.copyKnots（静态，:523-549）。
std::vector<double> KnotVector::copyKnots(std::vector<double> const& knotArray, int degree,
                                          bool includeExtraEndKnot, BSplineWrapMode wrapMode)
{
    bool const isExtraEndKnotPeriodic =
        (includeExtraEndKnot && wrapMode == BSplineWrapMode::OpenByAddingControlPoints);
    size_t const leftIndex = static_cast<size_t>(degree) - 1;
    size_t const rightIndex = knotArray.size() - static_cast<size_t>(degree);
    double const a0 = knotArray[leftIndex];
    double const a1 = knotArray[rightIndex];
    double const delta = a1 - a0;
    std::vector<double> values;
    if (includeExtraEndKnot) {
        if (isExtraEndKnotPeriodic)
            values.push_back(knotArray[rightIndex - static_cast<size_t>(degree)] - delta);
        else
            values.push_back(knotArray[0]);
    }
    for (double u : knotArray)
        values.push_back(u);
    if (includeExtraEndKnot) {
        if (isExtraEndKnotPeriodic)
            values.push_back(knotArray[leftIndex + static_cast<size_t>(degree)] + delta);
        else
            values.push_back(knotArray[knotArray.size() - 1]);
    }
    return values;
}

END_DQ_GEOM_NAMESPACE
