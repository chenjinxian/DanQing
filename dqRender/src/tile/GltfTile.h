// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Concrete tile for glTF content
// Ported from: itwinjs-core IModelTile.ts (adapted for glTF)
#pragma once

#include "dqRender/tile/Tile.h"
#include "dqRender/tile/TileRequest.h"

#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class GltfTileTree;
class TileRequestChannel;

// ---------------------------------------------------------------------------
// GltfTile — concrete tile that loads glTF/GLB content
// ---------------------------------------------------------------------------
class GltfTile : public Tile {
public:
    GltfTile(GltfTileTree& tree, Tile* parent,
             dqGeom::Range3d const& range,
             std::string const& contentUri,
             uint32_t depth = 0);
    ~GltfTile() override;

    // --- Tile interface ---
    bool requestContent() override;
    TileContent readContent(uint8_t const* data, size_t dataSize) override;
    void loadChildren() override;

    /// Get the content URI (file path or URL)
    std::string const& getContentUri() const noexcept { return m_contentUri; }

private:
    [[maybe_unused]] GltfTileTree& m_gltfTree;
    std::string m_contentUri;
    bool m_contentRequested = false;
};

END_DQ_RENDER_NAMESPACE
