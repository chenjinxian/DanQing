// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Reality model uniforms
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RealityModelUniforms.ts
//
// Maintains state for uniforms related to reality model and point cloud display.
//
// NOTE: The reference uses SyncTarget/SyncObserver for change detection.
// The current dqRender uniform pattern does not use sync (uniforms are uploaded
// every frame). The sync optimization can be added later when UniformHandle
// gains SyncObserver support.
//
// updateRange() is implemented in RealityModelUniforms.cpp using FrustumUniforms.
#pragma once

#include "UniformHandle.h"

#include <dqCommon/RealityModelDisplaySettings.h>
#include <dqGeom/Range3d.h>

#include <array>
#include <cmath>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PointCloudUniforms — point cloud display uniform handler
// Ported from: itwinjs-core PointCloudUniforms
//
// Handles point cloud rendering settings: size mode, voxel scale, EDL parameters.
// ---------------------------------------------------------------------------
class PointCloudUniforms {
public:
    PointCloudUniforms()
    {
        initialize(m_settings);
    }

    /// Update point cloud uniforms from settings.
    /// Ported from: itwinjs-core PointCloudUniforms.update()
    void update(dqCommon::PointCloudDisplaySettings const& settings)
    {
        if (m_settingsEquals(settings))
            return;

        m_settings = settings;
        initialize(settings);
    }

    /// Bind point cloud size/shape uniform (vec4).
    /// Ported from: itwinjs-core PointCloudUniforms.bind()
    void bind(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_vec4.data());
    }

    /// Bind EDL parameters uniform 1 (vec4).
    /// Ported from: itwinjs-core PointCloudUniforms.bindEDL1()
    void bindEDL1(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_edl1.data());
    }

    /// Bind EDL parameters uniform 2 (vec4).
    /// Ported from: itwinjs-core PointCloudUniforms.bindEDL2()
    void bindEDL2(UniformHandle& uniform) const
    {
        uniform.setUniform4fv(m_edl2.data());
    }

    /// Get the current settings.
    dqCommon::PointCloudDisplaySettings const& getSettings() const noexcept { return m_settings; }

private:
    /// Initialize uniform values from settings.
    /// Ported from: itwinjs-core PointCloudUniforms.initialize()
    void initialize(dqCommon::PointCloudDisplaySettings const& settings)
    {
        // vec4: x = fixed point size in pixels if > 0, else scale applied to voxel size (negated).
        //       y = minimum size in pixels if using voxel size.
        //       z = maximum size in pixels if using voxel size.
        //       w = 1.0 if drawing square points, 0.0 if round.
        m_vec4[0] = (settings.sizeMode == dqCommon::PointCloudSizeMode::Pixel) ?
            static_cast<float>(settings.pixelSize) : -static_cast<float>(settings.voxelScale);
        m_vec4[1] = static_cast<float>(settings.minPixelsPerVoxel);
        m_vec4[2] = static_cast<float>(settings.maxPixelsPerVoxel);
        m_vec4[3] = (settings.shape == dqCommon::PointCloudShape::Square) ? 1.0f : 0.0f;

        // EDL1: x = strength (0 disables EDL), y = radius, z = scale factor, w = is3d
        m_edl1[0] = static_cast<float>(settings.edlStrength);
        m_edl1[1] = static_cast<float>(settings.edlRadius);
        m_edl1[2] = m_scaleFactor;
        m_edl1[3] = m_is3d ? 1.0f : 0.0f;

        // EDL2: x = mix weight 1, y = mix weight 2, z = mix weight 4, w = unused
        m_edl2[0] = static_cast<float>(settings.edlMixWts1.value_or(1.0));
        m_edl2[1] = static_cast<float>(settings.edlMixWts2.value_or(0.5));
        m_edl2[2] = static_cast<float>(settings.edlMixWts4.value_or(0.25));
        m_edl2[3] = 0.0f;
    }

    /// Check if settings are equal (for change detection).
    bool m_settingsEquals(dqCommon::PointCloudDisplaySettings const& other) const
    {
        return m_settings.sizeMode == other.sizeMode &&
               m_settings.voxelScale == other.voxelScale &&
               m_settings.minPixelsPerVoxel == other.minPixelsPerVoxel &&
               m_settings.maxPixelsPerVoxel == other.maxPixelsPerVoxel &&
               m_settings.pixelSize == other.pixelSize &&
               m_settings.shape == other.shape &&
               m_settings.edlStrength == other.edlStrength &&
               m_settings.edlRadius == other.edlRadius &&
               m_settings.edlMixWts1 == other.edlMixWts1 &&
               m_settings.edlMixWts2 == other.edlMixWts2 &&
               m_settings.edlMixWts4 == other.edlMixWts4;
    }

    dqCommon::PointCloudDisplaySettings m_settings;
    float m_scaleFactor = 8.0f;
    bool m_is3d = true;

    std::array<float, 4> m_vec4{};
    std::array<float, 4> m_edl1{};
    std::array<float, 4> m_edl2{};
};

// ---------------------------------------------------------------------------
// RealityModelUniforms — reality model display uniform handler
// Ported from: itwinjs-core RealityModelUniforms
//
// Handles reality model rendering settings: point cloud + override color mix.
// ---------------------------------------------------------------------------
class FrustumUniforms;  // Forward declaration

class RealityModelUniforms {
public:
    /// Update reality model uniforms from settings.
    /// Ported from: itwinjs-core RealityModelUniforms.update()
    void update(dqCommon::RealityModelDisplaySettings const& settings)
    {
        m_overrideColorMix = static_cast<float>(settings.overrideColorRatio);
        m_pointCloud.update(settings.pointCloud);
    }

    /// Update scale factor based on range and frustum.
    /// Ported from: itwinjs-core RealityModelUniforms.updateRange()
    ///
    /// Calculates a normalized strength factor based on the size of the
    /// point cloud versus the current viewing depth.
    void updateRange(dqGeom::Range3d const* range,
                     FrustumUniforms const& frustum,
                     float const* transformScale,
                     bool is3d,
                     uint32_t viewWidth);

    /// Bind override color mix uniform.
    /// Ported from: itwinjs-core RealityModelUniforms.bindOverrideColorMix()
    void bindOverrideColorMix(UniformHandle& uniform) const
    {
        uniform.setUniform1f(m_overrideColorMix);
    }

    /// Get the point cloud uniforms.
    PointCloudUniforms& getPointCloud() noexcept { return m_pointCloud; }
    PointCloudUniforms const& getPointCloud() const noexcept { return m_pointCloud; }

private:
    PointCloudUniforms m_pointCloud;
    float m_overrideColorMix = 0.5f;
    float m_scaleFactor = 8.0f;
    bool m_is3d = true;
};

END_DQ_RENDER_NAMESPACE
