// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Fill flags enumeration
//
// Ported from: itwinjs-core core/common/src/GraphicParams.ts (FillFlags enum)
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Flags indicating whether and how the interiors of closed planar regions are displayed.
// Ported from: itwinjs-core core/common/src/GraphicParams.ts
enum class FillFlags : uint32_t {
    None = 0,
    /** Use the element's fill color when fill is enabled in the view's ViewFlags. */
    ByView = 1u << 0,
    /** Use the element's fill color even when fill is disabled. */
    Always = 1u << 1,
    /** Render the fill behind other geometry belonging to the same element. */
    Behind = 1u << 2,
    /** Combines Behind and Always flags. */
    Blanking = Behind | Always,
    /** Use the view's background color instead of the element's fill color. */
    Background = 1u << 3,
};

// Bitwise OR
inline FillFlags operator|(FillFlags a, FillFlags b) noexcept
{
    return static_cast<FillFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// Bitwise AND
inline FillFlags operator&(FillFlags a, FillFlags b) noexcept
{
    return static_cast<FillFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

END_DQ_COMMON_NAMESPACE
