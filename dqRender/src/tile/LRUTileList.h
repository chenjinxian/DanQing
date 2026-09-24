// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — LRU tile list for GPU memory management
// Ported from: itwinjs-core core/frontend/src/tile/LRUTileList.ts
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_set>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Tile;

// ---------------------------------------------------------------------------
// LRUTileList — intrusive doubly-linked list for LRU eviction
// Ported from: itwinjs-core LRUTileList class
//
// Partitions tiles into two sections separated by a sentinel node:
//   [Not Selected] -- sentinel -- [Selected]
//     least-recently-used -->        --> most-recently-used
//
// When GPU memory is over budget, evict from head of "Not Selected".
// Tiles in "Selected" are actively used by at least one viewport.
// Tiles in "Not Selected" are candidates for eviction.
// ---------------------------------------------------------------------------
class LRUTileList {
public:
    LRUTileList();
    ~LRUTileList();

    LRUTileList(LRUTileList const&) = delete;
    LRUTileList& operator=(LRUTileList const&) = delete;

    /// add a tile to the list (inserts at start of "selected" partition).
    /// Ported from: itwinjs-core LRUTileList.add()
    void add(Tile& tile);

    /// Mark tiles as used by a user (moves to end of "selected" partition).
    /// Ported from: itwinjs-core LRUTileList.markUsed()
    void markUsed(uint32_t userId, std::vector<Tile*> const& tiles);

    /// Clear user from all tiles.  Tiles whose userIds become empty are moved
    /// from "selected" to "not selected" partition.
    /// Ported from: itwinjs-core LRUTileList.clearUsed()
    void clearUsed(uint32_t userId);

    /// Free memory until under budget (evicts from "not selected" partition).
    /// Evicts tiles from the head (least recently used) first.
    /// Ported from: itwinjs-core LRUTileList.freeMemory()
    void freeMemory(size_t maxBytes);

    /// Drop a tile from the list (called when content is disposed).
    /// Ported from: itwinjs-core LRUTileList.drop()
    void drop(Tile& tile);

    /// Get total bytes used by all tiles in the list.
    size_t getTotalBytesUsed() const noexcept { return m_totalBytesUsed; }

    /// Get number of tiles in the list.
    size_t getTileCount() const noexcept { return m_tileCount; }

    /// Visit every tile in the "selected" partition (in use by >=1 user).
    /// Ported from: itwinjs-core LRUTileList.selectedTiles iterator.
    void forEachSelectedTile(std::function<void(Tile&)> const& visit) const;

    /// Visit every tile in the "not selected" partition (loaded, no user).
    /// Ported from: itwinjs-core LRUTileList.unselectedTiles iterator.
    void forEachUnselectedTile(std::function<void(Tile&)> const& visit) const;

private:
    // LRU list node — one per tile, managed internally.
    // In itwinjs-core this is embedded in Tile; here we use a map to avoid
    // circular header dependencies.
    struct LRUNode {
        LRUNode* previous = nullptr;
        LRUNode* next = nullptr;
        Tile* tile = nullptr;
        size_t bytesUsed = 0;
        std::unordered_set<uint32_t> userIds;
    };

    // Sentinel node separating "not selected" from "selected".
    // Layout: [Not Selected (head..sentinel)] [Selected (sentinel..tail)]
    LRUNode m_sentinel;

    // Head of the doubly-linked list (start of "not selected" partition).
    LRUNode* m_head = nullptr;

    // Tail of the doubly-linked list (end of "selected" partition).
    LRUNode* m_tail = nullptr;

    // Map from Tile* to LRUNode for O(1) lookup.
    // NOTE: deliberately std::map, NOT bmap. The intrusive doubly-linked list above
    // stores raw LRUNode* into these mapped values; std::map guarantees stable mapped-
    // value addresses (node-based, heap-allocated), so those pointers stay valid across
    // insertion. bmap (B-tree) relocates values on node split, which would dangle them
    // (UB). This is a §8.3 exception by technical necessity, registered as TD-10.
    // The TS reference (LRUTileList.ts) embeds the node in Tile instead; this C++ port
    // uses the map to avoid a circular header dependency on Tile.
    std::map<Tile*, LRUNode> m_nodeMap;

    size_t m_totalBytesUsed = 0;
    size_t m_tileCount = 0;

    // --- Linked list primitives ---

    /// Splice node out of the list (connects previous to next).
    void unlink(LRUNode& node);

    /// Insert node immediately after the given node.
    void insertAfter(LRUNode& after, LRUNode& node);

    /// Insert node immediately before the given node.
    void insertBefore(LRUNode& before, LRUNode& node);

    /// Get or create the LRUNode for a tile.
    LRUNode& getOrCreateNode(Tile& tile);

    /// Check if a node is in the "selected" partition (after sentinel).
    bool isSelected(LRUNode const& node) const;

    /// Move a node to the end of the "selected" partition (most recently used).
    void moveToSelectedEnd(LRUNode& node);

    /// Move a node to the "not selected" partition (just before sentinel).
    void moveToNotSelected(LRUNode& node);
};

END_DQ_RENDER_NAMESPACE
