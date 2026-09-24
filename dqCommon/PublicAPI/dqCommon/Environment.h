// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Environment settings
// Ported from: itwinjs-core core/common/src/Environment.ts
#pragma once

#include "Export.h"
#include "GroundPlane.h"
#include "SkyBox.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct EnvironmentProps {
    std::optional<GroundPlaneProps> ground;
    std::optional<SkyBoxProps> sky;
};

// Scene environment (sky + ground).
// Ported from: itwinjs-core Environment
class DQ_COMMON_EXPORT Environment {
public:
    bool displaySky = false;
    bool displayGround = false;
    SkyBox sky;
    GroundPlane ground;

    static const Environment& defaults() noexcept
    {
        static const Environment s_default;
        return s_default;
    }

    static Environment fromJSON(const EnvironmentProps* props = nullptr)
    {
        Environment env;
        if (!props) return env;
        if (props->ground) {
            env.ground = GroundPlane::fromJSON(&*props->ground);
            env.displayGround = props->ground->display;
        }
        if (props->sky) {
            env.sky = SkyBox::fromJSON(&*props->sky);
            env.displaySky = props->sky->display;
        }
        return env;
    }

    EnvironmentProps toJSON() const
    {
        EnvironmentProps p;
        p.ground = ground.toJSON(displayGround);
        p.sky = sky.toJSON(displaySky);
        return p;
    }

    Environment clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
