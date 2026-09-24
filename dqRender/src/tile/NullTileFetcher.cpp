// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — NullTileFetcher implementation
// Authored: no reference exists; Qt-free SDK default (§8.2).
#include "NullTileFetcher.h"

BEGIN_DQ_RENDER_NAMESPACE

// Authored: no network backend until the application injects one.
void NullTileFetcher::fetch(std::string const& /*url*/, Tile& tile,
                            std::function<void(Tile&, std::vector<uint8_t> const&)> /*onComplete*/,
                            std::function<void(Tile&, std::string const&)> onError)
{
    if (onError)
        onError(tile, "no ITileFetcher configured; inject one via TileAdmin::setFetcher()");
}

void NullTileFetcher::processCompleted() { /* no-op */ }

uint32_t NullTileFetcher::getActiveCount() const noexcept { return 0; }

void NullTileFetcher::cancelAll() { /* no-op */ }

END_DQ_RENDER_NAMESPACE
