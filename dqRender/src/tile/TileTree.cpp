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

    // Ported from: TileTree.selectTiles (TileTree.ts:136-142) + the tree
    // shells' _selectTiles (IModelTileTree.ts:435-445 — the root tile's own
    // selectTiles drives the recursion, numSkipped starts at 0). The selected
    // tiles' request/display effects flow through args' missing/ready sets
    // (TileDrawArgs.insertMissing/markReady); the TileAdmin report
    // (TileTree.ts:139 addTilesForUser) is batched at the frame tail —
    // registered adaptation at SceneContext.h (TileDrawArgs cannot reach the
    // TileUser; dqApp Viewport::CreateScene feeds addTilesForUser +
    // requestTiles from the collected sets).
    // OMISSION registered (fix round 1, deferred to the Task 5/6 shell work):
    // IModelTileTree._selectTiles also opens with `args.markUsed(this._rootTile)`
    // (IModelTileTree.ts:436 — the root's usage marker stamped every selection).
    // Not ported here: this shell serves the Reality/3D Tiles path whose
    // behavior is frozen at zero-change; land it with the shell's per-tree
    // addTilesForUser when the batched-report adaptation is retired.
    std::vector<Tile*> selected;
    m_rootTile->selectTiles(selected, args, /*numSkipped=*/0);
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
