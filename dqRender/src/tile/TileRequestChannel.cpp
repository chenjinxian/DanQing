// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileRequestChannel implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts
#include "dqRender/tile/TileRequestChannel.h"
#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileAdmin.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TileRequestChannel
// ---------------------------------------------------------------------------

TileRequestChannel::TileRequestChannel(uint32_t maxConcurrency)
    : m_maxConcurrency(maxConcurrency)
{
}

TileRequestChannel::~TileRequestChannel() = default;

void TileRequestChannel::append(std::unique_ptr<TileRequest> request)
{
    m_pending.push_back(std::move(request));
}

void TileRequestChannel::append(TileRequest& request)
{
    // See the owning header for the EQUIVALENCE registration (reference
    // Queue.append re-enqueues by reference — TileRequestChannel.ts:224-227;
    // unique_ptr single-slot ownership turns the dual queue membership into a
    // move from m_previouslyPending into m_pending).
    for (auto it = m_previouslyPending.begin(); it != m_previouslyPending.end(); ++it) {
        if (it->get() == &request) {
            m_pending.push_back(std::move(*it));  // 跨队列搬运（单槽位所有权）
            m_previouslyPending.erase(it);
            return;
        }
    }
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (it->get() == &request) {
            std::rotate(it, it + 1, m_pending.end());  // vector 随机访问迭代器
            return;
        }
    }
    // Unreachable via the shared-request gate: a queued request always lives
    // in exactly one of the two queues (unique_ptr ownership).
}

void TileRequestChannel::swapPending()
{
    // Ported from: TileRequestChannel.swapPending (TileRequestChannel.ts:215-219)
    // — the queue objects are exchanged; membership (ownership here) travels
    // with the queue, so last frame's pending requests wake up in
    // m_previouslyPending and new appends land in the fresh m_pending.
    std::swap(m_pending, m_previouslyPending);
}

void TileRequestChannel::clearAll()
{
    // Ported from: UniqueTileUserSets.clearAll (TileUserSet.ts:126-128 —
    // "forEach(set => set.clear())" via TileAdmin.ts:829); DanQing hosts the
    // sets on the requests, so the walk is over the channel's live requests.
    for (auto& request : m_pending)
        if (request)
            request->clearUsers();
    for (auto& request : m_previouslyPending)
        if (request)
            request->clearUsers();
    for (auto& request : m_active)
        if (request)
            request->clearUsers();
}

void TileRequestChannel::forgetUser(TileUser& user)
{
    // Ported from: UniqueTileUserSets.forgetUser (TileUserSet.ts:107-110 —
    // "for each set, remove(user)"); DanQing's sets live on the requests, so
    // the walk covers the pending, previously-pending and active requests.
    for (auto& request : m_pending)
        if (request)
            request->removeUser(user);
    for (auto& request : m_previouslyPending)
        if (request)
            request->removeUser(user);
    for (auto& request : m_active)
        if (request)
            request->removeUser(user);
}

void TileRequestChannel::cancel(std::unique_ptr<TileRequest> const& request)
{
    // Ported from: TileRequestChannel.cancel (TileRequestChannel.ts:325-328 —
    // request.cancel() + ++_statistics.numCanceled; DanQing folds the channel
    // statistics into TileAdmin.Statistics).
    request->cancel();
    TileAdmin::instance().recordCanceled();
}

void TileRequestChannel::process(uint32_t externalInFlight)
{
    // Ported from: itwinjs-core TileRequestChannel.ts process() (:232-266)
    // 1. Sort pending queue: tree loadPriority dominates, request priority
    //    breaks ties (both ascending — lower = dispatched first).
    // Ported from: TileRequestChannel.ts:13-20 — tree priority dominates;
    // request priority breaks ties (both ascending: lower = dispatched first).
    // EQUIVALENCE: 参考源=TileRequestChannel.ts:16-17（TileRequestQueue 比较器，
    // 差值比较）；DanQing=std::sort 谓词（每帧 process 重排，等价于参考每帧
    // _pending.sort() :240——TileRequestQueue 继承 PriorityQueue，其 sort() 是
    // 二叉堆自底向上建堆）。发散=std::sort 非稳定 vs 参考 PriorityQueue.sort()
    // 二叉堆（core/bentley/src/PriorityQueue.ts:77-80 建堆——同样完全不稳定）——
    // 仅当两请求两级键全等时可达；键全等的请求互为可互换候选，调度序发散无可
    // 观测语义（验证法=TileRequestChannelTest 两级键单测）。
    std::sort(m_pending.begin(), m_pending.end(),
        [](std::unique_ptr<TileRequest> const& a,
           std::unique_ptr<TileRequest> const& b) {
            auto const lpA = a->getTile().getTree().getLoadPriority();
            auto const lpB = b->getTile().getTree().getLoadPriority();
            if (lpA != lpB)
                return lpA < lpB;
            return a->getPriority() < b->getPriority();
        });

    // 2. Cancel any previously pending requests that are no longer needed
    //    (TileRequestChannel.ts:242-245): after this frame's clearAll
    //    (TileAdmin.processQueue) emptied the user sets, a request left here
    //    with no users is of interest to nobody. The cancel() releases the
    //    tile hook, so the clear() below destroys exactly canceled requests —
    //    every still-wanted queued request was moved into m_pending by the
    //    shared-request gate's re-enqueue (TileAdmin.ts:913-915).
    for (auto const& queued : m_previouslyPending)
        if (queued->getUsers().empty())
            cancel(queued);
    m_previouslyPending.clear();  // (:247)

    // 3. Cancel any active requests that are no longer needed.
    //    NB (:250): Do NOT remove them from the active set until their http
    //    activity has completed — the fetch keeps running and the late
    //    response is dropped by TileAdmin::deliverTileContent
    //    (TileRequest.ts:109-110). (TileRequestChannel.ts:249-253)
    for (auto const& active : m_active)
        if (active->getUsers().empty())
            cancel(active);

    // 4. Batch-cancel running requests (TileRequestChannel.ts:256 → :299) —
    //    no-op base for DanQing's polling fetcher (see the header note).
    processCancellations();

    // 5. Dispatch pending requests up to the concurrency limit. The reference
    // dispatches by invoking `channel.requestContent(tile, isCanceled)` which
    // forwards to `tile.requestContent()` and awaits its promise
    // (TileRequestChannel.ts:305-306 → TileRequest.ts:80-89). DanQing's fetcher
    // is polling-based (completion delivered via TileAdmin's sink on a later
    // process()), so dispatch = initiate the fetch now; the request lives in
    // m_active until TileAdmin settles it (deliverTileContent/reportTileFetchError).
    while (!m_pending.empty()
           && m_active.size() + externalInFlight < m_maxConcurrency) {
        auto request = std::move(m_pending.front());
        m_pending.erase(m_pending.begin());

        Tile& tile = request->getTile();
        TileRequest* raw = request.get();

        // The reference's channel.dispatch counts the statistic before the
        // request's own canceled guard can trip (TileRequestChannel.ts:313).
        TileAdmin::instance().recordDispatched();

        // Reference dispatch guard (TileRequest.ts:81-82): a request canceled
        // while queued never begins its fetch. The owning handle drops it
        // here; release the tile hook first if it still points at the request
        // (the reference leaves the hook for GC — DanQing frees eagerly).
        if (raw->isCanceled()) {
            if (tile.getRequest() == raw)
                tile.setRequest(nullptr);
            continue;
        }

        // Dispatch BEFORE initiating the fetch. The fetcher may fail
        // synchronously (e.g. NullTileFetcher invokes onError inline), which
        // settles the request via TileAdmin and destroys it — the raw pointer
        // must not be touched after requestContent() unless it is still the
        // tile's in-flight request.
        raw->dispatch();
        tile.setRequest(raw);
        m_active.push_back(std::move(request));  // ownership → channel
        bool const initiated = tile.requestContent();

        // A synchronous completion/failure path has already settled the request
        // (tile's request hook cleared); only touch it while still in flight.
        if (tile.getRequest() == raw) {
            if (!initiated) {
                // Fetch could not even be initiated (no content URI, ...).
                raw->fail();
                TileAdmin::instance().recordFailed();
                settle(*raw, /*failed=*/true);
            } else {
                // Reference: loadStatus composes to Loading while a request is
                // in flight (Tile.ts:271-295) — prevents re-requesting.
                tile.setLoadStatus(TileLoadStatus::Loading);
            }
        }
    }
}

void TileRequestChannel::settle(TileRequest& request, bool failed)
{
    // Ported from: channel.recordCompletion (TileRequestChannel.ts:341-349) —
    // the request leaves the active set, freeing a concurrency slot.
    for (auto it = m_active.begin(); it != m_active.end(); ++it) {
        if (it->get() == &request) {
            if (request.getTile().getRequest() == &request)
                request.getTile().setRequest(nullptr);
            if (failed)
                request.fail();
            else
                request.complete();
            m_active.erase(it);
            return;
        }
    }
}

TileRequest* TileRequestChannel::findActiveRequestForTile(Tile& tile)
{
    // See the owning header for the EQUIVALENCE registration (the mirror of
    // _active.add, TileRequestChannel.ts:314 — the reference reaches the
    // in-flight request through the dispatch closure instead).
    for (auto& request : m_active)
        if (request && &request->getTile() == &tile)
            return request.get();
    return nullptr;
}

void TileRequestChannel::clear()
{
    // Pending/active requests die with the channel; detach tile hooks first.
    for (auto& request : m_pending)
        if (request && request->getTile().getRequest() == request.get())
            request->getTile().setRequest(nullptr);
    for (auto& request : m_previouslyPending)
        if (request && request->getTile().getRequest() == request.get())
            request->getTile().setRequest(nullptr);
    for (auto& request : m_active)
        if (request && request->getTile().getRequest() == request.get())
            request->getTile().setRequest(nullptr);
    m_pending.clear();
    m_previouslyPending.clear();
    m_active.clear();
}

// ---------------------------------------------------------------------------
// TileRequestChannels
// ---------------------------------------------------------------------------

TileRequestChannel& TileRequestChannels::getChannel(char const* name)
{
    // Find existing channel
    for (auto& entry : m_channels) {
        if (entry.name == name) {
            return *entry.channel;
        }
    }

    // Create new channel
    auto channel = std::make_unique<TileRequestChannel>();
    auto* ptr = channel.get();
    m_channels.push_back({name, std::move(channel)});
    return *ptr;
}

void TileRequestChannels::swapPending()
{
    // Ported from: TileRequestChannels.swapPending (TileRequestChannels.ts:179-182)
    // — invoked by TileAdmin.processQueue when it is about to start enqueuing
    // new requests.
    for (auto& entry : m_channels) {
        entry.channel->swapPending();
    }
}

void TileRequestChannels::clearAll()
{
    // Ported from: TileAdmin.processQueue's clearAll step (TileAdmin.ts:828-829
    // — UniqueTileUserSets.clearAll); DanQing hosts the user sets on the
    // requests, so the walk is over the channels' live requests.
    for (auto& entry : m_channels) {
        entry.channel->clearAll();
    }
}

void TileRequestChannels::forgetUser(TileUser& user)
{
    // See TileRequestChannel::forgetUser (the UniqueTileUserSets.forgetUser
    // role — TileUserSet.ts:107-110).
    for (auto& entry : m_channels) {
        entry.channel->forgetUser(user);
    }
}

TileRequest* TileRequestChannels::findActiveRequestForTile(Tile& tile)
{
    // See TileRequestChannel::findActiveRequestForTile.
    for (auto& entry : m_channels) {
        if (TileRequest* request = entry.channel->findActiveRequestForTile(tile))
            return request;
    }
    return nullptr;
}

void TileRequestChannels::process(uint32_t externalInFlight)
{
    for (auto& entry : m_channels) {
        entry.channel->process(externalInFlight);
    }
}

void TileRequestChannels::clear()
{
    for (auto& entry : m_channels) {
        entry.channel->clear();
    }
}

uint32_t TileRequestChannels::totalActiveCount() const
{
    uint32_t n = 0;
    for (auto const& entry : m_channels)
        n += entry.channel->getActiveCount();
    return n;
}

size_t TileRequestChannels::totalPendingCount() const
{
    size_t n = 0;
    for (auto const& entry : m_channels)
        n += entry.channel->getPendingCount();
    return n;
}

END_DQ_RENDER_NAMESPACE
