// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileAdmin implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileAdmin.ts
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/ITileFetcher.h"
#include "NullTileFetcher.h"
#include "LRUTileList.h"

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型
#include <dqRender/RenderGraphic.h>  // TileContent::graphic unique_ptr 析构需完整类型

#include <algorithm>
#include <chrono>
#include <optional>

BEGIN_DQ_RENDER_NAMESPACE

TileAdmin* TileAdmin::sInstance = nullptr;

std::optional<double> TileAdmin::s_nowOverride = std::nullopt;  // test clock override

TileAdmin::TileAdmin()
    : m_fetcher(std::make_unique<NullTileFetcher>())
    , m_lruList(std::make_unique<LRUTileList>())
{
    sInstance = this;
}

TileAdmin::~TileAdmin()
{
    if (sInstance == this) {
        sInstance = nullptr;
    }
}

void TileAdmin::setFetcher(std::unique_ptr<ITileFetcher> fetcher)
{
    m_fetcher = std::move(fetcher);
}

TileAdmin& TileAdmin::instance()
{
    // Lazy initialization
    static TileAdmin sDefaultInstance;
    if (!sInstance) {
        sInstance = &sDefaultInstance;
    }
    return *sInstance;
}

void TileAdmin::process()
{
    // Ported from: itwinjs-core TileAdmin.ts process()
    // Called each frame from Application::EventLoop

    // 1. Process the request queue (collect from users, dispatch via channels)
    processQueue();

    // 2. Process all channels (sort, dispatch up to concurrency limit).
    //    The polling fetcher owns the real in-flight set, so its active count
    //    participates in the channels' concurrency limit (see
    //    TileRequestChannel::process — reference keeps this inside the channel).
    m_channels.process(m_fetcher ? m_fetcher->getActiveCount() : 0);

    // 3. Process completed async fetches (deliver data to tiles)
    // Ported from: itwinjs-core TileRequestChannel._processCompleted()
    if (m_fetcher) {
        m_fetcher->processCompleted();
    }

    // Statistics mirrors (panel reads statistics() only — keep the live
    // active/pending counts in sync with the queue state).
    m_statistics.numActiveRequests = getActiveRequestCount();
    m_statistics.numPendingRequests = static_cast<uint32_t>(getPendingRequestCount());

    // 4. Prune expired tiles and purge unused trees
    pruneAndPurge();

    // 5. Free GPU memory if over budget
    // Ported from: TileAdmin.process → freeMemory (TileAdmin.ts:451-453).
    freeMemory();
}

void TileAdmin::processQueue()
{
    // Ported from: itwinjs-core TileAdmin.ts processQueue()
    // For each user, collect requested tiles and create requests
    for (auto* user : m_users) {
        processRequestsForUser(*user);
    }
}

void TileAdmin::processRequestsForUser(TileUser& user)
{
    // Ported from: itwinjs-core TileAdmin.ts processRequests()
    auto it = m_requestedTiles.find(&user);
    if (it == m_requestedTiles.end()) return;

    auto& requestedTiles = it->second;
    auto& channel = m_channels.getChannel("default");

    for (auto* tile : requestedTiles) {
        if (!tile) continue;

        // Only create request if tile is not loaded and not already loading
        if (tile->getLoadStatus() != TileLoadStatus::NotLoaded) continue;

        // Create a request for this tile
        auto request = std::make_unique<TileRequest>(*tile, channel);

        // Compute priority based on tile depth
        request->setPriority(tile->computeLoadPriority());

        // Append to channel for dispatch
        channel.append(std::move(request));
    }

    // Clear processed requests
    requestedTiles.clear();
}

void TileAdmin::registerUser(TileUser& user)
{
    m_users.insert(&user);
}

void TileAdmin::forgetUser(TileUser& user)
{
    m_users.erase(&user);
    m_requestedTiles.erase(&user);
    m_selectedTiles.erase(&user);
    m_readyTiles.erase(&user);
    m_externalTiles.erase(&user);
}

void TileAdmin::addTilesForUser(TileUser& user,
                                 std::vector<Tile*> const& selected,
                                 std::vector<Tile*> const& ready,
                                 std::vector<Tile*> const& touched)
{
    // Ported from: TileAdmin.addTilesForUser (TileAdmin.ts:514-533) — LRU
    // markUsed for the three sets + the selectedAndReady entry. (The former
    // DanQing extension that re-derived the request set from `selected`
    // ∩ NotLoaded is gone: the request feed comes from the scene's missing
    // set via requestTiles, as in the reference.)
    // Store tiles for this user
    m_selectedTiles[&user] = selected;
    m_readyTiles[&user] = ready;

    // "selected"/"ready"/"touched" keep contents alive — mark used in the LRU
    // (reference: _lruList.markUsed for all three sets, TileAdmin.ts:516-520).
    if (m_lruList) {
        m_lruList->markUsed(user.getTileUserId(), selected);
        m_lruList->markUsed(user.getTileUserId(), ready);
        m_lruList->markUsed(user.getTileUserId(), touched);
    }

    // Mark the timestamp of every selected tile (TileUsageMarker.mark —
    // TileUsageMarker.ts:43-45; drives time-based pruning).
    double const now = nowSeconds();
    for (auto* tile : selected)
        if (tile) tile->markUsed(now);
}

void TileAdmin::requestTiles(TileUser& user, std::vector<Tile*> const& tiles)
{
    // Ported from: TileAdmin.requestTiles (TileAdmin.ts:498-500) —
    // _requestsPerUser.set(user, tiles): set-REPLACE per user. Consumed (and
    // cleared) by processRequestsForUser on the next process().
    m_requestedTiles[&user] = tiles;
}

void TileAdmin::resetStatistics() noexcept
{
    // Reference: channels.resetStatistics() + _totalElided = 0 — i.e. the
    // cumulative totals reset; the live queue mirrors re-populate next process().
    // Keep the current live mirrors (active/pending) so the panel doesn't flicker.
    auto const numActive = m_statistics.numActiveRequests;
    auto const numPending = m_statistics.numPendingRequests;
    auto const numCanceled = m_statistics.numCanceled;
    m_statistics = Statistics{};
    m_statistics.numActiveRequests = numActive;
    m_statistics.numPendingRequests = numPending;
    m_statistics.numCanceled = numCanceled;
}

void TileAdmin::recordDecodingTime(double milliseconds) noexcept
{
    auto& d = m_statistics.decoding;
    d.total += milliseconds;
    ++m_numDecodes;
    d.mean = d.total / m_numDecodes;
    d.max = std::max(d.max, milliseconds);
    d.min = (m_numDecodes == 1) ? milliseconds : std::min(d.min, milliseconds);
}

bool TileAdmin::getTilesForUser(TileUser const& user, SelectedAndReadyTiles& out) const
{
    // Ported from: TileAdmin.ts:506 getTilesForUser (returns undefined → false).
    auto selIt = m_selectedTiles.find(const_cast<TileUser*>(&user));
    if (selIt == m_selectedTiles.end())
        return false;

    out.selected = &selIt->second;
    auto readyIt = m_readyTiles.find(const_cast<TileUser*>(&user));
    out.ready = (readyIt != m_readyTiles.end()) ? &readyIt->second : nullptr;
    auto extIt = m_externalTiles.find(&user);
    out.external = (extIt != m_externalTiles.end()) ? extIt->second : ExternalTileStatistics{};
    return true;
}

size_t TileAdmin::getNumRequestsForUser(TileUser const& user) const
{
    // Ported from: TileAdmin.ts:474 getNumRequestsForUser
    // (requests set size + external.requested).
    size_t count = 0;
    auto it = m_requestedTiles.find(const_cast<TileUser*>(&user));
    if (it != m_requestedTiles.end())
        count = it->second.size();
    auto extIt = m_externalTiles.find(&user);
    if (extIt != m_externalTiles.end())
        count += extIt->second.requested;
    return count;
}

void TileAdmin::addExternalTilesForUser(TileUser& user, ExternalTileStatistics const& statistics)
{
    // Ported from: TileAdmin.ts:536 addExternalTilesForUser (entry created if absent).
    m_externalTiles[&user] = statistics;
}

void TileAdmin::clearTilesForUser(TileUser& user)
{
    // Ported from: TileAdmin.ts:550-555 — entry delete + LRU clearUsed (tiles
    // left with no user demote to the "not selected" partition).
    m_selectedTiles.erase(&user);
    m_readyTiles.erase(&user);
    if (m_lruList)
        m_lruList->clearUsed(user.getTileUserId());
}

size_t TileAdmin::totalTileContentBytes() const
{
    return m_lruList ? m_lruList->getTotalBytesUsed() : 0;
}

void TileAdmin::forEachSelectedLoadedTile(std::function<void(Tile&)> const& visit) const
{
    if (m_lruList)
        m_lruList->forEachSelectedTile(visit);
}

void TileAdmin::forEachUnselectedLoadedTile(std::function<void(Tile&)> const& visit) const
{
    if (m_lruList)
        m_lruList->forEachUnselectedTile(visit);
}

void TileAdmin::purgeUnselectedTileContents()
{
    if (!m_lruList)
        return;
    std::vector<Tile*> unselected;
    m_lruList->forEachUnselectedTile([&unselected](Tile& tile) {
        unselected.push_back(&tile);
    });
    for (auto* tile : unselected) {
        if (!tile)
            continue;
        tile->freeMemory();
        m_lruList->drop(*tile);
    }
}

void TileAdmin::onTileContentLoaded(Tile& tile)
{
    // Ported from: TileAdmin.ts:759-765 — may already be present (content
    // replacement) → drop + add, then raise onTileLoad.
    if (m_lruList) {
        m_lruList->drop(tile);
        m_lruList->add(tile);
    }
    onTileLoad.Raise(tile);
}

void TileAdmin::deliverTileContent(Tile& tile, std::vector<uint8_t> const& data)
{
    // Ported from: TileRequest.handleResponse (TileRequest.ts:156-195) —
    // settle the channel request first (recordCompletion), then bytes →
    // readContent → setContent (which raises onTileLoad → scene invalidation).
    if (TileRequest* request = tile.getRequest()) {
        request->getChannel().settle(*request);
        recordCompleted();
    }

    TileContent content = tile.readContent(data.data(), data.size());
    tile.setContent(std::move(content));
}

void TileAdmin::reportTileFetchError(Tile& tile, std::string const& error)
{
    // Ported from: TileRequest.ts error paths (tile.setNotFound) with channel
    // failure accounting.
    (void)error;
    if (TileRequest* request = tile.getRequest()) {
        request->getChannel().settle(*request);
        recordFailed();
    }
    tile.setNotFound();
}

void TileAdmin::onTileContentDisposed(Tile& tile)
{
    // Ported from: TileAdmin.ts:770-773.
    if (m_lruList)
        m_lruList->drop(tile);
}

void TileAdmin::setTileExpirationTime(double seconds) noexcept
{
    // Ported from: TileAdmin.ts:302-303 — clamp(seconds, 5, 60), default 20.
    m_tileExpirationTime = std::clamp(seconds, 5.0, 60.0);
}

double TileAdmin::nowSeconds()
{
    if (s_nowOverride.has_value())
        return *s_nowOverride;
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TileAdmin::setNowOverrideForTest(std::optional<double> seconds)
{
    s_nowOverride = seconds;
    // Re-arm the throttle AT the new "now" so the next process() prunes
    // immediately — the per-tile timestamps decide what actually gets
    // released, the throttle only decides WHEN the pass runs.
    if (seconds.has_value())
        instance().m_nextPruneTime = *seconds;
}

void TileAdmin::pruneAndPurge()
{
    // Ported from: TileAdmin.ts pruneAndPurge (:849-895 — needPrune branch):
    // collect the trees in use by all users, then tree.prune(now - expiration).
    // The needPurge branch (iModel.tiles.purge) has no DanQing registry-level
    // purge yet — the Tiles registry from the faithful-path port is app-side;
    // registered follow-up.
    double const now = nowSeconds();
    if (now < m_nextPruneTime)
        return;
    m_nextPruneTime = now + m_tileExpirationTime;

    std::vector<TileTree*> trees;
    for (auto* user : m_users) {
        if (!user) continue;
        user->discloseTileTrees(trees);
    }

    double const cutoff = now - m_tileExpirationTime;
    for (auto* tree : trees)
        if (tree) tree->prune(cutoff);
}

void TileAdmin::freeMemory()
{
    // Ported from: itwinjs-core TileAdmin.ts freeMemory (:843-847) —
    // evict the LRU's not-selected partition until under budget.
    if (m_maxTotalTileContentBytes > 0 && m_lruList)
        m_lruList->freeMemory(m_maxTotalTileContentBytes);
}

uint32_t TileAdmin::getActiveRequestCount() const
{
    return m_channels.totalActiveCount();
}

size_t TileAdmin::getPendingRequestCount() const
{
    return m_channels.totalPendingCount();
}

END_DQ_RENDER_NAMESPACE
