// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Geometry class enumeration
//
// Ported from: itwinjs-core core/common/src/GeometryParams.ts (GeometryClass enum)
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// Categorizes a piece of geometry within a GeometryStream.
// Ported from: itwinjs-core core/common/src/GeometryParams.ts
enum class GeometryClass : uint8_t {
    /** The "real" geometry within a model. */
    Primary = 0,
    /** Drawing aid geometry (e.g., grid lines). */
    Construction = 1,
    /** Annotations which dimension (measure) the Primary geometry. */
    Dimension = 2,
    /** Geometry used to fill planar regions with a 2d pattern (e.g., hatch lines). */
    Pattern = 3,
};

END_DQ_COMMON_NAMESPACE
