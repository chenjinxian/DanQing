// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Flash settings for element highlighting
// Ported from: itwinjs-core core/frontend/src/FlashSettings.ts
//
// Controls how elements flash when selected or emphasized.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Flash mode — how to render the flashed element.
// Ported from: itwinjs-core FlashSettings.ts FlashMode
enum class FlashMode : uint8_t {
    Hilite = 0,   // Apply hilite color overlay
    Brighten = 1, // Brighten the element's color
};

// Flash settings — controls flash animation behavior.
// Ported from: itwinjs-core FlashSettings.ts
struct DQ_COMMON_EXPORT FlashSettings {
    // Duration of the flash animation in seconds.
    // Ported from: itwinjs-core FlashSettings.duration (default 0.25)
    float duration = 0.25f;

    // Maximum flash intensity (0-1).
    // Ported from: itwinjs-core FlashSettings.maxIntensity (default 1.0)
    float maxIntensity = 1.0f;

    // How to render the flash.
    // Ported from: itwinjs-core FlashSettings.litMode
    FlashMode litMode = FlashMode::Hilite;
};

END_DQ_COMMON_NAMESPACE
