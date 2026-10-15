// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Environment settings
// Ported from: itwinjs-core core/common/src/Environment.ts
#pragma once

#include "Export.h"
#include "Atmosphere.h"
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
    // ← EnvironmentProps.atmosphere（Environment.ts:25——M-W 集成面载体）。
    std::optional<Atmosphere::Props> atmosphere;
};

// Scene environment (sky + ground).
// Ported from: itwinjs-core Environment
class DQ_COMMON_EXPORT Environment {
public:
    bool displaySky = false;
    bool displayGround = false;
    // ← Environment.displayAtmosphere（Environment.ts:53——true 时大气显示并覆盖
    //   sky box 颜色信息；M-W 集成面）。
    bool displayAtmosphere = false;
    SkyBox sky;
    GroundPlane ground;
    // ← Environment.atmosphere（Environment.ts:63——Atmosphere.Settings.defaults
    //   缺省，ctor :71）。
    Atmosphere::Settings atmosphere;

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
        // ← Environment.fromJSON atmosphere 腿（Environment.ts:113-120——
        //   displayAtmosphere 读 props.atmosphere.display）。
        if (props->atmosphere) {
            env.atmosphere = Atmosphere::Settings::fromJSON(&*props->atmosphere);
            env.displayAtmosphere = props->atmosphere->display.value_or(false);
        }
        return env;
    }

    EnvironmentProps toJSON() const
    {
        EnvironmentProps p;
        p.ground = ground.toJSON(displayGround);
        p.sky = sky.toJSON(displaySky);
        // ← Environment.toJSON（Environment.ts:100-106——atmosphere 恒携 display 写出）。
        p.atmosphere = atmosphere.toJSON(displayAtmosphere);
        return p;
    }

    Environment clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
