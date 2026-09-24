// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Lighting uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/LightingUniforms.ts
//
// Packs a DisplayStyle3d LightSettings into a single 16-float array for GPU
// upload. Solar direction is handled separately (in TargetUniforms).
#pragma once

#include "FloatRGBA.h"
#include "UniformHandle.h"

#include <dqCommon/LightSettings.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// LightingUniforms — LightSettings uniform packing
// Ported from: itwinjs-core LightingUniforms
//
// Single float array laid out as:
//   0  float solar intensity
//   1  vec3 ambient color
//   4  float ambient intensity
//   5  vec3 hemisphere lower color
//   8  vec3 hemisphere upper color
//  11  float hemisphere intensity
//  12  float portrait intensity
//  13  float specular intensity
//  14  float num cels
//  15  fresnel intensity (negative if fresnel inverted)
// ---------------------------------------------------------------------------
class LightingUniforms {
public:
    LightingUniforms() = default;

    /// Update from new light settings.
    /// Ported from: itwinjs-core LightingUniforms.update()
    void update(dqCommon::LightSettings const& settings) noexcept
    {
        if (m_initialized && settings.equals(m_settings))
            return;

        m_initialized = true;
        m_settings = settings;

        m_data[0] = static_cast<float>(settings.solar.intensity);

        setRgb(settings.ambient.color, 1);
        m_data[4] = static_cast<float>(settings.ambient.intensity);

        setRgb(settings.hemisphere.lowerColor, 5);
        setRgb(settings.hemisphere.upperColor, 8);
        m_data[11] = static_cast<float>(settings.hemisphere.intensity);

        m_data[12] = static_cast<float>(settings.portraitIntensity);
        m_data[13] = static_cast<float>(settings.specularIntensity);
        m_data[14] = static_cast<float>(settings.numCels);

        // Ported from: itwinjs-core LightingUniforms.update() fresnel block
        double fresnel = settings.fresnel.intensity;
        m_data[15] = settings.fresnel.invert ? static_cast<float>(-fresnel) : static_cast<float>(fresnel);
    }

    /// Bind the 16-float lighting array.
    /// Ported from: itwinjs-core LightingUniforms.bind()
    void bind(UniformHandle& uniform) const noexcept
    {
        uniform.setUniform1fv(m_data, 16);
    }

    float const* getData() const noexcept { return m_data; }
    dqCommon::LightSettings const& getSettings() const noexcept { return m_settings; }

private:
    // Ported from: itwinjs-core LightingUniforms.setRgb()
    void setRgb(dqCommon::RgbColor const& rgb, int index) noexcept
    {
        FloatRgb f;
        f.setRgbColor(rgb);
        m_data[index + 0] = f.red();
        m_data[index + 1] = f.green();
        m_data[index + 2] = f.blue();
    }

    bool m_initialized = false;
    dqCommon::LightSettings m_settings;
    float m_data[16] = {};
};

END_DQ_RENDER_NAMESPACE
