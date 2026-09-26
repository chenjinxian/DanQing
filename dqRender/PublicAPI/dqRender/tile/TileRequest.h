// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile request
// Ported from: itwinjs-core core/frontend/src/tile/TileRequest.ts
#pragma once

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Tile;
class TileRequestChannel;
class TileUser;

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

    // The reference constructor is (tile, user) and derives the channel from
    // tile.channel (TileRequest.ts:34-39); DanQing threads the channel
    // explicitly (registered adaptation: Tile carries no channel member) and
    // seeds the user set with the requesting user exactly as the reference
    // constructor does via getTileUserSetForRequest.
    TileRequest(Tile& tile, TileRequestChannel& channel, TileUser& user);
    ~TileRequest();

    TileRequest(TileRequest const&) = delete;
    TileRequest& operator=(TileRequest const&) = delete;

    // --- Accessors ---
    Tile& getTile() const noexcept { return m_tile; }
    TileRequestChannel& getChannel() const noexcept { return m_channel; }
    State getState() const noexcept { return m_state; }
    uint32_t getPriority() const noexcept { return m_priority; }

    /// The set of TileUsers awaiting the result of this request. When this
    /// becomes empty, the request is canceled because no user cares about it
    /// (TileRequest.ts:25-28/63). EQUIVALENCE: 参考源=TileRequest.ts:28 +
    /// TileUserSet.ts:15-40（ReadonlyTileUserSet = 按 tileUserId 排序的去重
    /// 集，经 UniqueTileUserSets 池共享实例，TileUserSet.ts:113-141）；发散=
    /// DanQing 每请求自持 vector、按 tileUserId 线性查重（池是 GC 期内存
    /// 优化，行为面=集合语义，已保留；迭代序发散：参考按 id 升序、DanQing
    /// 按插入序——移植子集内无消费者，notify 归 Task 3 时复核）。
    /// 验证法=TileRequestUsersTest（去重/遗忘/取消三面）。
    std::vector<TileUser*> const& getUsers() const noexcept { return m_users; }

    /// Indicate that the specified user is awaiting the result of this
    /// request.
    /// Ported from: itwinjs-core TileRequest.addUser (TileRequest.ts:72-74 —
    /// the set union via getTileUserSetForRequest; SortedArray insert dedups
    /// by tileUserId, TileUserSet.ts:37).
    void addUser(TileUser& user);

    /// Remove a user from the request's set (the reference removes via the
    /// pooled TileUserSet.remove, TileUserSet.ts:38 — DanQing hosts the set
    /// on the request, so the set's remove operation lands here; the pool's
    /// "for each set, remove(user)" walk is TileRequestChannel::forgetUser).
    /// Ported from: itwinjs-core TileUserSet.remove (TileUserSet.ts:38/:107-110).
    void removeUser(TileUser& user);

    /// True if the request has been enqueued but not yet dispatched.
    /// Ported from: itwinjs-core TileRequest.isQueued (TileRequest.ts:50 —
    /// `State.Queued === this._state`). 实现计划登记的"请求自持 m_queued 位
    /// （append 置/dispatch 清）"由此既有状态机承担：构造即 Queued
    /// （TileRequest.ts:35），dispatch() 翻转（TileRequest.ts:84-85）——无
    /// 需第二位。
    bool isQueued() const noexcept { return m_state == State::Queued; }

    /// True if the request has been canceled.
    /// Ported from: itwinjs-core TileRequest.isCanceled (TileRequest.ts:52-64).
    /// The reference's first clause (`tile.iModel.tiles.isDisposed → true`)
    /// has no DanQing counterpart yet — TODO port with iModel linkage.
    bool isCanceled() const noexcept
    {
        // After we've received the raw tile data, always finish processing it
        // - otherwise tile may end up in limbo (TileRequest.ts:58-60).
        if (m_state == State::Loading)
            return false;

        // If no user cares about this tile any more, we're canceled.
        return m_users.empty();
    }

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
    std::vector<TileUser*> m_users;  // TileRequest.ts:25-28 users（去重见 getUsers）
};

END_DQ_RENDER_NAMESPACE
