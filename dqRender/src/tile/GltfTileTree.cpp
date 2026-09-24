// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GltfTileTree implementation
// Ported from: itwinjs-core IModelTileTree.ts (adapted for glTF)
#include "GltfTileTree.h"

#include "dqRender/GltfReader.h"

#include <cmath>

BEGIN_DQ_RENDER_NAMESPACE

GltfTileTree::GltfTileTree(std::string const& filePath)
    : TileTree(nullptr, TileLoadPriority::Primary, 20.0f)
    , m_filePath(filePath)
{
    // Create root tile from the glTF file
    // First, load the file to get its bounds
    auto scene = GltfReader::LoadFromFile(filePath);
    if (!scene) {
        setLoadStatus(TileTreeLoadStatus::NotFound);
        return;
    }

    // Create root tile with the scene bounds
    auto rootTile = std::make_unique<GltfTile>(
        *this, nullptr, scene->bounds, filePath, 0);

    // Set as root
    // Note: We need to set the root tile on the base class
    // For now, we'll use the constructor approach

    setLoadStatus(TileTreeLoadStatus::Loaded);
}

GltfTileTree::~GltfTileTree() = default;

TileVisibility GltfTileTree::computeVisibility(TileDrawArgs& /*args*/, Tile* /*tile*/)
{
    // TODO: GltfTileTree never installs its root tile (known-broken shell —
    // see docs/itwinjs-tiles-*.md §3.2); visibility is never consulted.
    return TileVisibility::Visible;
}

END_DQ_RENDER_NAMESPACE
