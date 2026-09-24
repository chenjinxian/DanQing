// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Sync (change detection for uniforms)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Sync.ts
//
// Lightweight change-tracking system.  Uniforms use this to avoid
// re-uploading unchanged data to the GPU.
//
// Pattern: SyncTarget has a syncKey (incremented on change).
//          SyncObserver has a syncToken (compared against target's key).
//          If key != token, the observer is out of date and must re-upload.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SyncToken — snapshot of a SyncTarget's state
// ---------------------------------------------------------------------------
struct SyncToken {
    uint32_t key = 0;
};

// ---------------------------------------------------------------------------
// SyncTarget — a source of change-tracked state
// (Ported from: itwinjs-core Sync.ts SyncTarget)
// ---------------------------------------------------------------------------
class SyncTarget {
public:
    SyncTarget() = default;

    /// Get the current sync key.
    uint32_t getSyncKey() const noexcept { return m_syncKey; }

    /// Mark the target as changed (increment key).
    void desync() noexcept { ++m_syncKey; }

    /// Check if an observer is synchronized with this target.
    bool isSynchronized(SyncToken const& token) const noexcept
    {
        return token.key == m_syncKey;
    }

    /// Synchronize an observer with this target (update its token).
    void sync(SyncToken& token) const noexcept
    {
        token.key = m_syncKey;
    }

private:
    uint32_t m_syncKey = 1;  // Start at 1 so fresh observer (0) is not synchronized
};

// ---------------------------------------------------------------------------
// SyncObserver — an observer that tracks a SyncTarget's state
// (Ported from: itwinjs-core Sync.ts SyncObserver)
// ---------------------------------------------------------------------------
class SyncObserver {
public:
    SyncObserver() = default;

    /// Check if this observer is synchronized with the given target.
    bool isSynchronized(SyncTarget const& target) const noexcept
    {
        return target.isSynchronized(m_token);
    }

    /// Synchronize this observer with the given target.
    void sync(SyncTarget const& target) noexcept
    {
        target.sync(m_token);
    }

    /// Reset the observer (mark as out of date).
    void reset() noexcept { m_token.key = 0; }

private:
    SyncToken m_token;
};

END_DQ_RENDER_NAMESPACE
