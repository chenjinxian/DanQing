// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Concrete tile tree for glTF content
// Ported from: itwinjs-core IModelTileTree.ts (adapted for glTF)
#pragma once

#include "GltfTile.h"

#include "dqRender/tile/TileTree.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GltfTileTree — concrete tile tree for loading glTF/GLB files
// ---------------------------------------------------------------------------
class GltfTileTree : public TileTree {
public:
    /// Create a tile tree from a glTF/GLB file
    explicit GltfTileTree(std::string const& filePath);
    ~GltfTileTree() override;

    // --- TileTree interface ---
    TileVisibility computeVisibility(TileDrawArgs& args, Tile* tile) override;

    /// Get the file path
    std::string const& getFilePath() const noexcept { return m_filePath; }

private:
    std::string m_filePath;
};

END_DQ_RENDER_NAMESPACE
