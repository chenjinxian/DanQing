// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Grid orientation enumeration
//
// Ported from: itwinjs-core core/common/src/ViewDetails.ts (GridOrientationType enum, lines 44-55)
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Describes the orientation of the grid displayed within a Viewport.
// Ported from: itwinjs-core GridOrientationType (ViewDetails.ts:44-55)
enum class GridOrientationType : int32_t {
    View     = 0,  // Ported from: ViewDetails.ts:46 — oriented with the view
    WorldXY  = 1,  // Ported from: ViewDetails.ts:48 — top
    WorldYZ  = 2,  // Ported from: ViewDetails.ts:50 — right
    WorldXZ  = 3,  // Ported from: ViewDetails.ts:52 — front
    AuxCoord = 4,  // Ported from: ViewDetails.ts:54 — oriented by the auxiliary coordinate system
};

END_DQ_COMMON_NAMESPACE
