// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — CoordSystem (coordinate systems used by ViewingSpace)
// Ported from: itwinjs-core core/common/src/CoordSystem.ts
//
// The three coordinate spaces a ViewingSpace converts between:
//   World — model/world coordinates.
//   View  — viewport pixels (x/y) + view-space z in [-32767, 32767].
//   Npc   — normalized perspective coordinates, the corner-anchored [0,1]^3 cube.
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Coordinate systems for ViewingSpace frustum/point conversion.
// Ported from: itwinjs-core CoordSystem (CoordSystem.ts)
enum class CoordSystem : int {
    World = 0,
    View = 1,
    Npc = 2,
};

END_DQ_COMMON_NAMESPACE
