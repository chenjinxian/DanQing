// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/ThematicUniforms.ts
//
// Maintains state for uniforms related to thematic display (height maps, slopes, etc.).
//
// NOTE: The reference implementation uses ThematicSensors, TextureHandle.createForImageBuffer,
// Gradient.Symb.createThematic, and target.plan.thematic. This is a simplified version that
// stores the settings but defers texture creation and sensor management.
//
// Gradient texture creation is implemented via GradientSymb.produceImage.
// ThematicSensors is available in ThematicSensors.h.
// View matrix transforms for axis/sun direction are implemented.
#pragma once

#include "UniformHandle.h"
#include "FloatRGBA.h"
#include "TextureHandle.h"
#include "ThematicSensors.h"

#include <dqCommon/ThematicDisplay.h>
#include <dqCommon/Gradient.h>
#include <dqCommon/Image.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

/// Default gradient dimension for thematic textures.
/// Ported from: itwinjs-core ThematicUniforms._getGradientDimension()
constexpr int kDefaultGradientDimension = 8192;

// ---------------------------------------------------------------------------
// ThematicUniforms — thematic display uniform handler
// Ported from: itwinjs-core ThematicUniforms
//
// Handles thematic rendering settings: display mode, range, axis, sun direction,
// margin color, gradient settings.
//
// Gradient texture creation is implemented.
// ThematicSensors is available.
// View matrix transforms are implemented.
// ---------------------------------------------------------------------------
class ThematicUniforms {
public:
    ThematicUniforms() = default;

    /// Get the current thematic display settings.
    dqCommon::ThematicDisplay const* getThematicDisplay() const noexcept
    {
        return m_thematicDisplay ? &m_thematicDisplay.value() : nullptr;
    }

    /// Check if iso lines are wanted.
    /// Ported from: itwinjs-core ThematicUniforms.wantIsoLines
    bool wantIsoLines() const noexcept
    {
        return m_thematicDisplay &&
               m_thematicDisplay->displayMode == dqCommon::ThematicDisplayMode::Height &&
               m_thematicDisplay->gradientSettings.mode == dqCommon::ThematicGradientMode::IsoLines;
    }

    /// Check if slope mode is wanted.
    /// Ported from: itwinjs-core ThematicUniforms.wantSlopeMode
    bool wantSlopeMode() const noexcept
    {
        return m_thematicDisplay &&
               m_thematicDisplay->displayMode == dqCommon::ThematicDisplayMode::Slope;
    }

    /// Check if hill shade mode is wanted.
    /// Ported from: itwinjs-core ThematicUniforms.wantHillShadeMode
    bool wantHillShadeMode() const noexcept
    {
        return m_thematicDisplay &&
               m_thematicDisplay->displayMode == dqCommon::ThematicDisplayMode::HillShade;
    }

    /// Update thematic uniforms from settings.
    /// Ported from: itwinjs-core ThematicUniforms.update()
    /// @param thematic The thematic display settings.
    /// @param driver Optional driver for texture creation.
    /// @param viewMatrix Optional 4x4 view matrix for transforming axis/sun direction.
    void update(dqCommon::ThematicDisplay const& thematic,
                rhi::Driver* driver = nullptr,
                float const* viewMatrix = nullptr)
    {
        if (m_thematicDisplay && m_thematicDisplay->equals(thematic))
            return;

        m_thematicDisplay = thematic;

        // Store settings for later use when full infrastructure is in place.
        if (thematic.displayMode == dqCommon::ThematicDisplayMode::Slope) {
            // Convert range to radians for slope mode.
            m_range[0] = static_cast<float>(thematic.rangeMin * M_PI / 180.0);
            m_range[1] = static_cast<float>(thematic.rangeMax * M_PI / 180.0);
        } else {
            m_range[0] = static_cast<float>(thematic.rangeMin);
            m_range[1] = static_cast<float>(thematic.rangeMax);
        }

        m_colorMix = static_cast<float>(thematic.gradientSettings.colorMix);

        // Transform axis by viewMatrix (rotate only, no translation).
        // Ported from: itwinjs-core ThematicUniforms.update() axis transform
        double ax = thematic.axis.x, ay = thematic.axis.y, az = thematic.axis.z;
        if (viewMatrix) {
            // Apply 3x3 rotation part of view matrix to axis
            double nx = viewMatrix[0]*ax + viewMatrix[4]*ay + viewMatrix[8]*az;
            double ny = viewMatrix[1]*ax + viewMatrix[5]*ay + viewMatrix[9]*az;
            double nz = viewMatrix[2]*ax + viewMatrix[6]*ay + viewMatrix[10]*az;
            ax = nx; ay = ny; az = nz;
        }
        m_axis[0] = static_cast<float>(ax);
        m_axis[1] = static_cast<float>(ay);
        m_axis[2] = static_cast<float>(az);

        // Transform sun direction by viewMatrix (rotate only, no translation).
        // Ported from: itwinjs-core ThematicUniforms.update() sunDirection transform
        double sx = thematic.sunDirection.x, sy = thematic.sunDirection.y, sz = thematic.sunDirection.z;
        if (viewMatrix) {
            double nx = viewMatrix[0]*sx + viewMatrix[4]*sy + viewMatrix[8]*sz;
            double ny = viewMatrix[1]*sx + viewMatrix[5]*sy + viewMatrix[9]*sz;
            double nz = viewMatrix[2]*sx + viewMatrix[6]*sy + viewMatrix[10]*sz;
            sx = nx; sy = ny; sz = nz;
        }
        m_sunDirection[0] = static_cast<float>(sx);
        m_sunDirection[1] = static_cast<float>(sy);
        m_sunDirection[2] = static_cast<float>(sz);

        m_marginColor = FloatRgba::fromHex(
            thematic.gradientSettings.marginColor.getRgb(),
            static_cast<uint8_t>(thematic.gradientSettings.marginColor.getAlpha()));

        m_displayMode[0] = static_cast<float>(thematic.displayMode);

        m_fragSettings[0] = static_cast<float>(thematic.gradientSettings.mode);

        m_fragSettings[1] = static_cast<float>(thematic.sensorSettings.distanceCutoff);

        m_fragSettings[2] = static_cast<float>(
            std::min(thematic.gradientSettings.stepCount, kDefaultGradientDimension));

        m_fragSettings[3] = (thematic.gradientSettings.transparencyMode ==
            dqCommon::ThematicGradientTransparencyMode::SurfaceOnly) ? 0.0f : 1.0f;

        // Create gradient texture from thematic display settings.
        // Ported from: itwinjs-core ThematicUniforms.update() line 150-151
        if (driver) createGradientTexture(*driver);
    }

    /// Bind range uniform (vec2).
    /// Ported from: itwinjs-core ThematicUniforms.bindRange()
    void bindRange(UniformHandle& uniform) const
    {
        uniform.setUniform2fv(m_range.data());
    }

    /// Bind axis uniform (vec3).
    /// Ported from: itwinjs-core ThematicUniforms.bindAxis()
    void bindAxis(UniformHandle& uniform) const
    {
        uniform.setUniform3fv(m_axis.data());
    }

    /// Bind sun direction uniform (vec3).
    /// Ported from: itwinjs-core ThematicUniforms.bindSunDirection()
    void bindSunDirection(UniformHandle& uniform) const
    {
        uniform.setUniform3fv(m_sunDirection.data());
    }

    /// Bind margin color uniform (vec4).
    /// Ported from: itwinjs-core ThematicUniforms.bindMarginColor()
    void bindMarginColor(UniformHandle& uniform) const
    {
        float v[4] = {m_marginColor.r, m_marginColor.g, m_marginColor.b, m_marginColor.a};
        uniform.setUniform4fv(v);
    }

    /// Bind display mode uniform (int).
    /// Ported from: itwinjs-core ThematicUniforms.bindDisplayMode()
    void bindDisplayMode(UniformHandle& uniform) const
    {
        uniform.setUniform1fv(m_displayMode.data());
    }

    /// Bind fragment settings uniform (vec4).
    /// Ported from: itwinjs-core ThematicUniforms.bindFragSettings()
    void bindFragSettings(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_fragSettings.data());
    }

    /// Check if the uniform is disposed (always true — no GPU resources in simplified version).
    /// Ported from: itwinjs-core ThematicUniforms.isDisposed
    bool isDisposed() const noexcept { return true; }

    /// Dispose — release GPU resources.
    /// Ported from: itwinjs-core ThematicUniforms.dispose()
    void dispose() {
        m_texture = {};
    }

    /// Bind gradient texture to a texture unit.
    /// Ported from: itwinjs-core ThematicUniforms.bindGradientTexture()
    void bindGradientTexture(rhi::Driver& driver, uint32_t unit) const {
        if (m_texture.isValid()) {
            driver.bindTexture(unit, m_texture.getRhiHandle());
        }
    }

    /// Get the gradient texture handle.
    rhi::TextureHandle getGradientTexture() const noexcept { return m_texture.getRhiHandle(); }

    /// Get the number of sensors.
    int getNumSensors() const noexcept { return m_numSensors; }

private:
    /// Create gradient texture from thematic display settings.
    /// Ported from: itwinjs-core ThematicUniforms.update() line 150-151
    void createGradientTexture(rhi::Driver& driver) {
        if (!m_thematicDisplay.has_value()) return;

        // Create gradient symb from thematic settings.
        // Ported from: itwinjs-core GradientSymb.createThematic()
        // This correctly sets GradientMode::Thematic and populates key colors
        // from the color scheme. The ThematicGradientMode (Smooth/Stepped/IsoLines)
        // is stored in m_fragSettings[0] for the shader, not in the GradientMode.
        dqCommon::GradientSymb symb = dqCommon::GradientSymb::createThematic(
            m_thematicDisplay->gradientSettings);

        // Produce gradient image
        dqCommon::ProduceImageArgs imgArgs;
        imgArgs.width = m_gradientDimension;
        imgArgs.height = 1;

        auto image = symb.produceImage(imgArgs);
        if (!image.has_value()) return;

        // Create GPU texture from image buffer
        // ImageBuffer: data is width * height * bytesPerPixel
        auto const& imgBuffer = image.value();
        int bytesPerPixel = (imgBuffer.format == dqCommon::ImageBufferFormat::Rgba) ? 4 : 3;
        int height = static_cast<int>(imgBuffer.data.size() / (imgBuffer.width * bytesPerPixel));
        if (height <= 0) height = 1;

        m_texture = TextureHandle::create2D(
            driver,
            static_cast<uint32_t>(imgBuffer.width),
            static_cast<uint32_t>(height),
            rhi::TextureFormat::RGBA8,
            imgBuffer.data.data(),
            static_cast<uint32_t>(imgBuffer.data.size()));
    }

    std::optional<dqCommon::ThematicDisplay> m_thematicDisplay;

    // CPU state
    std::array<float, 2> m_range{};
    float m_colorMix = 0.0f;
    std::array<float, 3> m_axis{};
    std::array<float, 3> m_sunDirection{};
    FloatRgba m_marginColor;
    std::array<float, 1> m_displayMode{};
    std::array<float, 4> m_fragSettings{};

    // GPU resources
    TextureHandle m_texture;
    int m_numSensors = 0;
    int m_gradientDimension = kDefaultGradientDimension;
};

END_DQ_RENDER_NAMESPACE
