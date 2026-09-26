// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — scene context for tile tree collection
// Ported from: itwinjs-core core/frontend/src/ViewContext.ts (SceneContext)
//
// DanQing adaptation (registered): the reference's SceneContext carries a
// viewport-wide TileDrawArgs template (eye/pixelSize/frustumPlanes filled
// once by the viewport) — the reference builds a per-tree copy via
// TileTreeReference::createDrawArgs. Selection reporting (TileAdmin
// addTilesForUser) stays batched at the frame tail in Viewport::CreateScene
// (equivalence registered 2026-09-21, P7d note) rather than per-tree inside
// TileTree.selectTiles (TileTree.ts:136-142) — DanQing's TileDrawArgs lives in
// dqRender and cannot reach the TileUser.
#pragma once

#include "../Export.h"

#include <dqRender/RenderGraphic.h>
#include <dqRender/tile/Tile.h>
#include <dqRender/tile/TileDrawArgs.h>

#include <algorithm>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class Viewport;

// SceneContext — collects the scene graphics and missing-tile set for one
// viewport scene rebuild, and hands the viewport-level TileDrawArgs template
// to tile tree references.
// Ported from: itwinjs-core SceneContext (ViewContext.ts:330-435 — the
// missing-tile collection :380-434 subset).
class DQ_APP_EXPORT SceneContext {
public:
    SceneContext(Viewport& viewport, dqRender::TileDrawArgs viewportDrawArgs)
        : m_viewport(viewport)
        , m_viewportDrawArgs(std::move(viewportDrawArgs))
    {
    }

    /// The viewport whose scene is being built.
    Viewport& getViewport() const noexcept { return m_viewport; }

    /// Viewport-level draw-args template (eyePos/pixelSizeRatio/frustumPlanes
    /// filled by the viewport; references copy it and set treeToWorld).
    dqRender::TileDrawArgs const& viewportDrawArgs() const noexcept { return m_viewportDrawArgs; }

    /// Add a graphic to the scene.
    /// Ported from: SceneContext.outputGraphic (ViewContext.ts:406-418).
    void outputGraphic(dqRender::RenderGraphic* graphic) { m_graphics.push_back(graphic); }

    /// Record a tile whose content is not yet loaded. A request to load its
    /// contents will later be enqueued (frame-tail requestTiles).
    /// Ported from: SceneContext.insertMissingTile (ViewContext.ts:421-429 —
    /// only NotLoaded/Queued/Loading enter the missing set; Set semantics —
    /// DanQing hosts the set as a deduplicated vector).
    void insertMissingTile(dqRender::Tile& tile)
    {
        switch (tile.getLoadStatus()) {
        case dqRender::TileLoadStatus::NotLoaded:
        case dqRender::TileLoadStatus::Queued:
        case dqRender::TileLoadStatus::Loading:
            if (std::find(m_missingTiles.begin(), m_missingTiles.end(), &tile) ==
                m_missingTiles.end())
                m_missingTiles.push_back(&tile);
            break;
        default:
            break;
        }
    }

    /// Record the tiles a tree selected for display this frame (ready +
    /// requested) — feeds the batched TileAdmin selection report. The
    /// requested (not-yet-ready) tiles enter the missing set through
    /// insertMissingTile's reference load-status gate.
    void collectSelection(std::vector<dqRender::Tile*> const& ready,
                          std::vector<dqRender::Tile*> const& requested)
    {
        m_selectedTiles.insert(m_selectedTiles.end(), ready.begin(), ready.end());
        m_selectedTiles.insert(m_selectedTiles.end(), requested.begin(), requested.end());
        for (auto* tile : requested)
            if (tile)
                insertMissingTile(*tile);
    }

    std::vector<dqRender::RenderGraphic*> const& graphics() const noexcept { return m_graphics; }
    std::vector<dqRender::Tile*> const& missingTiles() const noexcept { return m_missingTiles; }
    std::vector<dqRender::Tile*> const& selectedTiles() const noexcept { return m_selectedTiles; }
    bool hasMissingTiles() const noexcept { return !m_missingTiles.empty() || !m_selectedTiles.empty(); }

private:
    Viewport& m_viewport;
    dqRender::TileDrawArgs m_viewportDrawArgs;
    std::vector<dqRender::RenderGraphic*> m_graphics;
    std::vector<dqRender::Tile*> m_missingTiles;
    std::vector<dqRender::Tile*> m_selectedTiles;
};

END_DQ_APP_NAMESPACE
