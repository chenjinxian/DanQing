// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/polyface/Polyface.ts (Polyface abstract base)
// DanQing dqGeom — Polyface implementation (destructor key function + AreIndicesValid)
#include "dqGeom/Polyface.h"

BEGIN_DQ_GEOM_NAMESPACE

// Out-of-line dtor as the vtable key function.
Polyface::~Polyface() = default;

// Ported from: itwinjs-core Polyface.areIndicesValid.
bool Polyface::AreIndicesValid(std::vector<int32_t> const& indices, size_t posA, size_t posB,
                               size_t dataLength) noexcept {
    if (posA >= indices.size())
        return false;
    if (posB <= posA || posB > indices.size())
        return false;
    for (size_t i = posA; i < posB; ++i) {
        if (indices[i] < 0 || static_cast<size_t>(indices[i]) >= dataLength)
            return false;
    }
    return true;
}

END_DQ_GEOM_NAMESPACE
