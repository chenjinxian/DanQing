// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile abstract base class
// Ported from: itwinjs-core core/frontend/src/tile/Tile.ts
#pragma once

#include "TileContent.h"
#include "TileLoadPriority.h"
#include "TileLoadStatus.h"

#include <dqRender/RenderMemory.h>  // collectStatistics payload
#include <dqGeom/Range3d.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class TileTree;
class RenderGraphic;
class TileRequest;

// ---------------------------------------------------------------------------
// BoundingSphere — simple bounding sphere for fast culling
// ---------------------------------------------------------------------------
struct BoundingSphere {
    std::array<float, 3> center = {0.0f, 0.0f, 0.0f};
    float radius = 0.0f;
};

// ---------------------------------------------------------------------------
// Tile — abstract base class for a single tile in the hierarchy
// Ported from: itwinjs-core Tile.ts
// ---------------------------------------------------------------------------
class Tile {
public:
    Tile(TileTree& tree, Tile* parent, dqGeom::Range3d const& range,
         uint32_t depth = 0);
    virtual ~Tile();

    Tile(Tile const&) = delete;
    Tile& operator=(Tile const&) = delete;

    // --- Accessors ---
    TileTree& getTree() const noexcept { return m_tree; }
    Tile* getParent() const noexcept { return m_parent; }
    dqGeom::Range3d const& getRange() const noexcept { return m_range; }
    BoundingSphere const& getBoundingSphere() const noexcept { return m_boundingSphere; }
    uint32_t getDepth() const noexcept { return m_depth; }
    TileLoadStatus getLoadStatus() const noexcept { return m_loadStatus; }

    /// Get the loaded graphic (nullptr if not ready)
    RenderGraphic* getGraphic() const noexcept { return m_graphic.get(); }

    /// Get child tiles (empty if not loaded)
    std::vector<Tile*> const& getChildren() const noexcept { return m_children; }

    /// Check if tile has a graphic ready to display
    bool isDisplayable() const noexcept {
        return m_loadStatus == TileLoadStatus::Ready && m_graphic != nullptr;
    }

    /// Whether this tile has content that can be loaded and displayed.
    /// Content-less tiles are legal 3D Tiles LOD intermediates (grouping
    /// nodes) — selection descends through them instead of displaying them
    /// (reference: RealityTile.selectRealityTiles traversal, RealityTile.ts
    /// :340-390). Base default: tiles have content.
    virtual bool hasContent() const noexcept { return true; }

    /// Check if tile is a leaf (no children possible)
    bool isLeaf() const noexcept { return m_isLeaf; }

    /// Get bytes used by GPU resources (for LRU eviction)
    size_t getBytesUsed() const noexcept { return m_bytesUsed; }

    /// Last-use timestamp in seconds (TileUsageMarker's timestamp part —
    /// the in-use test lives in TileAdmin's LRU selection sets).
    /// Ported from: itwinjs-core TileUsageMarker (TileUsageMarker.ts:22-45 —
    /// mark/isTimestampExpired subset).
    double getLastUsedTime() const noexcept { return m_lastUsedTime; }
    void markUsed(double nowSeconds) noexcept { m_lastUsedTime = nowSeconds; }
    bool isTimestampExpired(double cutoffSeconds) const noexcept
    {
        return m_lastUsedTime < cutoffSeconds;
    }

    /// The in-flight request for this tile's content, if any.
    /// Ported from: itwinjs-core Tile.ts:246-250 (`this.request = request` —
    /// the TileRequest hook; loadStatus getter composes `_state` with
    /// `request.state`, Tile.ts:271-295). DanQing adaptation: the hook is an
    /// explicit pointer the completion sink (TileAdmin::deliverTileContent)
    /// uses to settle the request with its channel.
    TileRequest* getRequest() const noexcept { return m_request; }
    void setRequest(TileRequest* request) noexcept { m_request = request; }

    /// Explicit load-status transition (Queued/Loading while a request is in
    /// flight). The reference composes this from `request.state` inside the
    /// `loadStatus` getter (Tile.ts:271-295); DanQing stores it directly —
    /// TileRequestChannel::process marks Loading at dispatch so
    /// TileAdmin::processRequestsForUser does not re-request in-flight tiles.
    void setLoadStatus(TileLoadStatus status) noexcept { m_loadStatus = status; }

    /// Disclose resources owned by this tile and (by default) all of its
    /// child tiles.
    /// Ported from: itwinjs-core Tile.collectStatistics (Tile.ts:335-349 —
    /// graphic walk + _collectStatistics + recursive children; the
    /// includeChildren=false form is the TileMemoryTracer per-tile query).
    void collectStatistics(RenderMemory::Statistics& stats, bool includeChildren = true);

    // --- Content lifecycle ---
    /// Set tile content (transitions to Ready)
    void setContent(TileContent content);

    /// Mark tile as not found
    void setNotFound();

    /// Free GPU resources (transitions to Abandoned)
    void freeMemory();

    /// Compute load priority (lower = higher priority)
    virtual uint32_t computeLoadPriority() const { return m_depth; }

    // --- Abstract methods (must be implemented by concrete tiles) ---

    /// Fetch raw tile content from data source
    /// @return true if request was initiated, false if already loading
    virtual bool requestContent() = 0;

    /// Deserialize raw data into TileContent
    /// @param data Raw bytes from data source
    /// @param dataSize Size of data in bytes
    /// @return TileContent with graphic and metadata
    virtual TileContent readContent(uint8_t const* data, size_t dataSize) = 0;

    /// Load child tiles (called when tile is selected for refinement)
    virtual void loadChildren() = 0;

    /// Check if children have been loaded
    virtual bool hasLoadedChildren() const { return !m_children.empty(); }

    /// Set child tiles (called by loadChildren implementation or tree construction)
    void setChildren(std::vector<std::unique_ptr<Tile>> children);

protected:
    /// Set leaf flag
    void setIsLeaf(bool isLeaf) noexcept { m_isLeaf = isLeaf; }

    /// Set bytes used
    void setBytesUsed(size_t bytes) noexcept { m_bytesUsed = bytes; }

private:
    TileTree& m_tree;
    Tile* m_parent;
    dqGeom::Range3d m_range;
    BoundingSphere m_boundingSphere;
    uint32_t m_depth;
    TileLoadStatus m_loadStatus = TileLoadStatus::NotLoaded;
    bool m_isLeaf = false;
    size_t m_bytesUsed = 0;

    std::unique_ptr<RenderGraphic> m_graphic;
    std::vector<Tile*> m_children;  // Owned by tree
    std::vector<std::unique_ptr<Tile>> m_ownedChildren;
    TileRequest* m_request = nullptr;  // in-flight request (reference: tile.request)
    double m_lastUsedTime = 0.0;      // TileUsageMarker timestamp (seconds)
};

END_DQ_RENDER_NAMESPACE
