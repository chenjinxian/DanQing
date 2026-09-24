// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Tile content
// Ported from: itwinjs-core core/frontend/src/tile/TileContent.ts
#pragma once

#include <dqGeom/Range3d.h>

#include <cstdint>
#include <memory>

// Forward declare FeatureTable from dqCommon (separate module).
namespace dqCommon { class FeatureTable; }

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declaration - full definition in RenderGraphic.h
class RenderGraphic;

// ---------------------------------------------------------------------------
// TileContent — result of loading tile content
// Ported from: itwinjs-core TileContent interface
// ---------------------------------------------------------------------------
struct TileContent {
    /// The renderable graphic (nullptr if tile has no geometry)
    std::unique_ptr<RenderGraphic> graphic;

    /// Tighter bounding box of the actual content (may be smaller than tile range)
    dqGeom::Range3d contentRange;

    /// If true, no further subdivision is needed (leaf tile)
    bool isLeaf = false;

    /// If true, content contains point cloud data (affects shader selection)
    bool containsPointCloud = false;

    /// Feature table for per-feature picking and symbology overrides.
    /// Populated from b3dm/i3dm batch table hierarchy.
    std::unique_ptr<dqCommon::FeatureTable> featureTable;

    /// Destructor defined in .cpp where RenderGraphic is complete
    ~TileContent();

    /// Move constructor and assignment
    TileContent(TileContent&&) noexcept = default;
    TileContent& operator=(TileContent&&) noexcept = default;

    /// Default constructor
    TileContent() = default;

    /// Delete copy (unique_ptr)
    TileContent(TileContent const&) = delete;
    TileContent& operator=(TileContent const&) = delete;
};

END_DQ_RENDER_NAMESPACE
