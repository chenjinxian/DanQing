// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — View flags (rendering configuration)
// Ported from: itwinjs-core core/frontend/src/ViewFlags.ts
#pragma once

#include "Export.h"

#include <cstdint>

namespace dqApp {

// ---------------------------------------------------------------------------
// RenderMode — how surfaces are rendered
// ---------------------------------------------------------------------------
enum class RenderMode : uint8_t {
    Wireframe = 0,
    HiddenLine = 1,
    SmoothShade = 2,
    FlatShade = 3,
};

// ---------------------------------------------------------------------------
// ViewFlags — controls what is visible and how it is rendered
// Ported from: itwinjs-core ViewFlags.ts
// ---------------------------------------------------------------------------
struct DQ_APP_EXPORT ViewFlags {
    bool renderMode = true;       // SmoothShade (vs wireframe)
    bool lighting = true;
    bool visibleEdges = false;
    bool hiddenEdges = false;
    bool backgroundMap = false;
    bool shadows = false;
    bool ambientOcclusion = false;
    bool thematicDisplay = false;
    bool grid = false;
    bool acs = false;             // auxiliary coordinate system
    bool wiremesh = false;
    bool materials = true;
    bool textures = true;
    bool monochrome = false;
    bool fill = true;
};

}  // namespace dqApp
