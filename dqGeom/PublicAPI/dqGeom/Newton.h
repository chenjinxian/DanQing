// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Newton iterators
//
// Ported from: itwinjs-core core/geometry/src/numerics/Newton.ts
//
// Subset: AbstractNewtonIterator + NewtonEvaluatorRRtoRRD +
// Newton2dUnboundedWithDerivative — the 2d Newton used by
// Ellipsoid.projectPointToSurface (EllipsoidClosestPoint). The 1d iterators and
// approximate-derivative variants are TODO.
#pragma once

#include "Export.h"
#include "Geometry.h"
#include "Plane3dByOriginAndVectors.h"
#include "Point2d.h"
#include "SmallSystem.h"

#include <optional>

namespace dqGeom {

/// Base class for Newton iterations in various dimensions.
/// Ported from: itwinjs-core AbstractNewtonIterator (Newton.ts:33-135)
class DQ_GEOM_EXPORT AbstractNewtonIterator {
public:
    virtual ~AbstractNewtonIterator() = default;
    /// Compute a step. The current x and function values must be retained for use in later method calls.
    virtual bool computeStep() = 0;
    /// Return the current step size, scaled for use in tolerance tests.
    virtual double currentStepSize() = 0;
    /// Apply the current step (in all dimensions).
    virtual void applyCurrentStep(bool isFinalStep) = 0;

    /// Number of iterations (incremented at each step).
    int numIterations = 0;

    /// Test if a step is converged.
    /// Ported from: AbstractNewtonIterator.testConvergence (Newton.ts:103-112)
    bool testConvergence(double delta) {
        // Reference guards cacheCandidate/restoreCandidate calls on the optional
        // methods being implemented; in C++ they are always callable (empty default),
        // so the calls are unconditional — identical behavior for both cases.
        if (delta < m_leastDelta && numIterations > 0.5 * m_maxIterations) {
            m_leastDelta = delta;
            cacheCandidate();
        }
        if (std::abs(delta) < m_stepSizeTolerance) {
            m_numAccepted++;
            return m_numAccepted >= m_successiveConvergenceTarget;
        }
        m_numAccepted = 0;
        return false;
    }

    /// Run iterations.
    /// Ported from: AbstractNewtonIterator.runIterations (Newton.ts:120-134)
    bool runIterations() {
        m_numAccepted = 0;
        numIterations = 0;
        m_leastDelta = 1.7976931348623157e308;  // Number.MAX_VALUE
        while (numIterations++ < m_maxIterations && computeStep()) {
            if (testConvergence(currentStepSize())) {
                applyCurrentStep(true);
                return true;
            }
            applyCurrentStep(false);
        }
        if (numIterations >= m_maxIterations && std::abs(currentStepSize()) > m_leastDelta)
            restoreCandidate();  // we may have ended up in a late cycle; return our best guess
        return false;
    }

protected:
    /// Ported from: AbstractNewtonIterator constructor (Newton.ts:57-68).
    /// Defaults: stepSizeTolerance = Geometry.smallNewtonStep,
    /// successiveConvergenceTarget = 2, maxIterations = 15.
    explicit AbstractNewtonIterator(
        double stepSizeTolerance = kSmallNewtonStep,
        int successiveConvergenceTarget = 2,
        int maxIterations = 15) noexcept
        : m_stepSizeTolerance(stepSizeTolerance)
        , m_successiveConvergenceTarget(successiveConvergenceTarget)
        , m_maxIterations(maxIterations) {}

    /// The current late iterate has the least delta encountered. Remember it.
    /// (Reference: optional protected method; empty default = "not implemented".)
    virtual void cacheCandidate() {}
    /// Set Newton result to the cached candidate.
    virtual void restoreCandidate() {}

    int m_numAccepted = 0;
    int m_successiveConvergenceTarget;
    double m_stepSizeTolerance;
    int m_maxIterations;
    double m_leastDelta = 1.7976931348623157e308;
};

/// Object to evaluate a 2-parameter newton function with derivatives.
/// Ported from: itwinjs-core NewtonEvaluatorRRtoRRD (Newton.ts:307-329)
class DQ_GEOM_EXPORT NewtonEvaluatorRRtoRRD {
public:
    virtual ~NewtonEvaluatorRRtoRRD() = default;
    /// Evaluate the function and its two partial derivatives; on true return,
    /// currentF must be set.
    virtual bool evaluate(double x, double y) = 0;

    /// Most recent function evaluation as parts of the plane:
    ///   origin  = F(X) = (x(X), y(X))
    ///   vectorU = 1st column of the Jacobian at X
    ///   vectorV = 2nd column of the Jacobian at X
    Plane3dByOriginAndVectors currentF;

    NewtonEvaluatorRRtoRRD() : currentF(Plane3dByOriginAndVectors::createXYPlane()) {}
};

/// Newton iteration in 2 dimensions with derivatives: X_{n+1} = X_n - JInv(X_n) F(X_n).
/// Ported from: itwinjs-core Newton2dUnboundedWithDerivative (Newton.ts:342-414)
class DQ_GEOM_EXPORT Newton2dUnboundedWithDerivative : public AbstractNewtonIterator {
public:
    /// Ported from: constructor (Newton.ts:353-359).
    explicit Newton2dUnboundedWithDerivative(
        NewtonEvaluatorRRtoRRD& func,
        std::optional<int> maxIterations = std::nullopt,
        std::optional<double> stepSizeTolerance = std::nullopt)
        : AbstractNewtonIterator(
              stepSizeTolerance.value_or(kSmallNewtonStep),
              2,
              maxIterations.value_or(15))
        , m_func(func) {}

    /// Set the current uv parameters, i.e., X_n = (u_n, v_n).
    void setUV(double u, double v) noexcept { m_currentUV = Point2d::From(u, v); }
    double getU() const noexcept { return m_currentUV.x; }
    double getV() const noexcept { return m_currentUV.y; }

    /// X_{n+1} := X_n - dX = (u_n - du, v_n - dv).
    /// Ported from: applyCurrentStep (Newton.ts:370-372)
    void applyCurrentStep(bool /*isFinalStep*/) override {
        setUV(m_currentUV.x - m_currentStep.x, m_currentUV.y - m_currentStep.y);
    }

    /// Evaluate the function and Jacobian at X_n and solve J dX = F for dX.
    /// Ported from: computeStep (Newton.ts:377-390)
    bool computeStep() override {
        if (!m_func.evaluate(m_currentUV.x, m_currentUV.y))
            return false;
        Plane3dByOriginAndVectors const& fA = m_func.currentF;
        Vector3d const& jCol0 = fA.vectorU;
        Vector3d const& jCol1 = fA.vectorV;
        Point3d const& fX = fA.origin;
        // Solve J(X_n) dX = F(X_n) for dX; X_{n+1} = X_n - dX.
        return SmallSystem::linearSystem2d(
            jCol0.x, jCol1.x, jCol0.y, jCol1.y, fX.x, fX.y, m_currentStep);
    }

    /// Current relative step size: larger absolute component of dX / (1 + |X_n|).
    /// Ported from: currentStepSize (Newton.ts:395-400)
    double currentStepSize() override {
        return maxAbsXY(
            m_currentStep.x / (1.0 + std::abs(m_currentUV.x)),
            m_currentStep.y / (1.0 + std::abs(m_currentUV.y)));
    }

protected:
    /// Ported from: cacheCandidate (Newton.ts:403-409)
    void cacheCandidate() override { m_cachedUV = m_currentUV; }
    /// Ported from: restoreCandidate (Newton.ts:411-414)
    void restoreCandidate() override {
        if (m_cachedUV.has_value())
            setUV(m_cachedUV->x, m_cachedUV->y);
    }

private:
    NewtonEvaluatorRRtoRRD& m_func;
    Point2d m_currentStep;   // dX = (du, dv); §3.4: Vector2d → Point2d (see SmallSystem.h)
    Point2d m_currentUV;     // X_n = (u_n, v_n)
    std::optional<Point2d> m_cachedUV;
};

} // namespace dqGeom
