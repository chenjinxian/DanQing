// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — LRUTileList implementation
// Ported from: itwinjs-core core/frontend/src/tile/LRUTileList.ts
#include "LRUTileList.h"

#include "dqRender/tile/Tile.h"

BEGIN_DQ_RENDER_NAMESPACE

LRUTileList::LRUTileList()
{
    // Sentinel starts as both head and tail (empty list).
    m_head = &m_sentinel;
    m_tail = &m_sentinel;
    m_sentinel.previous = nullptr;
    m_sentinel.next = nullptr;
    m_sentinel.tile = nullptr;
    m_sentinel.bytesUsed = 0;
}

LRUTileList::~LRUTileList() = default;

// ---------------------------------------------------------------------------
// add — insert tile at start of "selected" partition (right after sentinel)
// Ported from: itwinjs-core LRUTileList.add()
// ---------------------------------------------------------------------------
void LRUTileList::add(Tile& tile)
{
    LRUNode& node = getOrCreateNode(tile);
    node.tile = &tile;
    node.bytesUsed = tile.getBytesUsed();

    // Ensure node is not already in the list (defensive).
    if (node.previous || node.next || &node == m_head || &node == m_tail)
        unlink(node);

    // Insert right after sentinel (start of "selected" partition).
    insertAfter(m_sentinel, node);

    m_totalBytesUsed += node.bytesUsed;
    m_tileCount++;
}

// ---------------------------------------------------------------------------
// markUsed — move tiles to end of "selected" partition and register user
// Ported from: itwinjs-core LRUTileList.markUsed()
// ---------------------------------------------------------------------------
void LRUTileList::markUsed(uint32_t userId, std::vector<Tile*> const& tiles)
{
    for (auto* tile : tiles) {
        if (!tile)
            continue;

        auto it = m_nodeMap.find(tile);
        if (it == m_nodeMap.end())
            continue;

        LRUNode& node = it->second;
        node.userIds.insert(userId);

        // Move to end of "selected" (most recently used position).
        moveToSelectedEnd(node);
    }
}

// ---------------------------------------------------------------------------
// clearUsed — remove user from all tiles; move unselected to "not selected"
// Ported from: itwinjs-core LRUTileList.clearUsed()
// ---------------------------------------------------------------------------
void LRUTileList::clearUsed(uint32_t userId)
{
    for (auto& [tile, node] : m_nodeMap) {
        node.userIds.erase(userId);

        // If no users remain, move to "not selected" partition.
        if (node.userIds.empty()) {
            moveToNotSelected(node);
        }
    }
}

// ---------------------------------------------------------------------------
// freeMemory — evict from head of "not selected" until under budget
// Ported from: itwinjs-core LRUTileList.freeMemory()
// ---------------------------------------------------------------------------
void LRUTileList::freeMemory(size_t maxBytes)
{
    while (m_totalBytesUsed > maxBytes && m_head != &m_sentinel) {
        // Head of list is the least recently used "not selected" tile.
        LRUNode* victim = m_head;
        if (!victim || !victim->tile)
            break;

        Tile* tile = victim->tile;
        size_t bytes = victim->bytesUsed;

        // Unlink from list before freeing.
        unlink(*victim);

        // Free the tile's GPU resources.
        tile->freeMemory();

        // Update accounting.
        if (m_totalBytesUsed >= bytes)
            m_totalBytesUsed -= bytes;
        else
            m_totalBytesUsed = 0;
        if (m_tileCount > 0)
            m_tileCount--;

        // Remove from map.
        m_nodeMap.erase(tile);
    }
}

// ---------------------------------------------------------------------------
// drop — remove tile from list (called when content is disposed)
// Ported from: itwinjs-core LRUTileList.drop()
// ---------------------------------------------------------------------------
void LRUTileList::drop(Tile& tile)
{
    auto it = m_nodeMap.find(&tile);
    if (it == m_nodeMap.end())
        return;

    LRUNode& node = it->second;

    // Unlink from the doubly-linked list.
    unlink(node);

    // Update accounting.
    if (m_totalBytesUsed >= node.bytesUsed)
        m_totalBytesUsed -= node.bytesUsed;
    else
        m_totalBytesUsed = 0;
    if (m_tileCount > 0)
        m_tileCount--;

    // Remove from map.
    m_nodeMap.erase(it);
}

// ---------------------------------------------------------------------------
// Linked list primitives
// ---------------------------------------------------------------------------

void LRUTileList::unlink(LRUNode& node)
{
    // Splice node out of the doubly-linked list.
    if (node.previous)
        node.previous->next = node.next;
    if (node.next)
        node.next->previous = node.previous;

    // Update head/tail pointers.
    if (m_head == &node)
        m_head = node.next ? node.next : &m_sentinel;
    if (m_tail == &node)
        m_tail = node.previous ? node.previous : &m_sentinel;

    node.previous = nullptr;
    node.next = nullptr;
}

void LRUTileList::insertAfter(LRUNode& after, LRUNode& node)
{
    node.previous = &after;
    node.next = after.next;
    if (after.next)
        after.next->previous = &node;
    after.next = &node;
    if (m_tail == &after)
        m_tail = &node;
}

void LRUTileList::insertBefore(LRUNode& before, LRUNode& node)
{
    node.next = &before;
    node.previous = before.previous;
    if (before.previous)
        before.previous->next = &node;
    before.previous = &node;
    if (m_head == &before)
        m_head = &node;
}

LRUTileList::LRUNode& LRUTileList::getOrCreateNode(Tile& tile)
{
    auto [it, inserted] = m_nodeMap.try_emplace(&tile);
    if (inserted) {
        it->second.tile = &tile;
        it->second.bytesUsed = tile.getBytesUsed();
    }
    return it->second;
}

bool LRUTileList::isSelected(LRUNode const& node) const
{
    // A node is "selected" if it comes after the sentinel in the list.
    // Walk forward from sentinel; if we reach node before tail, it's selected.
    for (LRUNode* cur = m_sentinel.next; cur != nullptr; cur = cur->next) {
        if (cur == &node)
            return true;
    }
    return false;
}

void LRUTileList::moveToSelectedEnd(LRUNode& node)
{
    // If already at tail (most recently used), nothing to do.
    if (m_tail == &node)
        return;

    unlink(node);
    // Append after the tail = the END of the "selected" partition (layout
    // [not selected: head..sentinel][selected: sentinel..tail]; when the
    // partition is empty m_tail == sentinel and insertAfter lands right
    // after the sentinel = first selected). The previous insertBefore-tail
    // landed second-to-last, and inserted into the NOT-selected partition
    // when m_tail == sentinel (2026-09-21 Debug Info 取证：TileAdmin 诊断
    // 测试的 clear→re-mark 序列暴露）。
    insertAfter(*m_tail, node);
}

void LRUTileList::moveToNotSelected(LRUNode& node)
{
    // Move to just before sentinel (end of "not selected" partition).
    unlink(node);
    insertBefore(m_sentinel, node);
}

void LRUTileList::forEachSelectedTile(std::function<void(Tile&)> const& visit) const
{
    // Ported from: LRUTileList.selectedTiles (generator walking m_tail → sentinel).
    // Layout: [Not Selected (head..sentinel)] [Selected (sentinel..tail)].
    for (LRUNode* cur = m_sentinel.next; cur != nullptr; cur = cur->next) {
        if (cur->tile)
            visit(*cur->tile);
    }
}

void LRUTileList::forEachUnselectedTile(std::function<void(Tile&)> const& visit) const
{
    // Ported from: LRUTileList.unselectedTiles (generator walking sentinel → head).
    for (LRUNode* cur = m_head; cur != nullptr && cur != &m_sentinel; cur = cur->next) {
        if (cur->tile)
            visit(*cur->tile);
    }
}

END_DQ_RENDER_NAMESPACE
