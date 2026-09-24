// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Qt-free null/default ITileFetcher
// Authored: no reference exists in itwinjs-core for a null tile fetcher;
//           itwinjs couples tile fetch to IModelConnection. This is a DanQing
//           SDK default so dqRender stays Qt-free (§8.2) until the application
//           injects a concrete fetcher (e.g., dqApp's QtTileRequestFetcher)
//           via TileAdmin::setFetcher().
#pragma once

#include "dqRender/tile/ITileFetcher.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// NullTileFetcher — no-op ITileFetcher used as TileAdmin's Qt-free default.
// fetch() reports failure via onError so callers do not wait on a backend
// that was never injected. Real fetching requires the application to inject
// a concrete fetcher via TileAdmin::setFetcher().
class NullTileFetcher : public ITileFetcher {
public:
    void fetch(std::string const& url, Tile& tile,
               std::function<void(Tile&, std::vector<uint8_t> const&)> onComplete,
               std::function<void(Tile&, std::string const&)> onError) override;

    void processCompleted() override;
    uint32_t getActiveCount() const noexcept override;
    void cancelAll() override;
};

END_DQ_RENDER_NAMESPACE
