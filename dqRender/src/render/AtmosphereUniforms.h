// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Atmosphere uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/AtmosphereUniforms.ts
//
// Maintains state for uniforms related to atmospheric scattering rendering.
// Uses Matrix3d for ellipsoid transforms and FrustumUniforms for view matrix.
#pragma once

#include "UniformHandle.h"
#include "Matrix.h"

#include <dqCommon/Atmosphere.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

using dqGeom::Matrix3d;
using dqGeom::Point3d;
using dqGeom::Transform;

/// Maximum number of sample points for in-scattering and out-scattering computations.
/// Ported from: itwinjs-core AtmosphereUniforms.MAX_SAMPLE_POINTS
constexpr int kMaxSamplePoints = 40;

// ---------------------------------------------------------------------------
// RenderPlanEllipsoid — ellipsoid model for atmosphere rendering
// Ported from: itwinjs-core RenderPlanEllipsoid
// ---------------------------------------------------------------------------
struct RenderPlanEllipsoid {
    Point3d ellipsoidCenter = Point3d::FromZero();
    Matrix3d ellipsoidRotation = Matrix3d::CreateIdentity();
    Point3d ellipsoidRadii = Point3d::From(6378137.0, 6378137.0, 6356752.3142);  // WGS84

    bool equals(RenderPlanEllipsoid const& other) const {
        return ellipsoidCenter.AlmostEqual(other.ellipsoidCenter)
            && ellipsoidRotation.IsAlmostEqual(other.ellipsoidRotation)
            && ellipsoidRadii.AlmostEqual(other.ellipsoidRadii);
    }
};

// ---------------------------------------------------------------------------
// AtmosphereUniforms — atmospheric scattering uniform handler
// Ported from: itwinjs-core AtmosphereUniforms
//
// Handles atmosphere rendering settings: scale matrices, density falloff,
// scattering coefficients, exposure. The full implementation uses Matrix3d
// for ellipsoid transforms and FrustumUniforms for view matrix.
// ---------------------------------------------------------------------------
class AtmosphereUniforms {
public:
    AtmosphereUniforms() = default;

    /// Update atmosphere uniforms from settings and ellipsoid.
    /// Ported from: itwinjs-core AtmosphereUniforms.update()
    void update(dqCommon::Atmosphere::Settings const& atmosphere,
                RenderPlanEllipsoid const* ellipsoid = nullptr,
                float const* viewMatrix = nullptr)
    {
        bool atmosphereChanged = !m_atmosphere || !m_atmosphere->equals(atmosphere);
        bool ellipsoidChanged = ellipsoid && (!m_ellipsoid || !m_ellipsoid->equals(*ellipsoid));

        if (!atmosphereChanged && !ellipsoidChanged)
            return;

        m_atmosphere = atmosphere;
        if (ellipsoid) m_ellipsoid = *ellipsoid;

        m_exposure = static_cast<float>(atmosphere.exposure);

        // Update earth scale matrix from ellipsoid radii
        if (ellipsoid) {
            updateEarthScaleMatrix(ellipsoid->ellipsoidRadii);
            updateAtmosphereScaleMatrix(atmosphere.atmosphereHeightAboveEarth);
            updateInverseEllipsoidRotationMatrix(ellipsoid->ellipsoidRotation, viewMatrix);
            updateEarthCenter(ellipsoid->ellipsoidCenter, viewMatrix);
        }

        updateAtmosphereRadiusScaleFactor(atmosphere.atmosphereHeightAboveEarth);
        updateAtmosphereMaxDensityThresholdScaleFactor(atmosphere.depthBelowEarthForMaxDensity);
        updateDensityFalloff(atmosphere.densityFalloff);
        updateNumViewRaySamples(atmosphere.numViewRaySamples);
        updateNumSunRaySamples(atmosphere.numSunRaySamples);
        updateScatteringCoefficients(atmosphere.scatteringStrength, atmosphere.wavelengths);
    }

    /// Bind exposure uniform.
    /// Ported from: itwinjs-core AtmosphereUniforms.bindExposure()
    void bindExposure(UniformHandle& uniform) const
    {
        uniform.setUniform1f(m_exposure);
    }

    /// Bind atmosphere data uniform (mat4).
    /// Ported from: itwinjs-core AtmosphereUniforms.atmosphereData getter
    void bindAtmosphereData(UniformHandle& uniform) const
    {
        uniform.setMatrix4(m_atmosphereData.data);
    }

    /// Bind inverse rotation * inverse earth scale matrix (mat3).
    /// Ported from: itwinjs-core AtmosphereUniforms.bindInverseRotationInverseEarthScaleMatrix()
    void bindInverseRotationInverseEarthScaleMatrix(UniformHandle& uniform) const
    {
        Matrix3d result;
        m_earthScaleMatrix.MultiplyMatrixInverseMatrix(m_inverseEllipsoidRotationMatrix, result);
        setMatrix3FromDouble(uniform, result);
    }

    /// Bind inverse rotation * inverse atmosphere scale matrix (mat3).
    /// Ported from: itwinjs-core AtmosphereUniforms.bindInverseRotationInverseAtmosphereScaleMatrix()
    void bindInverseRotationInverseAtmosphereScaleMatrix(UniformHandle& uniform) const
    {
        Matrix3d result;
        m_atmosphereScaleMatrix.MultiplyMatrixInverseMatrix(m_inverseEllipsoidRotationMatrix, result);
        setMatrix3FromDouble(uniform, result);
    }

    /// Bind earth scale matrix (mat3).
    /// Ported from: itwinjs-core AtmosphereUniforms.bindEarthScaleMatrix()
    void bindEarthScaleMatrix(UniformHandle& uniform) const
    {
        setMatrix3FromDouble(uniform, m_earthScaleMatrix);
    }

    /// Bind atmosphere scale matrix (mat3).
    /// Ported from: itwinjs-core AtmosphereUniforms.bindAtmosphereScaleMatrix()
    void bindAtmosphereScaleMatrix(UniformHandle& uniform) const
    {
        setMatrix3FromDouble(uniform, m_atmosphereScaleMatrix);
    }

    /// Bind inverse earth scale matrix (mat3).
    /// Ported from: itwinjs-core AtmosphereUniforms.bindInverseEarthScaleMatrix()
    void bindInverseEarthScaleMatrix(UniformHandle& uniform) const
    {
        Matrix3d inv;
        if (m_earthScaleMatrix.Inverse(inv))
            setMatrix3FromDouble(uniform, inv);
    }

    /// Bind inverse atmosphere scale matrix (mat3).
    /// Ported from: itwinjs-core AtmosphereUniforms.bindInverseAtmosphereScaleMatrix()
    void bindInverseAtmosphereScaleMatrix(UniformHandle& uniform) const
    {
        Matrix3d inv;
        if (m_atmosphereScaleMatrix.Inverse(inv))
            setMatrix3FromDouble(uniform, inv);
    }

    /// Helper: set mat3 uniform from double-precision Matrix3d.
    static void setMatrix3FromDouble(UniformHandle& uniform, Matrix3d const& m)
    {
        float f[9];
        for (int i = 0; i < 9; ++i) f[i] = static_cast<float>(m.coffs[i]);
        uniform.setMatrix3(f);
    }

    /// Get the atmosphere data matrix.
    Matrix4 const& getAtmosphereData() const noexcept { return m_atmosphereData; }

    /// Get the current atmosphere settings.
    dqCommon::Atmosphere::Settings const* getAtmosphere() const noexcept { return m_atmosphere ? &m_atmosphere.value() : nullptr; }

    /// Check if the uniform is disposed (always true — no GPU resources).
    bool isDisposed() const noexcept { return true; }

    /// dispose (no-op — no GPU resources).
    void dispose() {}

private:
    /// Update earth scale matrix from ellipsoid radii.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateEarthScaleMatrix()
    void updateEarthScaleMatrix(Point3d const& earthRadii)
    {
        m_earthScaleMatrix = Matrix3d::CreateScale(earthRadii.x, earthRadii.y, earthRadii.z);
    }

    /// Update atmosphere scale matrix.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateAtmosphereScaleMatrix()
    void updateAtmosphereScaleMatrix(double heightAboveSurface)
    {
        double earthPolarRadius = m_earthScaleMatrix.at(2, 2);
        double scaleFactor = (earthPolarRadius == 0) ? 1.0 : (earthPolarRadius + heightAboveSurface) / earthPolarRadius;
        m_earthScaleMatrix.Scale(scaleFactor, m_atmosphereScaleMatrix);
    }

    /// Update inverse ellipsoid rotation matrix.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateInverseEllipsoidRotationMatrix()
    void updateInverseEllipsoidRotationMatrix(Matrix3d const& ellipsoidRotation, float const* viewMatrix)
    {
        if (viewMatrix) {
            // Extract 3x3 rotation from 4x4 view matrix
            Matrix3d viewRotation(
                viewMatrix[0], viewMatrix[1], viewMatrix[2],
                viewMatrix[4], viewMatrix[5], viewMatrix[6],
                viewMatrix[8], viewMatrix[9], viewMatrix[10]);
            Matrix3d viewRotationInv;
            if (viewRotation.Inverse(viewRotationInv))
                ellipsoidRotation.MultiplyMatrixInverseMatrix(viewRotationInv, m_inverseEllipsoidRotationMatrix);
            else
                m_inverseEllipsoidRotationMatrix = ellipsoidRotation;
        } else {
            m_inverseEllipsoidRotationMatrix = ellipsoidRotation;
        }
    }

    /// Update earth center in view space.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateEarthCenter()
    void updateEarthCenter(Point3d const& earthCenter, float const* viewMatrix)
    {
        if (viewMatrix) {
            // Transform earth center by view matrix
            double x = earthCenter.x, y = earthCenter.y, z = earthCenter.z;
            m_atmosphereData.data[8] = static_cast<float>(viewMatrix[0]*x + viewMatrix[1]*y + viewMatrix[2]*z + viewMatrix[3]);
            m_atmosphereData.data[9] = static_cast<float>(viewMatrix[4]*x + viewMatrix[5]*y + viewMatrix[6]*z + viewMatrix[7]);
            m_atmosphereData.data[10] = static_cast<float>(viewMatrix[8]*x + viewMatrix[9]*y + viewMatrix[10]*z + viewMatrix[11]);
        } else {
            m_atmosphereData.data[8] = static_cast<float>(earthCenter.x);
            m_atmosphereData.data[9] = static_cast<float>(earthCenter.y);
            m_atmosphereData.data[10] = static_cast<float>(earthCenter.z);
        }
    }

    /// Update atmosphere radius scale factor.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateAtmosphereRadiusScaleFactor()
    void updateAtmosphereRadiusScaleFactor(double atmosphereHeightAboveEarth)
    {
        double earthPolarRadius = m_earthScaleMatrix.at(2, 2);
        double minDensityThresholdRadius = earthPolarRadius + atmosphereHeightAboveEarth;
        double atmosphereRadiusScaleFactor = (earthPolarRadius == 0)
            ? 1.0
            : (minDensityThresholdRadius / earthPolarRadius);
        m_atmosphereData.data[0] = static_cast<float>(atmosphereRadiusScaleFactor);
    }

    /// Update atmosphere max density threshold scale factor.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateAtmosphereMaxDensityThresholdScaleFactor()
    void updateAtmosphereMaxDensityThresholdScaleFactor(double maxDensityDepthBelowEarth)
    {
        double earthPolarRadius = m_earthScaleMatrix.at(2, 2);
        double maxDensityThresholdRadius = earthPolarRadius - maxDensityDepthBelowEarth;
        double atmosphereMaxDensityThresholdScaleFactor = (earthPolarRadius == 0)
            ? 1.0
            : (maxDensityThresholdRadius / earthPolarRadius);
        m_atmosphereData.data[1] = static_cast<float>(atmosphereMaxDensityThresholdScaleFactor);
    }

    /// Update density falloff.
    void updateDensityFalloff(double densityFalloff)
    {
        m_atmosphereData.data[2] = static_cast<float>(densityFalloff);
    }

    /// Update number of view ray samples.
    void updateNumViewRaySamples(int numViewRaySamples)
    {
        numViewRaySamples = std::max(0, std::min(kMaxSamplePoints, numViewRaySamples));
        m_atmosphereData.data[4] = static_cast<float>(numViewRaySamples);
    }

    /// Update number of sun ray samples.
    void updateNumSunRaySamples(int numSunRaySamples)
    {
        numSunRaySamples = std::max(0, std::min(kMaxSamplePoints, numSunRaySamples));
        m_atmosphereData.data[5] = static_cast<float>(numSunRaySamples);
    }

    /// Update scattering coefficients.
    /// Ported from: itwinjs-core AtmosphereUniforms._updateScatteringCoefficients()
    void updateScatteringCoefficients(double scatteringStrength, dqCommon::Atmosphere::Wavelengths const& wavelengths)
    {
        constexpr double violetLightWavelength = 400.0;
        m_atmosphereData.data[12] = static_cast<float>(std::pow(violetLightWavelength / wavelengths.r, 4.0) * scatteringStrength);
        m_atmosphereData.data[13] = static_cast<float>(std::pow(violetLightWavelength / wavelengths.g, 4.0) * scatteringStrength);
        m_atmosphereData.data[14] = static_cast<float>(std::pow(violetLightWavelength / wavelengths.b, 4.0) * scatteringStrength);
    }

    std::optional<dqCommon::Atmosphere::Settings> m_atmosphere;
    std::optional<RenderPlanEllipsoid> m_ellipsoid;

    // Main shader uniforms
    Matrix4 m_atmosphereData;

    // Ellipsoid matrices (Ported from: itwinjs-core AtmosphereUniforms)
    Matrix3d m_earthScaleMatrix = Matrix3d::CreateIdentity();
    Matrix3d m_inverseEllipsoidRotationMatrix = Matrix3d::CreateIdentity();
    Matrix3d m_atmosphereScaleMatrix = Matrix3d::CreateIdentity();

    // Fragment shader uniforms
    float m_exposure = 0.0f;
};

END_DQ_RENDER_NAMESPACE
