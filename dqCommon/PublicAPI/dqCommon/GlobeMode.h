// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Globe mode enumeration
//
// Ported from: itwinjs-core core/common/src/BackgroundMapSettings.ts (GlobeMode enum)
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// How the earth is displayed.
// Ported from: itwinjs-core BackgroundMapSettings.ts
enum class GlobeMode : uint8_t {
    /** Display Earth as 3d ellipsoid */
    Ellipsoid = 0,
    /** Display Earth as plane. */
    Plane = 1,
};

END_DQ_COMMON_NAMESPACE
