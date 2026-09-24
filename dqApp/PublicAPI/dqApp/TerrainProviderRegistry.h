// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — TerrainProviderRegistry (terrain provider management)
// Ported from: itwinjs-core core/frontend/src/tile/map/TerrainProvider.ts
//              (class TerrainProviderRegistry, co-defined with the TerrainProvider interface)
#pragma once

#include "Export.h"

namespace dqApp {

// ---------------------------------------------------------------------------
// TerrainProviderRegistry — registry for terrain data providers
// Ported from: itwinjs-core core/frontend/src/tile/map/TerrainProvider.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT TerrainProviderRegistry {
public:
    TerrainProviderRegistry() = default;
    ~TerrainProviderRegistry() = default;
};

}  // namespace dqApp
