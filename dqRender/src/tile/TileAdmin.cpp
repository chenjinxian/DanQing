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
#include <cassert>
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

TileAdmin::TileAdmin(DefaultInstanceTag)
    : m_fetcher(std::make_unique<NullTileFetcher>())
    , m_lruList(std::make_unique<LRUTileList>())
{
    // Non-registering: see DefaultInstanceTag in the header. instance()
    // registers this object itself, only when no live instance exists.
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
    // Lazy initialization. The default object is built with the
    // non-registering constructor: when this static constructs while a live
    // instance (test fixture, embedder) is registered, that instance must
    // keep receiving the instance() handle and the statistics — the public
    // constructor's sInstance = this would otherwise hijack the registration.
    static TileAdmin sDefaultInstance{DefaultInstanceTag::Default};
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
    // Ported from: itwinjs-core TileAdmin.ts processQueue() (:827-841).
    // 1. "Mark all requests as being associated with no users, indicating
    //    they are no longer needed" (:828-829 — UniqueTileUserSets.clearAll;
    //    DanQing hosts the sets on the requests, so the walk is over the
    //    channels' live requests).
    m_channels.clearAll();

    // 2. "Notify channels that we are enqueuing new requests" (:831-832) —
    //    last frame's pending queue becomes previouslyPending; its requests
    //    are canceled by channels.process unless re-enqueued below.
    m_channels.swapPending();

    // 3. "Repopulate pending requests queue from each user. We do NOT sort by
    //    priority while doing so." (:834-835) — processRequests per user; the
    //    feeds are NOT consumed here (see processRequestsForUser).
    for (auto* user : m_users) {
        processRequestsForUser(*user);
    }

    // 4. channels.process() runs from TileAdmin::process (frame order
    //    preserved: repopulate → process).
}

void TileAdmin::processRequestsForUser(TileUser& user)
{
    // Ported from: itwinjs-core TileAdmin.ts processRequests (:897-925) — the
    // shared-request gate: the first user's pass creates the request and hooks
    // it on the tile (tile.request, TileAdmin.ts:902) so every later user
    // sharing the tile joins the existing request (addUser) instead of
    // building a duplicate.
    auto it = m_requestedTiles.find(&user);
    if (it == m_requestedTiles.end()) return;

    auto& requestedTiles = it->second;
    auto& channel = m_channels.getChannel("default");

    for (auto* tile : requestedTiles) {
        if (!tile) continue;

        TileRequest* request = tile->getRequest();
        if (!request) {
            // Ported from: TileAdmin.ts:899-908 — no request yet: only a
            // NotLoaded tile gets one, and the tile hook is assigned at
            // creation, before append, so same-frame users hit the share
            // branch below (the reference's `tile.request = request`, :905).
            // (assert(this.channels.has(request.channel)), :906 — the DanQing
            // getChannel factory always returns a registered channel.)
            if (tile->getLoadStatus() == TileLoadStatus::NotLoaded) {
                auto owned = std::make_unique<TileRequest>(*tile, channel, user);

                // Compute priority based on tile depth (reference recomputes
                // per frame in channel process — TileRequestChannel.ts:237;
                // the creation-time value is the pre-existing DanQing seam).
                owned->setPriority(tile->computeLoadPriority());

                request = owned.get();
                tile->setRequest(request);
                channel.append(std::move(owned));  // ownership → channel
            }
        } else {
            // Ported from: TileAdmin.ts:909-920.
            // Request may already be dispatched (in channel's active requests)
            // - if so do not re-enqueue! (TileAdmin.ts:913-915) — after
            // swapPending the queued request sits in previouslyPending; the
            // append moves it back into the live queue.
            if (request->isQueued() && request->getUsers().empty())
                channel.append(*request);

            request->addUser(user);
            assert(0 < request->getUsers().size());  // TileAdmin.ts:918
        }
    }

    // NB: the feed is deliberately NOT consumed — the reference's
    // _requestsPerUser entry persists until requestTiles replaces it
    // (TileAdmin.ts:498-500) and is re-walked every processQueue so requests
    // still wanted keep their users re-added after each frame's clearAll. The
    // former DanQing one-shot clear would empty every request's user set one
    // frame after its feed was consumed once the clearAll cycle landed,
    // canceling in-flight loads. EQUIVALENCE: 参考源=TileAdmin.ts:837
    // (_requestsPerUser.forEach(processRequests) — 持久集合)；发散=DanQing 此前
    // 消费后清空（现在与参考一致）——验证法=
    // TileAdminTest.QueuedRequestSurvivesFrameBoundaryViaReenqueue。
}

void TileAdmin::registerUser(TileUser& user)
{
    m_users.insert(&user);
}

void TileAdmin::forgetUser(TileUser& user)
{
    // Ported from: TileAdmin.forgetUser (TileAdmin.ts:560-563 —
    // onUserIModelClosed(user) then _users.delete(user)) + onUserIModelClosed
    // (TileAdmin.ts:925-940): clear the usage/tile sets, then "if we can
    // establish that only this user wants a given tile, cancel its request
    // immediately" — `undefined !== request && 1 === request.users.length`.
    // The cancel walk runs BEFORE the user-set removal below: the reference
    // notes "user will be removed from TileUserSets in process()" (:924), so
    // the departing user is still in the sets here. (DanQing removes eagerly
    // via m_channels.forgetUser — the sole-user check also reads the feed,
    // which persists per processRequestsForUser.) The plan adds `contains`
    // (size==1 && front==user): identical whenever the feed was processed
    // (its tiles' requests carry this user); it only excludes the reference's
    // corner of canceling a foreign user's request from a never-processed
    // feed — registered plan requirement, no observable divergence otherwise.
    // NB: TileAdmin-level cancel does not record numCanceled — the reference
    // counts only channel.cancel (TileRequestChannel.ts:325-328) inside the
    // process loops (TileAdmin.ts:936 calls request.cancel() directly).
    auto feedIt = m_requestedTiles.find(&user);
    if (feedIt != m_requestedTiles.end()) {
        for (Tile* tile : feedIt->second) {
            if (!tile) continue;
            TileRequest* request = tile->getRequest();
            if (!request) continue;
            auto const& users = request->getUsers();
            if (users.size() == 1 && users.front() == &user)
                request->cancel();
        }
        m_requestedTiles.erase(feedIt);
    }

    // The UniqueTileUserSets.forgetUser role (TileUserSet.ts:107-110 — remove
    // the user from every remaining request's user set; DanQing hosts the
    // sets on the requests, so the walk is over the channels' live requests).
    m_channels.forgetUser(user);
    m_users.erase(&user);
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
    // _requestsPerUser.set(user, tiles): set-REPLACE per user. The feed is
    // PERSISTENT: processRequestsForUser re-walks it every processQueue (the
    // reference never clears the entry — see the NB in processRequestsForUser).
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
    // Ported from: TileRequest.dispatch + handleResponse (TileRequest.ts:88-110
    // → :156-195) — bytes have arrived: the data-arrival Loading migration
    // first (:92-93), then settle (recordCompletion/dropActiveRequest), then
    // the :109-110 gate → readContent → setContent (which raises onTileLoad →
    // scene invalidation).
    //
    // The in-flight request is resolved through the channel's active set
    // FIRST: a request canceled while its fetch ran has already released the
    // tile hook (TileRequest.cancel → TileRequest.ts:144-145) but stays in
    // the active set until its http activity completes
    // (TileRequestChannel.ts:250 NB).
    TileRequest* request = m_channels.findActiveRequestForTile(tile);
    if (!request)
        request = tile.getRequest();

    if (request) {
        // Data arrival → Loading migration (TileRequest.ts:92-93 — "Set this
        // now, so our `isCanceled` check can see it"): the reference moves the
        // request to Loading UNCONDITIONALLY at this point, before the drop
        // gate (:109-110). A request canceled while its fetch ran arrives here
        // Failed with an empty user set; the migration puts it under
        // isCanceled's Loading exemption (:58-60 — "After we've received the
        // raw tile data, always finish processing it - otherwise tile may end
        // up in limbo") so the data delivers. Without it the drop branch below
        // discards the bytes and the tile is pinned in Loading forever (cancel
        // released the tile hook + processRequestsForUser only creates requests
        // for NotLoaded tiles → never re-requested).
        //
        // EQUIVALENCE: 参考源=TileRequest.ts:92-93（数据到达点无条件 Loading
        // 迁移）+ :109-110（丢弃门）；语义=数据已到→交付（迁移后 ：109-110 门
        // 对已到数据恒放行——唯一残余丢弃面是参考 isCanceled 首句 iModel
        // disposed，DanQing 无对应物，TileRequest.h isCanceled 已登记 TODO）；
        // 无数据→丢弃/失败链（reportTileFetchError → settle(failed) +
        // setNotFound，:94-103 catch → :148-153 setFailed——不设 Loading）；
        // 发散=参考由 dispatch 的 promise 闭包直接持有请求对象，DanQing 的
        // 完成链只带 (tile, data)，以 channel 活动集按 tile 反查（_active.add
        // 的镜像）+ tile 钩子兜底（无 channel 请求的交付路径保持原行为）；
        // 验证法=TileAdminTest.ForgetUserCancelsSoleRequestImmediately（数据
        // 到达 → 交付）+ TileAdminTest.CanceledRequestWithoutDataStaysDropped
        // （无数据 → 失败链不变）+
        // TileRequestChannelTest.UsersEmptyCancelsPendingAndActive（active 段）。
        request->startLoading();

        if (request->isCanceled()) {
            // TileRequest.ts:109-110 的丢弃门。迁移后对已到数据不可达（见上
            // EQUIVALENCE：唯一可达面是未移植的 iModel disposed 句）；保留以
            // 对齐参考结构。
            request->getChannel().settle(*request, /*failed=*/true);
            return;
        }
        request->getChannel().settle(*request);
        recordCompleted();
    }

    TileContent content = tile.readContent(data.data(), data.size());
    tile.setContent(std::move(content));
}

void TileAdmin::reportTileFetchError(Tile& tile, std::string const& error)
{
    // Ported from: TileRequest.ts error path (TileRequest.ts:94-103 →
    // setFailed :148-153 — notifyAndClear + Failed + tile.setNotFound +
    // recordFailure). NB: setFailed runs unconditionally on fetch error —
    // even for a request canceled in flight (the reference's catch does not
    // re-check isCanceled). settle uses failed=true so the request state is
    // Failed, matching setFailed.
    (void)error;
    // Same active-set-first resolution as deliverTileContent: a canceled
    // request's hook is gone but its active slot must still be released (the
    // reference's dispatch catch reaches the request via its own closure).
    TileRequest* request = m_channels.findActiveRequestForTile(tile);
    if (!request)
        request = tile.getRequest();
    if (request) {
        request->getChannel().settle(*request, /*failed=*/true);
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
