// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile implementation
// Ported from: itwinjs-core core/frontend/src/tile/Tile.ts
#include "dqRender/tile/Tile.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/tile/TileAdmin.h"  // onTileContentLoaded/Disposed（LRU 入/出册）

#include <algorithm>
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
                            Tile* closestDisplayableAncestor,
                            std::vector<Tile*>& selected)
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
                    selectTilesBatchedForm(args, *child, closest, selected);
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
    if (closest && closest->isDisplayable()) {
        // Dual write (the BatchedTile form's two collections): markReady keeps
        // the ready set as the selection-report/LRU face (TileAdmin sees it at
        // the frame tail, unchanged); selected is what the tree shell draws
        // from (BatchedTile.ts:108-109 `selected.add(closestDisplayableAncestor)`
        // — the stand-in enters selected, the requested tile enters missing).
        // The reference's selected is a Set — siblings sharing a stand-in add
        // it once; DanQing hosts the collection as a deduplicated vector (the
        // TileDrawArgs sets' convention) so the draw list matches 1:1.
        // EQUIVALENCE: 参考源=BatchedTile.ts:88-110（:90 markReady(this) 只在
        // TooCoarse 下钻分支；显示分支 :106-109 只 insertMissing(this) +
        // selected.add(closestDisplayableAncestor)，不 markReady）；发散=DanQing
        // 在 stand-in 落点 markReady(closest)（存量分歧——报告面多记祖先，绘制
        // 面不受影响，draw 已从 selected 取图形）；验证法=TileTreeRender 像素锁
        // 10/10。
        args.markReady(closest);
        if (std::find(selected.begin(), selected.end(), closest) == selected.end())
            selected.push_back(closest);
    }
}

}  // namespace

// Base default = the BatchedTile form (see selectTilesBatchedForm above). The
// SelectParent protocol surface (selected/numSkipped/return value) is
// IModelTile's — unused here; the form's drawables land in BOTH the ready set
// (the report face, the former TileTree::selectTilesRecursive behavior) and
// `selected` (the draw face, BatchedTile.ts:108-109).
SelectParent Tile::selectTiles(std::vector<Tile*>& selected,
                               TileDrawArgs& args, uint32_t /*numSkipped*/)
{
    selectTilesBatchedForm(args, *this, /*closestDisplayableAncestor=*/nullptr,
                           selected);
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
    // NB: m_hadGraphics is deliberately NOT touched here — the reference's
    // unload path never assigns/clears it (the sole assignment point is
    // setIsReady, Tile.ts:210-212 → DanQing setContent); the flag's persistence
    // across unload is exactly the "previously loaded and later unloaded"
    // trigger (IModelTile.ts:264-265). (The brief's freeMemory assignment was
    // dropped at fix round 1: dead code — setContent already set it and
    // nothing ever clears the flag; zero observable difference.)
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
