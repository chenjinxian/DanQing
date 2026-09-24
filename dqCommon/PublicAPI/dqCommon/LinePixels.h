// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Line pixel patterns
//
// Ported from: itwinjs-core core/common/src/LinePixels.ts
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Enumerates the available patterns for drawing patterned lines.
// Each is a 32-bit pattern in which each bit specifies the on/off state of a pixel.
// Ported from: itwinjs-core core/common/src/LinePixels.ts
enum class LinePixels : uint32_t {
    Solid = 0,
    Code0 = Solid,
    /** 1 lit, 7 unlit */
    Code1 = 0x80808080,
    /** 5 lit, 3 unlit */
    Code2 = 0xf8f8f8f8,
    /** 11 lit, 5 unlit */
    Code3 = 0xffe0ffe0,
    /** 7 lit, 4 unlit, 1 lit, 1 lit */
    Code4 = 0xfe10fe10,
    /** 3 lit, 5 unlit */
    Code5 = 0xe0e0e0e0,
    /** 5 lit, 3 unlit, 1 lit, 3 unlit, 1 lit, 3 unlit */
    Code6 = 0xf888f888,
    /** 8 lit, 3 unlit, 2 lit, 3 unlit */
    Code7 = 0xff18ff18,
    /** 2 lit, 2 unlit — default hidden edge style */
    HiddenLine = 0xcccccccc,
    /** Barely visible — 1 lit pixel followed by 31 unlit */
    Invisible = 0x00000001,
    /** No valid style or none specified */
    Invalid = static_cast<uint32_t>(-1),
};

END_DQ_COMMON_NAMESPACE
