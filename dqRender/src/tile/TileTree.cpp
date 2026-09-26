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

void TileTree::selectTiles(TileDrawArgs& args, std::vector<Tile*>& selected)
{
    if (!m_rootTile) return;

    // Ported from: IModelTileTree.ts:435-449（含 root markUsed 与
    // draw-from-tiles）—— _selectTiles 开头 `args.markUsed(this._rootTile)`
    // （:436，每次选择的根使用标记）+ selected 集穿根瓦 selectTiles 递归
    // （:437-438，numSkipped = 0）；TileTree.selectTiles 的汇报契约
    // （TileTree.ts:136-142）。选中瓦的请求/显示效应流经 args 的 missing/ready
    // 集（TileDrawArgs.insertMissing/markReady）；TileAdmin 汇报
    // （TileTree.ts:139 addTilesForUser）批量在帧尾——registered adaptation
    // 见 SceneContext.h（TileDrawArgs 够不到 TileUser；dqApp
    // Viewport::CreateScene 从收集集喂 addTilesForUser + requestTiles）。
    args.markUsed(m_rootTile.get());
    m_rootTile->selectTiles(selected, args, /*numSkipped=*/0);
}

void TileTree::draw(TileDrawArgs& args)
{
    if (!m_rootTile) return;

    // Ported from: IModelTileTree.draw (IModelTileTree.ts:447-449 —
    // `const tiles = this.selectTiles(args); this._rootTile.draw(args, tiles,
    // ...)`; IModelTileTreeRoot.draw :263-272 iterates the tiles and
    // Tile.drawGraphics Tile.ts:503-512 adds each tile's graphic). DanQing
    // hosts that iteration in the tree shell: draw collects the SELECTED
    // tiles' graphics (not the ready set — the SelectParent protocol pushes
    // stand-ins without markReady, IModelTile.ts:250/:319). The isDisplayable
    // filter is the graphic-presence half of Tile.drawGraphics' undefined
    // check (Tile.h's registered Ready+graphic sense).
    std::vector<Tile*> selected;
    selectTiles(args, selected);
    for (Tile* tile : selected)
        if (tile && tile->isDisplayable())
            args.graphics.push_back(tile->getGraphic());
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
