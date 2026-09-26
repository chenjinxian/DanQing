// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileTree implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileTree.ts
#include "dqRender/tile/TileTree.h"

#include <cmath>
#include <functional>

BEGIN_DQ_RENDER_NAMESPACE

TileTree::TileTree(std::unique_ptr<Tile> rootTile,
                   TileLoadPriority priority,
                   float expirationTime)
    : m_rootTile(std::move(rootTile))
    , m_loadPriority(priority)
    , m_expirationTime(expirationTime)
{
    m_loadStatus = TileTreeLoadStatus::Loaded;
}

TileTree::~TileTree() = default;

void TileTree::collectStatistics(RenderMemory::Statistics& stats)
{
    // Ported from: itwinjs-core TileTree.collectStatistics (TileTree.ts:162-164):
    // this.rootTile.collectStatistics(stats).
    if (m_rootTile)
        m_rootTile->collectStatistics(stats, true);
}

void TileTree::selectTiles(TileDrawArgs& args)
{
    if (!m_rootTile) return;

    // Start recursive selection from root
    selectTilesRecursive(args, m_rootTile.get(), /*closestDisplayableAncestor=*/nullptr);
}

void TileTree::selectTilesRecursive(TileDrawArgs& args, Tile* tile,
                                    Tile* closestDisplayableAncestor)
{
    if (!tile) return;

    // Ported from: BatchedTile.selectTiles (BatchedTile.ts:76-110) —
    // the closest displayable ancestor stands in for descendants whose
    // content has not loaded yet (:87 update, :105-110 stand-in), and a
    // TooCoarse tile with available children is replaced by them (REPLACE).
    Tile* closest = tile->isDisplayable() ? tile : closestDisplayableAncestor;

    TileVisibility const vis = computeVisibility(args, tile);
    if (vis == TileVisibility::OutsideFrustum)
        return;

    if (vis == TileVisibility::TooCoarse) {
        if (!tile->hasLoadedChildren())
            tile->loadChildren();

        if (!tile->getChildren().empty()) {
            for (auto* child : tile->getChildren())
                selectTilesRecursive(args, child, closest);
            return;
        }
    }

    // We want to display this tile: request its content if not ready, and
    // display the closest displayable ancestor meanwhile (BatchedTile.ts
    // :105-110 — insertMissing + selected.add(closestDisplayableAncestor)).
    // Ported from: TileDrawArgs.insertMissing/markReady (TileDrawArgs.ts
    // :402-404/:419-421 — the reference's IModelTile.selectTiles calls these
    // markers instead of writing raw vectors).
    if (!tile->isDisplayable())
        args.insertMissing(tile);
    if (closest && closest->isDisplayable())
        args.markReady(closest);
}

void TileTree::draw(TileDrawArgs& args)
{
    if (!m_rootTile) return;

    // Select tiles first
    selectTiles(args);

    // Collect graphics from ready tiles
    for (auto* tile : args.getReadyTiles()) {
        if (tile->isDisplayable()) {
            args.graphics.push_back(tile->getGraphic());
        }
    }
}

void TileTree::freeContents()
{
    // Recursively release every tile's content graphic while the GL driver
    // is still alive (see header — viewport teardown ordering).
    std::function<void(Tile*)> freeRecursive = [&](Tile* tile) {
        if (!tile) return;
        if (tile->getGraphic())
            tile->freeMemory();
        for (auto* child : tile->getChildren())
            freeRecursive(child);
    };
    freeRecursive(m_rootTile.get());
}

void TileTree::prune(double cutoffSeconds)
{
    // Ported from: TileTree.prune (TileTree.ts:158-160) → Tile.pruneChildren
    // (IModelTile.ts:191-203 — an expired tile's children are discarded).
    // DanQing releases the children's CONTENT graphics and keeps the structure
    // (see header note).
    std::function<void(Tile*)> pruneRecursive = [&](Tile* tile) {
        if (!tile) return;
        if (tile->isTimestampExpired(cutoffSeconds)) {
            for (auto* child : tile->getChildren())
                if (child) child->freeMemory();   // contents only; structure kept
        }
        for (auto* child : tile->getChildren())
            pruneRecursive(child);
    };
    pruneRecursive(m_rootTile.get());
}

END_DQ_RENDER_NAMESPACE
