// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile implementation
// Ported from: itwinjs-core core/frontend/src/tile/Tile.ts
#include "dqRender/tile/Tile.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/tile/TileAdmin.h"  // onTileContentLoaded/Disposed（LRU 入/出册）

BEGIN_DQ_RENDER_NAMESPACE

Tile::Tile(TileTree& tree, Tile* parent, dqGeom::Range3d const& range,
           uint32_t depth, double maximumSize)
    : m_tree(tree)
    , m_parent(parent)
    , m_range(range)
    , m_depth(depth)
    , m_maximumSize(maximumSize)
{
    // Compute bounding sphere from range
    // Ported from: itwinjs-core Tile.ts constructor
    auto center = range.Center();
    m_boundingSphere.center[0] = static_cast<float>(center.x);
    m_boundingSphere.center[1] = static_cast<float>(center.y);
    m_boundingSphere.center[2] = static_cast<float>(center.z);

    auto diagonal = range.Diagonal();
    float maxExtent = static_cast<float>(std::max({diagonal.x, diagonal.y, diagonal.z}));
    m_boundingSphere.radius = maxExtent * 0.5f;
}

Tile::~Tile()
{
    // Children are owned by m_ownedChildren.
    // Content teardown must unregister from the TileAdmin's LRU — otherwise
    // the list keeps a dangling pointer and later traversals crash (exposed
    // by TileAdminMemoryTest cross-test state, 2026-09-21). The reference
    // does this in Tile.dispose → onTileContentDisposed (Tile.ts:160-166).
    if (TileAdmin::hasInstance())
        TileAdmin::instance().onTileContentDisposed(*this);
}

void Tile::setContent(TileContent content)
{
    // Ported from: itwinjs-core Tile.ts setContent
    m_graphic = std::move(content.graphic);
    m_isLeaf = content.isLeaf;

    if (m_graphic) {
        m_loadStatus = TileLoadStatus::Ready;
    } else {
        m_loadStatus = TileLoadStatus::Ready;
        // Empty tile (no geometry) — still "ready" but not displayable
    }
    // Content landed → register in TileAdmin's loaded-tile LRU
    // (TileAdmin.onTileContentLoaded, TileAdmin.ts:759-765 — called from the
    // reference's Tile content-ready path).
    TileAdmin::instance().onTileContentLoaded(*this);
}

void Tile::setNotFound()
{
    m_loadStatus = TileLoadStatus::NotFound;
    m_graphic.reset();
}

void Tile::freeMemory()
{
    // Ported from: itwinjs-core Tile.ts freeMemory
    m_graphic.reset();
    m_loadStatus = TileLoadStatus::Abandoned;
    m_bytesUsed = 0;
    // Content disposed → unregister from the LRU (TileAdmin.ts:770-773).
    TileAdmin::instance().onTileContentDisposed(*this);
}

void Tile::setChildren(std::vector<std::unique_ptr<Tile>> children)
{
    m_ownedChildren = std::move(children);
    m_children.clear();
    m_children.reserve(m_ownedChildren.size());
    for (auto const& child : m_ownedChildren) {
        m_children.push_back(child.get());
    }
}

void Tile::collectStatistics(RenderMemory::Statistics& stats, bool includeChildren)
{
    // Ported from: itwinjs-core Tile.collectStatistics (Tile.ts:335-349):
    //   graphic walk + _collectStatistics (subclass hook — DanQing tiles carry
    //   no extra resources yet) + recursive children when includeChildren.
    if (m_graphic)
        m_graphic->collectStatistics(stats);

    if (!includeChildren)
        return;

    for (auto* child : m_children)
        if (child)
            child->collectStatistics(stats, true);
}

END_DQ_RENDER_NAMESPACE
