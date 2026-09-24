// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Plan projection settings
// Ported from: itwinjs-core core/common/src/PlanProjectionSettings.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct PlanProjectionSettingsProps {
    std::optional<double> elevation;
    std::optional<double> transparency;
    std::optional<bool> overlay;
    std::optional<bool> enforceDisplayPriority;
};

// Settings for plan projection display.
// Ported from: itwinjs-core PlanProjectionSettings
class DQ_COMMON_EXPORT PlanProjectionSettings {
public:
    std::optional<double> elevation;
    std::optional<double> transparency;
    bool overlay = false;
    std::optional<bool> enforceDisplayPriority;

    static PlanProjectionSettings fromJSON(const PlanProjectionSettingsProps* props = nullptr)
    {
        PlanProjectionSettings s;
        if (!props) return s;
        s.elevation = props->elevation;
        s.transparency = props->transparency;
        if (props->overlay) s.overlay = *props->overlay;
        s.enforceDisplayPriority = props->enforceDisplayPriority;
        return s;
    }

    PlanProjectionSettingsProps toJSON() const
    {
        PlanProjectionSettingsProps p;
        p.elevation = elevation;
        p.transparency = transparency;
        p.overlay = overlay;
        p.enforceDisplayPriority = enforceDisplayPriority;
        return p;
    }

    bool equals(const PlanProjectionSettings& rhs) const noexcept
    {
        return elevation == rhs.elevation && transparency == rhs.transparency &&
               overlay == rhs.overlay && enforceDisplayPriority == rhs.enforceDisplayPriority;
    }

    PlanProjectionSettings clone() const { return *this; }
};

END_DQ_COMMON_NAMESPACE
