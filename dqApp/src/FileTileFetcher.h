// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — local-file tile fetcher (application-layer ITileFetcher)
//
// Authored: no reference equivalent exists — the browser-based reference has
// no filesystem, and in itwinjs-core standalone mode this role is played by
// the local backend serving tiles over IPC. DanQing's offline tileset assets
// (third_party/tile-sample-assets) are referenced by absolute/relative
// filesystem paths in tileset.json; this fetcher reads those paths directly.
//
// §8.2 零网络协议（2026-09-27 用户指令）：本地文件路径是唯一取数路径。曾经的
// http(s) → Qt 网络拉瓦器（TD-24 登记对象）透传分支已随 TD-24 清退删除
// （数据经 ITileFetcher DI 以字节进入引擎；RPC-dump 本地回放走
// DumpTileFetcher——同为纯文件 I/O）。
#pragma once

#include <dqRender/tile/ITileFetcher.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dqApp {

// FileTileFetcher — local-filesystem ITileFetcher (zero network, §8.2).
class FileTileFetcher : public dqRender::ITileFetcher {
public:
    FileTileFetcher() = default;

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

    std::vector<Completed> m_completed;
};

}  // namespace dqApp
