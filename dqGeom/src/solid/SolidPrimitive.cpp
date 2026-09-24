// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/SolidPrimitive.ts
// DanQing dqGeom — SolidPrimitive implementation (destructor key function)
#include "dqGeom/SolidPrimitive.h"

BEGIN_DQ_GEOM_NAMESPACE

// Out-of-line dtor as the vtable key function.
SolidPrimitive::~SolidPrimitive() = default;

END_DQ_GEOM_NAMESPACE
