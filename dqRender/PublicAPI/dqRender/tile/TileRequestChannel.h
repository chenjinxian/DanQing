// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile request channel
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts
#pragma once

#include "TileRequest.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
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
    /// "req.channel.append(req)" (TileAdmin.ts:906-908): the channel already
    /// owns the queued request, so re-appending moves the owning handle to the
    /// back of the live pending queue. EQUIVALENCE: 参考源=TileRequestChannel
    /// .ts:224-227（Queue.append 按引用再入队——同一请求可同时在
    /// _previouslyPending 与 _pending 两队列）；发散=DanQing unique_ptr 单槽
    /// 位所有权使一请求只占一个槽位，swapPending 未移植（Task 3）前 queued
    /// 请求必在存活队列中，"再入队"退化为移到队尾（process 每帧重排
    /// TileRequestChannel.ts:240，位置无可观测语义）；Task 3 移植
    /// swapPending 后此处负责把句柄从 _previouslyPending 搬入 _pending。
    /// 验证法=TileRequestUsersTest.QueuedUserlessRequestReenqueuedNotDuplicated。
    void append(TileRequest& request);

    /// Remove a user from every live request's user set (pending + active) —
    /// the UniqueTileUserSets.forgetUser role (TileUserSet.ts:107-110 —
    /// "for each set, remove(user)"): whenever a user is unregistered there is
    /// no need to track down every associated tile request, the user is just
    /// removed from its user sets. DanQing hosts the sets on the requests (no
    /// pool), so the walk is over the channel's requests.
    void forgetUser(TileUser& user);

    /// Process pending queue: sort by priority, dispatch up to concurrency limit.
    /// @param externalInFlight  In-flight fetches outside this channel (the
    ///         fetcher's active count) — DanQing's polling fetcher owns the real
    ///         in-flight set, so the channel combines it with its own active
    ///         requests for the concurrency limit.
    /// Ported from: itwinjs-core TileRequestChannel.process (TileRequestChannel.ts:232-266
    /// — reprioritize → sort → cancel unwanted → `while (active < concurrency) dispatch`).
    void process(uint32_t externalInFlight = 0);

    /// Settle a dispatched request (content delivered or fetch failed) —
    /// removes it from the active set, freeing a concurrency slot.
    /// Ported from: itwinjs-core channel.recordCompletion (TileRequestChannel.ts:341-349).
    void settle(TileRequest& request, bool failed = false);

    /// Get number of active (dispatched/loading) requests
    uint32_t getActiveCount() const noexcept { return static_cast<uint32_t>(m_active.size()); }

    /// Get number of pending requests
    size_t getPendingCount() const noexcept { return m_pending.size(); }

    /// Clear all pending requests
    void clear();

private:
    uint32_t m_maxConcurrency;
    std::vector<std::unique_ptr<TileRequest>> m_pending;
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

    /// Remove a user from every live request's user set, across all channels
    /// (see TileRequestChannel::forgetUser).
    void forgetUser(TileUser& user);

    /// Process all channels (see TileRequestChannel::process).
    void process(uint32_t externalInFlight = 0);

    /// Clear all channels
    void clear();

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
