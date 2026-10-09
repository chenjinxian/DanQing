// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Ambient occlusion settings
// Ported from: itwinjs-core core/common/src/AmbientOcclusion.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Ambient occlusion namespace (Props + Settings).
// Ported from: itwinjs-core AmbientOcclusion
namespace AmbientOcclusion {

    // JSON persistence.
    struct Props {
        std::optional<double> bias;
        std::optional<double> zLengthCap;
        std::optional<double> maxDistance;
        std::optional<double> intensity;
        std::optional<double> texelStepSize;
        std::optional<double> blurDelta;
        std::optional<double> blurSigma;
        std::optional<double> blurTexelStepSize;
    };

    // Ambient occlusion settings with resolved defaults.
    // Ported from: itwinjs-core AmbientOcclusion.Settings
    class DQ_COMMON_EXPORT Settings {
    public:
        double bias = 0.25;
        double zLengthCap = 0.0025;
        double maxDistance = 10000.0;
        double intensity = 1.0;
        double texelStepSize = 1.0;
        double blurDelta = 1.0;
        double blurSigma = 2.0;
        double blurTexelStepSize = 1.0;

        static const Settings& defaults() noexcept
        {
            static const Settings s_default;
            return s_default;
        }

        static Settings fromJSON(const Props* props = nullptr)
        {
            Settings s;
            if (!props) return s;
            if (props->bias) s.bias = *props->bias;
            if (props->zLengthCap) s.zLengthCap = *props->zLengthCap;
            if (props->maxDistance) s.maxDistance = *props->maxDistance;
            if (props->intensity) s.intensity = *props->intensity;
            if (props->texelStepSize) s.texelStepSize = *props->texelStepSize;
            if (props->blurDelta) s.blurDelta = *props->blurDelta;
            if (props->blurSigma) s.blurSigma = *props->blurSigma;
            if (props->blurTexelStepSize) s.blurTexelStepSize = *props->blurTexelStepSize;
            return s;
        }

        // Ported from: itwinjs-core AmbientOcclusion.Settings.toJSON（逐字段
        // !=默认才写——缺席=fromJSON 缺省守恒；M-T T-a 前恒写全字段系发散，
        // AmbientOcclusionTest.ToJsonOmitsDefaults RED 实锤后归位）。
        Props toJSON() const
        {
            auto const& d = defaults();
            Props p;
            if (bias != d.bias) p.bias = bias;
            if (zLengthCap != d.zLengthCap) p.zLengthCap = zLengthCap;
            if (maxDistance != d.maxDistance) p.maxDistance = maxDistance;
            if (intensity != d.intensity) p.intensity = intensity;
            if (texelStepSize != d.texelStepSize) p.texelStepSize = texelStepSize;
            if (blurDelta != d.blurDelta) p.blurDelta = blurDelta;
            if (blurSigma != d.blurSigma) p.blurSigma = blurSigma;
            if (blurTexelStepSize != d.blurTexelStepSize) p.blurTexelStepSize = blurTexelStepSize;
            return p;
        }

        bool equals(const Settings& rhs) const noexcept
        {
            return bias == rhs.bias && zLengthCap == rhs.zLengthCap &&
                   maxDistance == rhs.maxDistance && intensity == rhs.intensity &&
                   texelStepSize == rhs.texelStepSize && blurDelta == rhs.blurDelta &&
                   blurSigma == rhs.blurSigma && blurTexelStepSize == rhs.blurTexelStepSize;
        }
    };

}  // namespace AmbientOcclusion

END_DQ_COMMON_NAMESPACE
