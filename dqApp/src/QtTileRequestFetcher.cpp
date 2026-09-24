// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — QtTileRequestFetcher implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts
//              (network fetch layer)
#include "QtTileRequestFetcher.h"

#include <QNetworkRequest>

namespace dqApp {

QtTileRequestFetcher::QtTileRequestFetcher(QObject* parent)
    : QObject(parent)
{
}

QtTileRequestFetcher::~QtTileRequestFetcher()
{
    cancelAll();
}

// ---------------------------------------------------------------------------
// fetch — initiate async HTTP GET for tile content
// Ported from: itwinjs-core TileRequestChannel._dispatch()
// Converts std::string URL → QString at the boundary (§8.2 interface contract).
// ---------------------------------------------------------------------------
void QtTileRequestFetcher::fetch(std::string const& url, dqRender::Tile& tile,
                                 std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
                                 std::function<void(dqRender::Tile&, std::string const&)> onError)
{
    QNetworkRequest request(QString::fromStdString(url));
    // Headers for 3D Tiles content types.
    request.setRawHeader("Accept",
        "application/octet-stream, "
        "application/json, "
        "model/gltf-binary, "
        "model/gltf+json");

    QNetworkReply* reply = m_networkManager.get(request);
    if (!reply) {
        if (onError)
            onError(tile, "Failed to create network request");
        return;
    }

    auto pending = std::make_unique<PendingRequest>();
    pending->reply = reply;
    pending->tile = &tile;
    pending->onComplete = std::move(onComplete);
    pending->onError = std::move(onError);

    m_pending.push_back(std::move(pending));
    ++m_activeCount;
}

// ---------------------------------------------------------------------------
// processCompleted — deliver finished replies to callbacks.
// Called each frame from TileAdmin::process(). Converts QByteArray →
// std::vector<uint8_t> and QString → std::string at the boundary (§8.2).
// ---------------------------------------------------------------------------
void QtTileRequestFetcher::processCompleted()
{
    // Iterate in reverse so we can erase without invalidating indices.
    for (int i = static_cast<int>(m_pending.size()) - 1; i >= 0; --i) {
        auto& req = m_pending[static_cast<size_t>(i)];
        if (!req->reply->isFinished())
            continue;

        if (req->reply->error() == QNetworkReply::NoError) {
            QByteArray data = req->reply->readAll();
            if (req->onComplete && req->tile) {
                std::vector<uint8_t> bytes(data.constData(),
                                           data.constData() + data.size());
                req->onComplete(*req->tile, bytes);
            }
        } else {
            if (req->onError && req->tile) {
                std::string errStr = req->reply->errorString().toStdString();
                req->onError(*req->tile, errStr);
            }
        }

        req->reply->deleteLater();
        m_pending.erase(m_pending.begin() + i);
        --m_activeCount;
    }
}

// ---------------------------------------------------------------------------
// cancelAll — abort all in-flight requests
// ---------------------------------------------------------------------------
void QtTileRequestFetcher::cancelAll()
{
    for (auto& req : m_pending) {
        if (req->reply) {
            req->reply->abort();
            req->reply->deleteLater();
        }
    }
    m_pending.clear();
    m_activeCount = 0;
}

} // namespace dqApp
