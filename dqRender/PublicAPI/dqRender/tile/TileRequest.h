// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile request
// Ported from: itwinjs-core core/frontend/src/tile/TileRequest.ts
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Tile;
class TileRequestChannel;

// ---------------------------------------------------------------------------
// TileRequest — per-tile request lifecycle
// Ported from: itwinjs-core TileRequest class
// ---------------------------------------------------------------------------
class TileRequest {
public:
    enum class State : uint8_t {
        Queued = 0,     // Waiting in channel queue
        Dispatched = 1, // Sent to data source
        Loading = 2,    // Content loading in progress
        Completed = 3,  // Content loaded successfully
        Failed = 4,     // Content loading failed
    };

    TileRequest(Tile& tile, TileRequestChannel& channel);
    ~TileRequest();

    TileRequest(TileRequest const&) = delete;
    TileRequest& operator=(TileRequest const&) = delete;

    // --- Accessors ---
    Tile& getTile() const noexcept { return m_tile; }
    TileRequestChannel& getChannel() const noexcept { return m_channel; }
    State getState() const noexcept { return m_state; }
    uint32_t getPriority() const noexcept { return m_priority; }

    // --- State transitions ---
    void dispatch();       // Queued -> Dispatched
    void startLoading();   // Dispatched -> Loading
    void complete();       // Loading -> Completed
    void fail();           // Any -> Failed

    /// Check if request is active (Dispatched or Loading)
    bool isActive() const noexcept {
        return m_state == State::Dispatched || m_state == State::Loading;
    }

    /// Check if request is terminal (Completed or Failed)
    bool isTerminal() const noexcept {
        return m_state == State::Completed || m_state == State::Failed;
    }

    /// Update priority (called before dispatch)
    void setPriority(uint32_t priority) noexcept { m_priority = priority; }

private:
    Tile& m_tile;
    TileRequestChannel& m_channel;
    State m_state = State::Queued;
    uint32_t m_priority = 0;
};

END_DQ_RENDER_NAMESPACE
