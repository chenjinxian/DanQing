// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — local-file tile fetcher (application-layer ITileFetcher)
//
// Authored: no reference equivalent exists — the browser-based reference has
// no filesystem, and in itwinjs-core standalone mode this role is played by
// the local backend serving tiles over IPC. DanQing's offline tileset assets
// (third_party/tile-sample-assets) are referenced by absolute/relative
// filesystem paths in tileset.json, which QNetworkAccessManager cannot fetch
// (no file:// scheme support). This fetcher routes by URL scheme: http(s)
// transparently forwards to the injected network fetcher; anything else is
// read from the local filesystem.
#pragma once

#include <dqRender/tile/ITileFetcher.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dqApp {

// FileTileFetcher — scheme-routing fetcher (local files + HTTP passthrough).
class FileTileFetcher : public dqRender::ITileFetcher {
public:
    // Takes ownership of the network fetcher used for http(s) URLs.
    explicit FileTileFetcher(std::unique_ptr<dqRender::ITileFetcher> httpFetcher);

    // ITileFetcher
    void fetch(std::string const& url, dqRender::Tile& tile,
               std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
               std::function<void(dqRender::Tile&, std::string const&)> onError) override;
    void processCompleted() override;
    uint32_t getActiveCount() const noexcept override;
    void cancelAll() override;

private:
    struct Completed {
        dqRender::Tile* tile = nullptr;
        bool ok = false;
        std::vector<uint8_t> data;
        std::string error;
        std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete;
        std::function<void(dqRender::Tile&, std::string const&)> onError;
    };

    std::unique_ptr<dqRender::ITileFetcher> m_httpFetcher;
    std::vector<Completed> m_completed;
};

}  // namespace dqApp
