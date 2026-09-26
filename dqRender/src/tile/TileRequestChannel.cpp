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
    // unique_ptr single-slot ownership reduces "re-enqueue" to move-to-back
    // until swapPending lands).
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        if (it->get() == &request) {
            std::rotate(it, it + 1, m_pending.end());  // vector 随机访问迭代器
            return;
        }
    }
    // A queued request is always in the live pending queue while swapPending
    // is unported; the cross-queue move arrives with it (Task 3).
}

void TileRequestChannel::forgetUser(TileUser& user)
{
    // Ported from: UniqueTileUserSets.forgetUser (TileUserSet.ts:107-110 —
    // "for each set, remove(user)"); DanQing's sets live on the requests, so
    // the walk covers both the pending and the active requests.
    for (auto& request : m_pending)
        if (request)
            request->removeUser(user);
    for (auto& request : m_active)
        if (request)
            request->removeUser(user);
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

    // REGISTERED ADAPTATION (not ported): the reference's per-frame request
    // retirement has no DanQing counterpart yet —
    //   · swapPending double buffer (:215-219) + "previously pending and now
    //     userless → cancel" (:242-247);
    //   · "active and userless → cancel" (:249-253) + processCancellations
    //     (:256);
    //   · TileAdmin.forgetUser → onUserIModelClosed canceling requests of
    //     interest only to the departing user (TileAdmin.ts:560-563 → :925-940).
    // The faces these rest on ARE ported now — TileRequest.users / addUser /
    // isQueued / isCanceled (TileRequest.ts:25-77) and the processRequests
    // dedup gate `undefined === tile.request` (TileAdmin.ts:897-925) that
    // makes one shared request carry many users. What is still missing is the
    // cancel() state machine + the per-frame clearAll/swapPending cycle
    // (TileAdmin.ts:829-841) that empties the sets each frame: a request whose
    // users all depart is removed from the sets by TileRequestChannel::
    // forgetUser but keeps loading, and a userless queued request is not yet
    // canceled here. Porting that retirement cycle is the follow-up task
    // (Task 3), not a local patch.
    // EQUIVALENCE: 参考源=TileRequestChannel.ts:242-253 + TileAdmin.ts:925-940；
    // 发散=DanQing 残留请求不被取消而是完成加载（并发槽被无人关心的请求占用、
    // 内容进 LRU 未选中分区）——验证法=调度序单测（TileRequestChannelTest）不
    // 依赖取消面；取消面接线的回归锁由 Task 3 补（TileRequestUsersTest 已锁
    // users/isCanceled 面）。

    // 2. Dispatch pending requests up to the concurrency limit. The reference
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

        // Dispatch BEFORE initiating the fetch. The fetcher may fail
        // synchronously (e.g. NullTileFetcher invokes onError inline), which
        // settles the request via TileAdmin and destroys it — the raw pointer
        // must not be touched after requestContent() unless it is still the
        // tile's in-flight request.
        raw->dispatch();
        tile.setRequest(raw);
        m_active.push_back(std::move(request));  // ownership → channel
        bool const initiated = tile.requestContent();
        TileAdmin::instance().recordDispatched();

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

void TileRequestChannel::clear()
{
    // Pending/active requests die with the channel; detach tile hooks first.
    for (auto& request : m_pending)
        if (request && request->getTile().getRequest() == request.get())
            request->getTile().setRequest(nullptr);
    for (auto& request : m_active)
        if (request && request->getTile().getRequest() == request.get())
            request->getTile().setRequest(nullptr);
    m_pending.clear();
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

void TileRequestChannels::forgetUser(TileUser& user)
{
    // See TileRequestChannel::forgetUser (the UniqueTileUserSets.forgetUser
    // role — TileUserSet.ts:107-110).
    for (auto& entry : m_channels) {
        entry.channel->forgetUser(user);
    }
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
