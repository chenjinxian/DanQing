// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Rendering mode enumeration
//
// Ported from: itwinjs-core core/common/src/ViewFlags.ts (RenderMode enum)
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Enumerates the available basic rendering modes.
// Ported from: itwinjs-core core/common/src/ViewFlags.ts
enum class RenderMode : int {
    /** Renders only the edges of surfaces. Lighting not applied. */
    Wireframe = 0,
    /** By default, renders surfaces without their edges. */
    SmoothShade = 6,
    /** Renders surfaces and their edges. All surfaces rendered opaque. */
    SolidFill = 4,
    /** Identical to SolidFill, except surfaces drawn using background color. */
    HiddenLine = 3,
};

END_DQ_COMMON_NAMESPACE
