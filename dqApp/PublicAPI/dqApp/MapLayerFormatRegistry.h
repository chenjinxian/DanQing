// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — MapLayerFormatRegistry (map layer format management)
// Ported from: itwinjs-core core/frontend/src/tile/map/MapLayerFormatRegistry.ts
#pragma once

#include "Export.h"
#include <string>
#include <unordered_map>

namespace dqApp {

// ---------------------------------------------------------------------------
// MapLayerFormatRegistry — registry for map layer format providers
// Ported from: itwinjs-core core/frontend/src/tile/map/MapLayerFormatRegistry.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT MapLayerFormatRegistry {
public:
    MapLayerFormatRegistry() = default;
    ~MapLayerFormatRegistry() = default;

    // Whether a format is registered.
    bool hasFormat(std::string const& formatName) const {
        return m_formats.count(formatName) > 0;
    }

private:
    std::unordered_map<std::string, bool> m_formats;
};

}  // namespace dqApp
