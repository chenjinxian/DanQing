// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — local-file tile fetcher implementation
// Authored: see FileTileFetcher.h (no reference equivalent — offline assets).
#include "FileTileFetcher.h"

#include <dqRender/tile/Tile.h>

#include <algorithm>
#include <fstream>

namespace dqApp {

FileTileFetcher::FileTileFetcher(std::unique_ptr<dqRender::ITileFetcher> httpFetcher)
    : m_httpFetcher(std::move(httpFetcher))
{
}

void FileTileFetcher::fetch(std::string const& url, dqRender::Tile& tile,
                            std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
                            std::function<void(dqRender::Tile&, std::string const&)> onError)
{
    bool const isHttp = url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
    if (isHttp && m_httpFetcher) {
        m_httpFetcher->fetch(url, tile, std::move(onComplete), std::move(onError));
        return;
    }

    // Local filesystem path: read synchronously now, deliver on the next
    // processCompleted() (the polling-fetcher contract — delivery stays on
    // the TileAdmin process cycle, never inline from fetch()).
    Completed entry;
    entry.tile = &tile;
    entry.onComplete = std::move(onComplete);
    entry.onError = std::move(onError);

    std::ifstream file(url, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        entry.ok = false;
        entry.error = "FileTileFetcher: cannot open " + url;
    } else {
        auto const size = file.tellg();
        file.seekg(0, std::ios::beg);
        entry.data.resize(static_cast<size_t>(size));
        if (size > 0 && !file.read(reinterpret_cast<char*>(entry.data.data()), size)) {
            entry.ok = false;
            entry.error = "FileTileFetcher: short read on " + url;
            entry.data.clear();
        } else {
            entry.ok = true;
        }
    }

    m_completed.push_back(std::move(entry));
}

void FileTileFetcher::processCompleted()
{
    if (m_httpFetcher)
        m_httpFetcher->processCompleted();

    // Deliver from a copied list: callbacks may re-enter fetch().
    auto pending = std::move(m_completed);
    m_completed.clear();
    for (auto& entry : pending) {
        if (!entry.tile)
            continue;
        if (entry.ok)
            entry.onComplete(*entry.tile, entry.data);
        else
            entry.onError(*entry.tile, entry.error);
    }
}

uint32_t FileTileFetcher::getActiveCount() const noexcept
{
    uint32_t n = static_cast<uint32_t>(m_completed.size());
    if (m_httpFetcher)
        n += m_httpFetcher->getActiveCount();
    return n;
}

void FileTileFetcher::cancelAll()
{
    m_completed.clear();
    if (m_httpFetcher)
        m_httpFetcher->cancelAll();
}

}  // namespace dqApp
