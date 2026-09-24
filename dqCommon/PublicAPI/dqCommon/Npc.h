// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Normalized Plane Coordinate (NPC) enumeration
//
// Ported from: itwinjs-core core/common/src/Frustum.ts (Npc enum, NpcCorners, NpcCenter)
#pragma once

#include "DqCommon.h"

#include <dqGeom/Point3d.h>

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// The 8 corners of the Normalized Plane Coordinate cube.
// Ported from: itwinjs-core core/common/src/Frustum.ts
enum class Npc : int {
    LeftBottomRear = 0,
    RightBottomRear = 1,
    LeftTopRear = 2,
    RightTopRear = 3,
    LeftBottomFront = 4,
    RightBottomFront = 5,
    LeftTopFront = 6,
    RightTopFront = 7,
};

// Shorthand aliases — ported from: itwinjs-core Npc._000 etc.
inline constexpr Npc Npc_000 = Npc::LeftBottomRear;
inline constexpr Npc Npc_100 = Npc::RightBottomRear;
inline constexpr Npc Npc_010 = Npc::LeftTopRear;
inline constexpr Npc Npc_110 = Npc::RightTopRear;
inline constexpr Npc Npc_001 = Npc::LeftBottomFront;
inline constexpr Npc Npc_101 = Npc::RightBottomFront;
inline constexpr Npc Npc_011 = Npc::LeftTopFront;
inline constexpr Npc Npc_111 = Npc::RightTopFront;

// Number of corners
inline constexpr int kNpcCornerCount = 8;

// The 8 corners of the NPC cube.
// Ported from: itwinjs-core core/common/src/Frustum.ts NpcCorners
struct DQ_COMMON_EXPORT NpcCornersArray {
    dqGeom::Point3d corners[kNpcCornerCount];

    constexpr NpcCornersArray() noexcept
        : corners{
              dqGeom::Point3d{0.0, 0.0, 0.0},
              dqGeom::Point3d{1.0, 0.0, 0.0},
              dqGeom::Point3d{0.0, 1.0, 0.0},
              dqGeom::Point3d{1.0, 1.0, 0.0},
              dqGeom::Point3d{0.0, 0.0, 1.0},
              dqGeom::Point3d{1.0, 0.0, 1.0},
              dqGeom::Point3d{0.0, 1.0, 1.0},
              dqGeom::Point3d{1.0, 1.0, 1.0},
          }
    {
    }

    const dqGeom::Point3d& operator[](int index) const noexcept { return corners[index]; }
    const dqGeom::Point3d& operator[](Npc index) const noexcept { return corners[static_cast<int>(index)]; }
};

// Global NPC corners constant.
inline constexpr NpcCornersArray kNpcCorners{};

// The center point of the NPC cube.
// Ported from: itwinjs-core core/common/src/Frustum.ts NpcCenter
inline constexpr dqGeom::Point3d kNpcCenter{0.5, 0.5, 0.5};

END_DQ_COMMON_NAMESPACE
