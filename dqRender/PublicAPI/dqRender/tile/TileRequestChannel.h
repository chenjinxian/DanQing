// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile request channel
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts
#pragma once

#include "TileRequest.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TileRequestChannel — priority queue with concurrency limit
// Ported from: itwinjs-core TileRequestChannel class
// ---------------------------------------------------------------------------
class TileRequestChannel {
public:
    explicit TileRequestChannel(uint32_t maxConcurrency = 10);
    ~TileRequestChannel();

    TileRequestChannel(TileRequestChannel const&) = delete;
    TileRequestChannel& operator=(TileRequestChannel const&) = delete;

    /// Append a request to the pending queue
    void append(std::unique_ptr<TileRequest> request);

    /// Re-enqueue form of append — the shared-request gate's
    /// "req.channel.append(req)" (TileAdmin.ts:913-915): after the frame
    /// boundary's swapPending the still-queued request lives in
    /// m_previouslyPending, so re-appending MOVES the owning handle into the
    /// current m_pending. EQUIVALENCE: 参考源=TileRequestChannel.ts:224-227
    /// （Queue.append 按引用入 _pending——同一请求此刻仍是 _previouslyPending
    /// 的成员，双队列成员无害：取消循环按 users 判定、clear() 只摘队列成员
    /// :243-247）；发散=DanQing unique_ptr 单槽位所有权把"双成员 + clear 摘
    /// 除"折叠为一次跨队列搬运，可观测语义相同（仍排队的请求回到本帧队列、
    /// 被取消的留在 previouslyPending 等清空）。验证法=
    /// TileRequestChannelTest.ReenqueueMovesRequestFromPreviouslyPending +
    /// TileAdminTest.QueuedRequestSurvivesFrameBoundaryViaReenqueue。
    void append(TileRequest& request);

    /// Swap the pending queue with the previously-pending queue. Invoked by
    /// TileRequestChannels::swapPending when TileAdmin is about to start
    /// enqueuing new requests: last frame's queue becomes
    /// m_previouslyPending — its requests are canceled by process() unless
    /// re-enqueued this frame — and appends land in the fresh m_pending.
    /// Ported from: itwinjs-core TileRequestChannel.swapPending
    /// (TileRequestChannel.ts:215-219).
    void swapPending();

    /// Empty every live request's user set (pending + previouslyPending +
    /// active) — the UniqueTileUserSets.clearAll role (TileAdmin.processQueue
    /// head, TileAdmin.ts:828-829 → TileUserSet.ts:126-128 "for each set,
    /// set.clear()"): all requests are marked "no longer needed" and only
    /// tiles re-fed this frame get their users back via addUser.
    /// DanQing hosts the sets on the requests, so the walk is over the
    /// channel's live requests.
    /// Ported from: itwinjs-core UniqueTileUserSets.clearAll
    /// (TileUserSet.ts:126-128, reached from TileAdmin.ts:829).
    void clearAll();

    /// Remove a user from every live request's user set (pending +
    /// previouslyPending + active) — the UniqueTileUserSets.forgetUser role
    /// (TileUserSet.ts:107-110 — "for each set, remove(user)"): whenever a
    /// user is unregistered there is no need to track down every associated
    /// tile request, the user is just removed from its user sets. DanQing
    /// hosts the sets on the requests (no pool), so the walk is over the
    /// channel's requests.
    void forgetUser(TileUser& user);

    /// Process pending queue: reprioritize, sort, cancel userless requests in
    /// both the previously-pending and the active queue, then dispatch up to
    /// the concurrency limit.
    /// @param externalInFlight  In-flight fetches outside this channel (the
    ///         fetcher's active count) — DanQing's polling fetcher owns the real
    ///         in-flight set, so the channel combines it with its own active
    ///         requests for the concurrency limit.
    /// Ported from: itwinjs-core TileRequestChannel.process
    /// (TileRequestChannel.ts:232-266 — reprioritize :236-237 → sort :240 →
    /// cancel previously-pending userless :242-245 → clear previouslyPending
    /// :247 → cancel active userless :249-253 → processCancellations :256 →
    /// `while (active < concurrency) dispatch` :258-265).
    void process(uint32_t externalInFlight = 0);

    /// Settle a dispatched request (content delivered or fetch failed) —
    /// removes it from the active set, freeing a concurrency slot.
    /// Ported from: itwinjs-core channel.recordCompletion (TileRequestChannel.ts:341-349).
    void settle(TileRequest& request, bool failed = false);

    /// Get number of active (dispatched/loading) requests
    uint32_t getActiveCount() const noexcept { return static_cast<uint32_t>(m_active.size()); }

    /// Get number of pending requests
    size_t getPendingCount() const noexcept { return m_pending.size(); }

    /// Clear all queues (pending, previouslyPending, active), detaching the
    /// tile hooks of the dying requests first.
    void clear();

    /// Find this channel's active (dispatched, unsettled) request for a tile.
    /// REGISTERED ADAPTATION: the reference mediates the late response through
    /// the TileRequest's own promise closure (TileRequest.ts:89/:107-110);
    /// DanQing's completion chain carries only (tile, data) and the request
    /// must also be reachable after cancel() released the tile hook
    /// (TileRequest.ts:144-145) — the active set is that handle (the mirror
    /// of _active.add, TileRequestChannel.ts:314). Consumed by
    /// TileAdmin::deliverTileContent's cancel-drop path. (The cancel walks it
    /// serves landed in M-C Task 3: swapPending double-buffer + users-empty
    /// cancel + forgetUser withdrawal.)
    TileRequest* findActiveRequestForTile(Tile& tile);

private:
    /// Mark a request canceled and record the cancellation statistic.
    /// Ported from: itwinjs-core TileRequestChannel.cancel
    /// (TileRequestChannel.ts:325-328 — request.cancel() + ++numCanceled).
    void cancel(std::unique_ptr<TileRequest> const& request);

    /// Batch-cancel running requests accumulated by onActiveRequestCanceled.
    /// No-op base for DanQing: the polling fetcher has no backend
    /// cancellation accumulator (the reference override lives on the IPC
    /// ElementGraphicsChannel, TileRequestChannels.ts:16-41). TODO port the
    /// override with IPC-style channel support.
    /// Ported from: itwinjs-core TileRequestChannel.processCancellations
    /// (TileRequestChannel.ts:299, called from process :256).
    void processCancellations() noexcept {}

    uint32_t m_maxConcurrency;
    std::vector<std::unique_ptr<TileRequest>> m_pending;
    // Last frame's pending queue (TileRequestChannel.ts:134
    // _previouslyPending): swapped in by swapPending, canceled/cleared by
    // process unless the shared-request gate re-enqueued its requests.
    std::vector<std::unique_ptr<TileRequest>> m_previouslyPending;
    std::vector<std::unique_ptr<TileRequest>> m_active;
};

// ---------------------------------------------------------------------------
// TileRequestChannels — registry of named channels
// Ported from: itwinjs-core TileRequestChannels class
// ---------------------------------------------------------------------------
class TileRequestChannels {
public:
    TileRequestChannels() = default;
    ~TileRequestChannels() = default;

    /// Get or create a channel by name
    TileRequestChannel& getChannel(char const* name);

    /// Swap every channel's pending queue (TileAdmin.processQueue head).
    /// Ported from: itwinjs-core TileRequestChannels.swapPending
    /// (TileRequestChannels.ts:179-182).
    void swapPending();

    /// Empty every live request's user set, across all channels (the
    /// UniqueTileUserSets.clearAll role — see TileRequestChannel::clearAll).
    /// Ported from: itwinjs-core TileAdmin.processQueue's clearAll step
    /// (TileAdmin.ts:828-829).
    void clearAll();

    /// Remove a user from every live request's user set, across all channels
    /// (see TileRequestChannel::forgetUser).
    void forgetUser(TileUser& user);

    /// Process all channels (see TileRequestChannel::process).
    void process(uint32_t externalInFlight = 0);

    /// Clear all channels
    void clear();

    /// Find any channel's active request for a tile (see
    /// TileRequestChannel::findActiveRequestForTile).
    TileRequest* findActiveRequestForTile(Tile& tile);

    /// Aggregate active/pending counts across all channels (TileAdmin statistics).
    uint32_t totalActiveCount() const;
    size_t totalPendingCount() const;

private:
    struct ChannelEntry {
        std::string name;
        std::unique_ptr<TileRequestChannel> channel;
    };

    std::vector<ChannelEntry> m_channels;
};

END_DQ_RENDER_NAMESPACE
