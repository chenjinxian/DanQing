// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Reality tile for 3D Tiles content
// Ported from: itwinjs-core core/frontend/src/tile/RealityTile.ts
#pragma once

#include "dqRender/tile/Tile.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class RealityTileTree;

// Bounding volume types for 3D Tiles.
// Ported from: itwinjs-core BoundingVolume types
enum class BoundingVolumeType : uint8_t {
    None = 0,
    Box,        // 12 floats: center + 3 axes
    Sphere,     // 4 floats: center + radius
    Region,     // 6 floats: west, south, east, north, minHeight, maxHeight
};

// Bounding volume data.
struct BoundingVolume {
    BoundingVolumeType type = BoundingVolumeType::None;
    std::vector<float> values;  // Interpretation depends on type
};

// ---------------------------------------------------------------------------
// RealityTile — a tile in a 3D Tiles tileset
// Ported from: itwinjs-core RealityTile
// ---------------------------------------------------------------------------
class RealityTile : public Tile {
public:
    RealityTile(RealityTileTree& tree, Tile* parent,
                BoundingVolume const& boundingVolume,
                float geometricError,
                std::string const& contentUri,
                dqGeom::Range3d const& range);

    // --- 3D Tiles specific ---
    BoundingVolume const& getBoundingVolume() const noexcept { return m_boundingVolume; }
    float getGeometricError() const noexcept { return m_geometricError; }
    std::string const& getContentUri() const noexcept { return m_contentUri; }
    bool hasContent() const noexcept override { return !m_contentUri.empty(); }

    // --- Tile overrides ---
    bool requestContent() override;
    TileContent readContent(uint8_t const* data, size_t dataSize) override;
    void loadChildren() override;

private:
    BoundingVolume m_boundingVolume;
    float m_geometricError = 0.0f;
    std::string m_contentUri;
};

END_DQ_RENDER_NAMESPACE
