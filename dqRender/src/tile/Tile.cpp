// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile implementation
// Ported from: itwinjs-core core/frontend/src/tile/Tile.ts
#include "dqRender/tile/Tile.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/tile/TileAdmin.h"  // onTileContentLoaded/Disposed（LRU 入/出册）

#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// The BatchedTile selection form: children REPLACE a too-coarse tile, and the
// closest displayable ancestor stands in for descendants whose content has
// not loaded yet.
// Ported from: BatchedTile.selectTiles (frontend-tiles BatchedTile.ts:76-110,
// esp. :87 closestDisplayableAncestor update and :105-110 stand-in) — the
// pre-protocol selection the former TileTree::selectTilesRecursive hosted
// (moved verbatim here; the reference has no Tile-base selectTiles — the
// concrete tile classes carry their own and the tree shell dispatches on the
// root tile's, IModelTileTree.ts:435-445). DanQing keeps it as the
// Tile::selectTiles default so the Reality/3D Tiles path stays on the old
// behavior while IModelTile's SelectParent protocol lives in
// ImdlTile::selectTiles.
//
// The threaded `closestDisplayableAncestor` parameter is BatchedTile's third
// parameter (BatchedTile.ts:76); DanQing's virtual signature carries
// `numSkipped` instead (the IModelTile protocol's surface), so the ancestor
// threads through this file-local helper — the recursion re-enters the helper
// directly (the former selectTilesRecursive self-call, behavior identical;
// the virtual entry point Tile::selectTiles seeds it with nullptr at the
// root, matching the former TileTree::selectTiles call).
void selectTilesBatchedForm(TileDrawArgs& args, Tile& tile,
                            Tile* closestDisplayableAncestor)
{
    Tile* closest = tile.isDisplayable() ? &tile : closestDisplayableAncestor;

    TileVisibility const vis = tile.getTree().computeVisibility(args, &tile);
    if (vis == TileVisibility::OutsideFrustum)
        return;

    if (vis == TileVisibility::TooCoarse) {
        if (!tile.hasLoadedChildren())
            tile.loadChildren();

        if (!tile.getChildren().empty()) {
            for (auto* child : tile.getChildren())
                if (child)
                    selectTilesBatchedForm(args, *child, closest);
            return;
        }
    }

    // We want to display this tile: request its content if not ready, and
    // display the closest displayable ancestor meanwhile (BatchedTile.ts
    // :105-110 — insertMissing + selected.add(closestDisplayableAncestor)).
    // Ported from: TileDrawArgs.insertMissing/markReady (TileDrawArgs.ts
    // :402-404/:419-421).
    if (!tile.isDisplayable())
        args.insertMissing(&tile);
    if (closest && closest->isDisplayable())
        args.markReady(closest);
}

}  // namespace

// Base default = the BatchedTile form (see selectTilesBatchedForm above). The
// SelectParent protocol surface (selected/numSkipped/return value) is
// IModelTile's — unused here; the form's drawables land in args' ready set
// (the former TileTree::selectTilesRecursive behavior, unchanged).
SelectParent Tile::selectTiles(std::vector<Tile*>& /*selected*/,
                               TileDrawArgs& args, uint32_t /*numSkipped*/)
{
    selectTilesBatchedForm(args, *this, /*closestDisplayableAncestor=*/nullptr);
    return SelectParent::No;
}

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

    // Ported from: Tile.setIsReady (Tile.ts:210-216) — the reference's
    // `_hadGraphics` assignment point: `if (this.hasGraphics)
    // this._hadGraphics = true;`. DanQing's setContent is the content-ready
    // sink the reference's setIsReady serves (TileAdmin::deliverTileContent →
    // readContent → setContent).
    if (m_graphic)
        m_hadGraphics = true;

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
    // Keep the "ever had graphics" flag for the SelectParent protocol's
    // "previously loaded and later unloaded content" trigger (IModelTile.ts
    // :264-265). Guarded: the reference only ever sets _hadGraphics when a
    // graphic existed (Tile.ts:210-216), so a graphic-less tile must not gain
    // it here (prune calls freeMemory unguarded on content-less children,
    // TileTree.cpp pruneRecursive).
    if (m_graphic)
        m_hadGraphics = true;

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
