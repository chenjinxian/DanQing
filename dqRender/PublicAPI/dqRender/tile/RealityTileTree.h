// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Reality tile tree for 3D Tiles tilesets
// Ported from: itwinjs-core core/frontend/src/tile/RealityTileTree.ts
#pragma once

#include "dqRender/tile/RealityTile.h"
#include "dqRender/tile/TileTree.h"

#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Parsed tileset.json node.
// Ported from: itwinjs-core tileset JSON schema
struct TilesetNode {
    BoundingVolume boundingVolume;
    float geometricError = 0.0f;
    std::string contentUri;
    std::string refine;  // "ADD" or "REPLACE"
    std::vector<TilesetNode> children;
};

// ---------------------------------------------------------------------------
// RealityTileTree — a tile tree loaded from a 3D Tiles tileset.json
// Ported from: itwinjs-core RealityTileTree
// ---------------------------------------------------------------------------
class RealityTileTree : public TileTree {
public:
    RealityTileTree(std::unique_ptr<Tile> rootTile,
                    std::string const& tilesetUrl);

    // --- TileTree overrides ---
    TileVisibility computeVisibility(TileDrawArgs& args, Tile* tile) override;

    // --- 3D Tiles specific ---

    /// Parse a tileset.json and create a RealityTileTree.
    /// Ported from: itwinjs-core RealityTileLoader.loadTileset()
    static std::unique_ptr<RealityTileTree> loadTileset(
        std::string const& tilesetUrl,
        uint8_t const* jsonData, size_t jsonSize);

    /// Get the tileset URL.
    std::string const& getTilesetUrl() const noexcept { return m_tilesetUrl; }

    /// Resolve a content URI relative to the tileset URL.
    /// If the URI is already absolute (starts with "http" or "/"), returns as-is.
    /// Otherwise, prepends the tileset base directory.
    /// Ported from: itwinjs-core resolveContentUrl()
    std::string resolveContentUri(std::string const& contentUri) const;

private:
    /// Recursively create tiles from a tileset node.
    static std::unique_ptr<RealityTile> createTile(
        RealityTileTree& tree, Tile* parent,
        TilesetNode const& node);

    std::string m_tilesetUrl;
};

END_DQ_RENDER_NAMESPACE
