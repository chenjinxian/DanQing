// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile admin coordinator
// Ported from: itwinjs-core core/frontend/src/tile/TileAdmin.ts
#pragma once

#include "Tile.h"
#include "TileRequestChannel.h"
#include "TileTree.h"

#include <dqBase/DqEvent.h>

// Forward declare — §7.2: PublicAPI contains zero Qt types
namespace dqRender { class ITileFetcher; class LRUTileList; }

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TileUser — interface for anything that uses tiles (typically a Viewport)
// Ported from: itwinjs-core TileUser interface
// ---------------------------------------------------------------------------
class TileUser {
public:
    virtual ~TileUser() = default;

    /// Get unique user ID
    virtual uint32_t getTileUserId() const = 0;

    /// Disclose tile trees in use (called during tile selection)
    virtual void discloseTileTrees(std::vector<TileTree*>& trees) = 0;
};

// ---------------------------------------------------------------------------
// TileAdmin — central tile processing coordinator
// Ported from: itwinjs-core TileAdmin class
// ---------------------------------------------------------------------------
class TileAdmin {
public:
    TileAdmin();
    ~TileAdmin();

    TileAdmin(TileAdmin const&) = delete;
    TileAdmin& operator=(TileAdmin const&) = delete;

    /// Get the global TileAdmin instance
    static TileAdmin& instance();

    /// Whether a TileAdmin instance exists (guards ~Tile's LRU unregister
    /// against static-destruction ordering — the default instance may be gone
    /// while tiles still die).
    static bool hasInstance() noexcept { return sInstance != nullptr; }

    /// Process tile requests (called each frame from Application::EventLoop)
    /// Ported from: itwinjs-core TileAdmin.process()
    void process();

    // --- User management ---
    void registerUser(TileUser& user);
    void forgetUser(TileUser& user);

    // --- Tile selection ---
    /// add tiles selected by a user for rendering
    void addTilesForUser(TileUser& user,
                         std::vector<Tile*> const& selected,
                         std::vector<Tile*> const& ready,
                         std::vector<Tile*> const& touched);

    // Specifies the set of tiles currently requested for use by a TileUser.
    // This set replaces any previously specified for the same user. The
    // requests are not actually processed until the next call to process().
    // This is typically invoked when a viewport recreates its scene (the
    // frame-tail requestMissingTiles, Viewport.ts:2656 →
    // SceneContext.requestMissingTiles, ViewContext.ts:432-434).
    // Ported from: TileAdmin.requestTiles (TileAdmin.ts:493-500 —
    // _requestsPerUser.set(user, tiles)).
    void requestTiles(TileUser& user, std::vector<Tile*> const& tiles);

    // --- Events ---
    dqBase::DqEvent<Tile&> onTileLoad;
    dqBase::DqEvent<TileTree&> onTileTreeLoad;
    dqBase::DqEvent<Tile&> onTileChildrenLoad;

    // --- Statistics ---
    uint32_t getActiveRequestCount() const;
    size_t getPendingRequestCount() const;
    uint32_t getRegisteredUserCount() const noexcept { return static_cast<uint32_t>(m_users.size()); }

    // Statistics about time spent decoding tile content.
    // Ported from: itwinjs-core TileContentDecodingStatistics (TileRequestChannel.ts:28-37).
    struct DecodingStatistics {
        double total = 0.0;  // total milliseconds spent decoding content
        double mean = 0.0;   // mean milliseconds per decode
        double max = 0.0;    // longest single decode (ms)
        double min = 0.0;    // shortest single decode (ms)
    };

    // Session-lifetime statistics for the diagnostics panel.
    // Ported from: itwinjs-core TileAdmin.Statistics (TileAdmin.ts:983-1016 — the
    // TileStatisticsTracker data source, field by field).
    struct Statistics {
        uint32_t numActiveRequests = 0;      // currently in flight (mirror of getActiveRequestCount)
        uint32_t numPendingRequests = 0;     // queued (mirror of getPendingRequestCount)
        uint32_t numCanceled = 0;            // canceled while pending
        uint32_t totalCompletedRequests = 0;
        uint32_t totalFailedRequests = 0;
        uint32_t totalTimedOutRequests = 0;
        uint32_t totalEmptyTiles = 0;        // completed requests producing an empty tile
        uint32_t totalUndisplayableTiles = 0; // completed requests producing an undisplayable tile
        uint32_t totalElidedTiles = 0;       // tiles elided (content not requested — coarser tile used)
        uint32_t totalCacheMisses = 0;       // tile content not found in the cache
        uint32_t totalDispatchedRequests = 0;
        uint32_t totalAbortedRequests = 0;
        DecodingStatistics decoding;         // content-decode timings
    };
    Statistics const& statistics() const noexcept { return m_statistics; }
    // TileStatisticsTracker.ts:169 (resetStatistics — resets cumulative totals;
    // reference: channels.resetStatistics() + _totalElided = 0).
    void resetStatistics() noexcept;

    // Stat hooks — called from the queue lifecycle (.cpp). Counters live on
    // m_statistics so the panel reads them without touching the queue lock.
    void recordDispatched() noexcept { ++m_statistics.totalDispatchedRequests; }
    void recordCompleted() noexcept { ++m_statistics.totalCompletedRequests; }
    void recordFailed() noexcept { ++m_statistics.totalFailedRequests; }
    void recordCanceled() noexcept { ++m_statistics.numCanceled; }
    void recordAborted() noexcept { ++m_statistics.totalAbortedRequests; }
    void recordTimedOut() noexcept { ++m_statistics.totalTimedOutRequests; }
    void recordEmptyTile() noexcept { ++m_statistics.totalEmptyTiles; }
    void recordUndisplayableTile() noexcept { ++m_statistics.totalUndisplayableTiles; }
    void recordElidedTile() noexcept { ++m_statistics.totalElidedTiles; }
    void recordCacheMiss() noexcept { ++m_statistics.totalCacheMisses; }
    // Record one content-decode duration (ms) into the decoding statistics.
    // Ported from: TileRequestChannel content decode timing → TileContentDecodingStatistics.
    void recordDecodingTime(double milliseconds) noexcept;

    // --- Per-user tile sets (TileMemoryBreakdown / MemoryTracker data source) ---

    // Statistics about tiles that are handled externally from TileAdmin.
    // Ported from: itwinjs-core ExternalTileStatistics (TileAdmin.ts internal).
    struct ExternalTileStatistics {
        uint32_t selected = 0;
        uint32_t requested = 0;
        uint32_t ready = 0;
    };

    // Two sets of tiles associated with a user — typically a viewport's scene.
    // Ported from: itwinjs-core SelectedAndReadyTiles (TileAdmin.ts:155-ish).
    // (C++ form: out-param + bool; TS returns `SelectedAndReadyTiles | undefined`.)
    struct SelectedAndReadyTiles {
        std::vector<Tile*> const* ready = nullptr;      // nullptr when the entry exists but is empty
        std::vector<Tile*> const* selected = nullptr;
        ExternalTileStatistics external;
    };
    // Returns false if no entry exists for the user.
    // Ported from: TileAdmin.ts:506 getTilesForUser (do not modify the sets).
    bool getTilesForUser(TileUser const& user, SelectedAndReadyTiles& out) const;

    // The number of pending and active requests associated with a user.
    // Ported from: TileAdmin.ts:474 getNumRequestsForUser.
    size_t getNumRequestsForUser(TileUser const& user) const;

    // Disclose external-tile statistics for a user (adds into the entry).
    // Ported from: TileAdmin.ts:536 addExternalTilesForUser.
    void addExternalTilesForUser(TileUser& user, ExternalTileStatistics const& statistics);

    // Clears the sets of tiles associated with a TileUser.
    // Ported from: TileAdmin.ts:550-555 clearTilesForUser (entry delete +
    // LRU clearUsed — the demotion path: a viewport rebuilds its selection by
    // clearTilesForUser then addTilesForUser).
    void clearTilesForUser(TileUser& user);

    // --- Loaded-tile memory accounting (LRU) ---

    // Total bytes of GPU memory allocated to Tile contents.
    // Ported from: TileAdmin.ts:383 totalTileContentBytes (LRU list totalBytesUsed).
    size_t totalTileContentBytes() const;

    // Visit tiles with content loaded AND in use by some user.
    // Ported from: TileAdmin.ts:465 selectedLoadedTiles.
    void forEachSelectedLoadedTile(std::function<void(Tile&)> const& visit) const;

    // Visit tiles with content loaded but used by no user.
    // Ported from: TileAdmin.ts:458 unselectedLoadedTiles.
    void forEachUnselectedLoadedTile(std::function<void(Tile&)> const& visit) const;

    // Free the GPU contents of every loaded-but-unselected tile (LRU "not
    // selected" partition), dropping each from the LRU accounting. This is the
    // debug-purge path (ViewManager.purgeTileTrees → Tiles.purge equivalent).
    void purgeUnselectedTileContents();

    // Invoked when a Tile marks itself "ready" (content loaded). If the tile
    // has content it is added to the LRU list of tiles with content; the
    // onTileLoad event is raised.
    // Ported from: TileAdmin.ts:759-765 onTileContentLoaded.
    void onTileContentLoaded(Tile& tile);

    // Invoked when a Tile's content is disposed — removed from the LRU list.
    // Ported from: TileAdmin.ts:770-773 onTileContentDisposed.
    void onTileContentDisposed(Tile& tile);

    // --- Completion sink (polling-fetcher model) ---
    // The fetcher delivers content bytes here; the request is settled with its
    // channel, then bytes → tile.readContent → tile.setContent.
    // Ported from: itwinjs-core TileRequest.handleResponse (TileRequest.ts:156-195
    // — response data → tile.readContent → tile.setContent → channel.recordCompletion).
    // In the reference the promise resolves inside TileRequest.dispatch; DanQing's
    // polling ITileFetcher delivers on a later process() cycle, so the sink lives
    // on TileAdmin and settles the in-flight request via the tile's request hook.
    void deliverTileContent(Tile& tile, std::vector<uint8_t> const& data);

    // Fetch-failure sink: settle the request and mark the tile NotFound.
    // Ported from: TileRequest.ts:169-178 (error → tile.setNotFound) +
    // channel failure accounting.
    void reportTileFetchError(Tile& tile, std::string const& error);

    // --- Async fetcher ---
    /// Get the tile request fetcher for async HTTP content loading.
    ITileFetcher& getFetcher() { return *m_fetcher; }

    /// Replace the fetcher (DI: app injects platform-specific implementation at startup).
    void setFetcher(std::unique_ptr<ITileFetcher> fetcher);

    // --- Configuration ---
    void setMaxConcurrentRequests(uint32_t max) { m_maxConcurrentRequests = max; }
    uint32_t getMaxConcurrentRequests() const noexcept { return m_maxConcurrentRequests; }

    // Total GPU memory budget for tile contents. When exceeded, the LRU's
    // not-selected partition is evicted until back under budget.
    // Ported from: TileAdmin.Props.gpuMemoryLimits / _maxTotalTileContentBytes
    // (TileAdmin.ts:1322-1326 — desktop "default" 1GB; 0 disables the limit).
    void setMaxTotalTileContentBytes(size_t maxBytes) noexcept { m_maxTotalTileContentBytes = maxBytes; }
    size_t getMaxTotalTileContentBytes() const noexcept { return m_maxTotalTileContentBytes; }

    // Evict not-selected tile contents until under the memory budget.
    // Ported from: TileAdmin.freeMemory (TileAdmin.ts:843-847 —
    // `_lruList.freeMemory(this._maxTotalTileContentBytes)`).
    void freeMemory();

    // Seconds after which an unused tile's children are pruned
    // (tileExpirationTime, TileAdmin.ts:302-303 — default 20s, clamp [5,60]).
    void setTileExpirationTime(double seconds) noexcept;
    double getTileExpirationTime() const noexcept { return m_tileExpirationTime; }

    // Test seam for time-based pruning (the reference tests use fake timers;
    // DanQing's clock funnels through nowSeconds()).
    // Authored: test affordance, no reference equivalent beyond sinon usage.
    static double nowSeconds();
    static void setNowOverrideForTest(std::optional<double> seconds);
    static void clearNowOverrideForTest() { setNowOverrideForTest(std::nullopt); }

private:
    /// Process the request queue
    void processQueue();

    /// Process requests for a specific user
    void processRequestsForUser(TileUser& user);

    /// Prune expired tiles and purge unused trees
    void pruneAndPurge();

    TileRequestChannels m_channels;
    std::unique_ptr<ITileFetcher> m_fetcher;
    std::unordered_set<TileUser*> m_users;
    std::unordered_map<TileUser*, std::vector<Tile*>> m_requestedTiles;
    Statistics m_statistics;
    std::unordered_map<TileUser*, std::vector<Tile*>> m_selectedTiles;
    std::unordered_map<TileUser*, std::vector<Tile*>> m_readyTiles;
    // Loaded-tile LRU accounting (TileAdmin.ts:81-84 _lruList). Pimpl: the
    // LRUTileList type is internal (dqRender/src/tile), the PublicAPI header
    // only forward-declares it.
    std::unique_ptr<LRUTileList> m_lruList;
    // Per-user external-tile statistics (TileAdmin.ts _externalTiles —
    // addExternalTilesForUser storage).
    std::unordered_map<TileUser const*, ExternalTileStatistics> m_externalTiles;
    // Decoding-statistics accumulation backing (mean/min need count).
    uint32_t m_numDecodes = 0;

    static TileAdmin* sInstance;  // definition in .cpp (instance tracking)

    uint32_t m_maxConcurrentRequests = 10;
    // GPU memory budget for tile contents (TileAdmin.ts:1322-1326 desktop
    // "default" = 1GB; 0 = unlimited).
    size_t m_maxTotalTileContentBytes = 1024ull * 1024ull * 1024ull;
    double m_tileExpirationTime = 20.0;   // TileAdmin.ts:303 default
    double m_nextPruneTime = 0.0;         // _nextPruneTime throttle (:313)
    static std::optional<double> s_nowOverride;  // test clock override
    [[maybe_unused]] float m_lastPruneTime = 0.0f;
};

END_DQ_RENDER_NAMESPACE
