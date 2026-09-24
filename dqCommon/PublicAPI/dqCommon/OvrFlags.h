// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Feature override flags
//
// Ported from: itwinjs-core core/frontend/src/common/internal/render/OvrFlags.ts
// Bit flags encoding per-feature symbology overrides in the LUT texture.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// 8-bit override flags (packed into LUT texel[0].R).
// Ported from: itwinjs-core OvrFlags
enum class OvrFlag : uint8_t {
    None       = 0,
    LineRgb    = 1 << 0,  // Line color is overridden
    Rgb        = 1 << 1,  // Surface color is overridden
    Alpha      = 1 << 2,  // Surface alpha is overridden
    LineAlpha  = 1 << 3,  // Line alpha is overridden
    Flashed    = 1 << 4,  // Feature is flashed
    NonLocatable = 1 << 5, // Feature is non-locatable (not pickable)
    LineCode   = 1 << 6,  // Line code is overridden
    Weight     = 1 << 7,  // Line weight is overridden
};

inline OvrFlag operator|(OvrFlag a, OvrFlag b) noexcept {
    return static_cast<OvrFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline OvrFlag operator&(OvrFlag a, OvrFlag b) noexcept {
    return static_cast<OvrFlag>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline OvrFlag& operator|=(OvrFlag& a, OvrFlag b) noexcept { a = a | b; return a; }
inline bool HasFlag(OvrFlag flags, OvrFlag flag) noexcept {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

// 16-bit override flags (packed into LUT texel[0].G as high byte).
// Ported from: itwinjs-core OvrFlags (high byte)
enum class OvrFlags16 : uint8_t {
    None                       = 0,
    Hilited                    = 1 << 0,  // Feature is hilited
    Emphasized                 = 1 << 1,  // Feature is emphasized
    ViewIndependentTransparency = 1 << 2, // View-independent transparency
    InvisibleDuringPick        = 1 << 3,  // Invisible during pick operations
    Visibility                 = 1 << 4,  // Feature is visible (0 = invisible)
    IgnoreMaterial             = 1 << 5,  // Ignore material overrides
};

inline OvrFlags16 operator|(OvrFlags16 a, OvrFlags16 b) noexcept {
    return static_cast<OvrFlags16>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline OvrFlags16 operator&(OvrFlags16 a, OvrFlags16 b) noexcept {
    return static_cast<OvrFlags16>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline OvrFlags16& operator|=(OvrFlags16& a, OvrFlags16 b) noexcept { a = a | b; return a; }
inline bool HasFlag(OvrFlags16 flags, OvrFlags16 flag) noexcept {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

END_DQ_COMMON_NAMESPACE
