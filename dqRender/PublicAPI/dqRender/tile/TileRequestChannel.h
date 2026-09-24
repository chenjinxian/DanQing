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
