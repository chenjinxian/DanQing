// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Atmosphere settings
// Ported from: itwinjs-core core/common/src/Atmosphere.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Atmosphere rendering settings.
// Ported from: itwinjs-core Atmosphere namespace
namespace Atmosphere {

    // JSON persistence.
    struct WavelengthsProps {
        double r = 0.0;
        double g = 0.0;
        double b = 0.0;
    };

    struct Props {
        std::optional<bool> display;
        std::optional<double> atmosphereHeightAboveEarth;
        std::optional<double> exposure;
        std::optional<double> densityFalloff;
        std::optional<double> depthBelowEarthForMaxDensity;
        std::optional<int> numViewRaySamples;
        std::optional<int> numSunRaySamples;
        std::optional<double> scatteringStrength;
        std::optional<WavelengthsProps> wavelengths;
    };

    // Scattering wavelengths.
    class Wavelengths {
    public:
        double r = 680.0;
        double g = 550.0;
        double b = 440.0;

        bool equals(const Wavelengths& rhs) const noexcept
        {
            return r == rhs.r && g == rhs.g && b == rhs.b;
        }
    };

    // Atmosphere rendering settings.
    class DQ_COMMON_EXPORT Settings {
    public:
        double atmosphereHeightAboveEarth = 100000.0;
        double exposure = 20.0;
        double densityFalloff = 4.0;
        double depthBelowEarthForMaxDensity = -2000.0;
        int numViewRaySamples = 16;
        int numSunRaySamples = 4;
        double scatteringStrength = 1.0;
        Wavelengths wavelengths;

        static const Settings& defaults() noexcept
        {
            static const Settings s_default;
            return s_default;
        }

        static const Settings& highQuality() noexcept
        {
            static const Settings s_hq = []() {
                Settings s;
                s.numViewRaySamples = 64;
                s.numSunRaySamples = 16;
                return s;
            }();
            return s_hq;
        }

        static Settings fromJSON(const Props* props = nullptr)
        {
            Settings s;
            if (!props) return s;
            if (props->atmosphereHeightAboveEarth) s.atmosphereHeightAboveEarth = *props->atmosphereHeightAboveEarth;
            if (props->exposure) s.exposure = *props->exposure;
            if (props->densityFalloff) s.densityFalloff = *props->densityFalloff;
            if (props->depthBelowEarthForMaxDensity) s.depthBelowEarthForMaxDensity = *props->depthBelowEarthForMaxDensity;
            if (props->numViewRaySamples) s.numViewRaySamples = *props->numViewRaySamples;
            if (props->numSunRaySamples) s.numSunRaySamples = *props->numSunRaySamples;
            if (props->scatteringStrength) s.scatteringStrength = *props->scatteringStrength;
            if (props->wavelengths) {
                s.wavelengths.r = props->wavelengths->r;
                s.wavelengths.g = props->wavelengths->g;
                s.wavelengths.b = props->wavelengths->b;
            }
            return s;
        }

        Props toJSON() const
        {
            Props p;
            p.atmosphereHeightAboveEarth = atmosphereHeightAboveEarth;
            p.exposure = exposure;
            p.densityFalloff = densityFalloff;
            p.depthBelowEarthForMaxDensity = depthBelowEarthForMaxDensity;
            p.numViewRaySamples = numViewRaySamples;
            p.numSunRaySamples = numSunRaySamples;
            p.scatteringStrength = scatteringStrength;
            p.wavelengths = {wavelengths.r, wavelengths.g, wavelengths.b};
            return p;
        }

        bool equals(const Settings& rhs) const noexcept
        {
            return atmosphereHeightAboveEarth == rhs.atmosphereHeightAboveEarth &&
                   exposure == rhs.exposure && densityFalloff == rhs.densityFalloff &&
                   numViewRaySamples == rhs.numViewRaySamples &&
                   numSunRaySamples == rhs.numSunRaySamples &&
                   scatteringStrength == rhs.scatteringStrength &&
                   wavelengths.equals(rhs.wavelengths);
        }
    };

}  // namespace Atmosphere

END_DQ_COMMON_NAMESPACE
