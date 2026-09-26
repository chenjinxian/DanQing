// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileTree abstract base class
// Ported from: itwinjs-core core/frontend/src/tile/TileTree.ts
#pragma once

#include "Tile.h"
#include "TileDrawArgs.h"
#include "TileLoadPriority.h"

#include <dqGeom/Transform.h>

#include <cstdint>
#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class RenderSystem;

// ---------------------------------------------------------------------------
// TileTree — abstract base class for a hierarchical tile tree
// Ported from: itwinjs-core TileTree.ts
// ---------------------------------------------------------------------------
class TileTree {
public:
    TileTree(std::unique_ptr<Tile> rootTile,
             TileLoadPriority priority = TileLoadPriority::Primary,
             float expirationTime = 20.0f);
    virtual ~TileTree();

    TileTree(TileTree const&) = delete;
    TileTree& operator=(TileTree const&) = delete;

    // --- Accessors ---
    Tile* getRootTile() const noexcept { return m_rootTile.get(); }
    void setRootTile(std::unique_ptr<Tile> root) { m_rootTile = std::move(root); }
    TileLoadPriority getLoadPriority() const noexcept { return m_loadPriority; }
    TileTreeLoadStatus getLoadStatus() const noexcept { return m_loadStatus; }
    float getExpirationTime() const noexcept { return m_expirationTime; }
    bool isLoaded() const noexcept { return m_loadStatus == TileTreeLoadStatus::Loaded; }

    /// Get the transform from tree space to world space
    dqGeom::Transform const& getIModelTransform() const noexcept { return m_iModelTransform; }

    /// Set the tree-to-world transform
    void setIModelTransform(dqGeom::Transform const& transform) noexcept {
        m_iModelTransform = transform;
    }

    /// The render system this tree's tile content is created with.
    /// DanQing adaptation of the reference's explicit-system chain: the reference
    /// passes `system` into Tile.readContent (Tile.ts:429 / ImdlReader create →
    /// system.createBatch), sourced from the global IModelApp.renderSystem.
    /// DanQing's real system is per-viewport (RenderPipeline-owned; the global
    /// RenderSystem::get() is a no-op stub — RenderPipeline.h:101-102), so the
    /// owning Viewport injects it when the tree is attached (AddTileTree) and
    /// readContent implementations take it from the tree.
    RenderSystem* getRenderSystem() const noexcept { return m_renderSystem; }
    void setRenderSystem(RenderSystem* system) noexcept { m_renderSystem = system; }

    // --- Core methods ---

    /// Record graphics memory consumed by this tree (walks the root tile and
    /// its descendants).
    /// Ported from: itwinjs-core TileTree.collectStatistics (TileTree.ts:162-164).
    void collectStatistics(RenderMemory::Statistics& stats);

    /// Select tiles for rendering based on camera/LOD
    /// Populates the args missing/ready sets via TileDrawArgs.insertMissing/
    /// markReady (TileDrawArgs.ts:402-421)
    void selectTiles(TileDrawArgs& args);

    /// Draw the tree (select tiles + collect graphics)
    void draw(TileDrawArgs& args);

    /// Determine the visibility of a tile under the current args (culling +
    /// screen-space-error). Replaces the former selectTile/shouldRefine pair.
    /// Ported from: itwinjs-core Tile.computeVisibility (Tile.ts:429-453) —
    /// concrete trees implement culling and the LOD metric.
    virtual TileVisibility computeVisibility(TileDrawArgs& args, Tile* tile) = 0;

    /// Prune expired tile contents: a tile whose usage timestamp pre-dates
    /// `cutoffSeconds` has its children's contents released. DanQing keeps the
    /// child STRUCTURE (parse-time built, not re-derivable — registered
    /// deviation from the reference's disposable/reloadable children,
    /// IModelTile.pruneChildren IModelTile.ts:191-203).
    /// Ported from: itwinjs-core TileTree.prune (TileTree.ts:158-160 →
    /// rootTile.prune(now - tileExpirationTime)).
    void prune(double cutoffSeconds);

    /// Free the GPU contents of every tile in this tree. Called by the owning
    /// Viewport at teardown, BEFORE its GL driver is destroyed — tile content
    /// graphics (PolyfaceGraphic GPU objects) must die before the driver that
    /// created them (reference: viewport dispose drops its tile contents via
    /// TileAdmin's per-user cleanup / LRU eviction; Tile.disposeContents).
    void freeContents();

    /// Set load status
    void setLoadStatus(TileTreeLoadStatus status) noexcept { m_loadStatus = status; }

    // --- Abstract methods ---

protected:
    /// Recursively select tiles starting from root.
    /// `closestDisplayableAncestor` stands in for descendants whose content
    /// has not loaded yet — Ported from: BatchedTile.selectTiles
    /// (BatchedTile.ts:76-110, esp. :87 and :105-110).
    void selectTilesRecursive(TileDrawArgs& args, Tile* tile,
                              Tile* closestDisplayableAncestor);

private:
    std::unique_ptr<Tile> m_rootTile;
    TileLoadPriority m_loadPriority;
    TileTreeLoadStatus m_loadStatus = TileTreeLoadStatus::NotLoaded;
    float m_expirationTime;
    dqGeom::Transform m_iModelTransform;
    RenderSystem* m_renderSystem = nullptr;  // injected by owning Viewport (not owned)
    [[maybe_unused]] float m_lastUsedTime = 0.0f;
};

END_DQ_RENDER_NAMESPACE
