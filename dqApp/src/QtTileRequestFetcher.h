// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Qt-based ITileFetcher (application-layer network backend)
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts
//              (network fetch layer)
//
// Concrete implementation of dqRender::ITileFetcher using Qt
// QNetworkAccessManager. Lives in dqApp (Qt allowed per §8.2); injected into
// dqRender::TileAdmin via setFetcher() at Application::Startup(). Must live
// on a thread with a Qt event loop.
#pragma once

#include <dqRender/tile/ITileFetcher.h>

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dqApp {

// QtTileRequestFetcher — async HTTP fetcher for 3D Tiles tileset content
// (tileset.json, b3dm, i3dm, glTF). Implements the Qt-free ITileFetcher
// contract using std types at the boundary; Qt types are internal only.
class QtTileRequestFetcher : public QObject, public dqRender::ITileFetcher {
    Q_OBJECT

public:
    explicit QtTileRequestFetcher(QObject* parent = nullptr);
    ~QtTileRequestFetcher() override;

    QtTileRequestFetcher(QtTileRequestFetcher const&) = delete;
    QtTileRequestFetcher& operator=(QtTileRequestFetcher const&) = delete;

    // --- dqRender::ITileFetcher (std types at boundary, §8.2) ---
    void fetch(std::string const& url, dqRender::Tile& tile,
               std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
               std::function<void(dqRender::Tile&, std::string const&)> onError) override;

    void processCompleted() override;
    uint32_t getActiveCount() const noexcept override { return m_activeCount; }
    void cancelAll() override;

private:
    struct PendingRequest {
        QNetworkReply* reply = nullptr;
        dqRender::Tile* tile = nullptr;
        std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete;
        std::function<void(dqRender::Tile&, std::string const&)> onError;
    };

    QNetworkAccessManager m_networkManager;
    std::vector<std::unique_ptr<PendingRequest>> m_pending;
    uint32_t m_activeCount = 0;
};

} // namespace dqApp
