// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — local-file tile fetcher implementation
// Authored: see FileTileFetcher.h (no reference equivalent — offline assets;
//           §8.2 零网络——TD-24 清退后无任何网络回退分支).
#include "FileTileFetcher.h"

#include <dqRender/tile/Tile.h>

#include <fstream>

namespace dqApp {

void FileTileFetcher::fetch(std::string const& url, dqRender::Tile& tile,
                            std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
                            std::function<void(dqRender::Tile&, std::string const&)> onError)
{
    // 本地文件系统路径：同步读取，下一个 processCompleted() 投递（轮询契约——
    // 投递保持在 TileAdmin process 周期上，从不在 fetch() 内联）。
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
    return static_cast<uint32_t>(m_completed.size());
}

void FileTileFetcher::cancelAll()
{
    m_completed.clear();
}

}  // namespace dqApp
