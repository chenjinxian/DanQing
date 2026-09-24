// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GltfTile implementation
// Ported from: itwinjs-core IModelTile.ts (adapted for glTF)
#include "GltfTile.h"
#include "GltfTileTree.h"

#include "dqRender/GltfReader.h"
#include "dqRender/RenderSystem.h"

#include <dqCommon/FeatureTable.h>

#include <fstream>

BEGIN_DQ_RENDER_NAMESPACE

GltfTile::GltfTile(GltfTileTree& tree, Tile* parent,
                   dqGeom::Range3d const& range,
                   std::string const& contentUri,
                   uint32_t depth)
    : Tile(tree, parent, range, depth)
    , m_gltfTree(tree)
    , m_contentUri(contentUri)
{
}

GltfTile::~GltfTile() = default;

bool GltfTile::requestContent()
{
    // Ported from: itwinjs-core Tile.ts requestContent
    if (m_contentRequested) return false;
    if (getLoadStatus() != TileLoadStatus::NotLoaded) return false;

    m_contentRequested = true;

    // For file-based tiles, read the file directly
    // In a real implementation, this would be async via TileRequestChannel
    std::ifstream file(m_contentUri, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        setNotFound();
        return false;
    }

    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        setNotFound();
        return false;
    }

    // Read content synchronously (for now)
    auto content = readContent(buffer.data(), buffer.size());
    setContent(std::move(content));

    return true;
}

TileContent GltfTile::readContent(uint8_t const* data, size_t dataSize)
{
    // Ported from: itwinjs-core Tile.ts readContent
    TileContent content;

    // Use GltfReader to parse the glTF/GLB data
    auto scene = GltfReader::LoadFromMemory(data, dataSize, "");
    if (!scene) {
        return content;
    }

    // Create graphics from the scene. The system comes from the tree
    // (reference passes `system` into readContent explicitly — Tile.ts:429;
    // DanQing's per-viewport system is injected into the tree by the owning
    // Viewport, see TileTree::setRenderSystem — the global RenderSystem::get()
    // is a no-op stub).
    RenderSystem* renderSystem = getTree().getRenderSystem();
    if (!renderSystem)
        return content;
    std::vector<RenderGraphic*> graphics;

    for (auto const& mesh : scene->meshes) {
        if (!mesh.polyface) continue;

        // Compute packed RGBA from baseColorFactor
        uint32_t r = static_cast<uint32_t>(mesh.baseColorFactor[0] * 255.0f);
        uint32_t g = static_cast<uint32_t>(mesh.baseColorFactor[1] * 255.0f);
        uint32_t b = static_cast<uint32_t>(mesh.baseColorFactor[2] * 255.0f);
        uint32_t a = static_cast<uint32_t>(mesh.baseColorFactor[3] * 255.0f);
        uint32_t color = (a << 24) | (b << 16) | (g << 8) | r;

        auto* graphic = renderSystem->createGraphicFromPolyface(
            mesh.polyface.Get(), color, 0);
        if (graphic) {
            graphics.push_back(graphic);
        }
    }

    if (!graphics.empty()) {
        content.graphic.reset(renderSystem->createGraphicList(std::move(graphics)));
    }

    // Set content range from scene bounds
    content.contentRange = scene->bounds;
    content.isLeaf = true;  // glTF files are typically leaf tiles

    return content;
}

void GltfTile::loadChildren()
{
    // glTF tiles are typically leaf tiles (no children)
    // In a 3D Tiles implementation, this would parse tileset.json
    setIsLeaf(true);
}

END_DQ_RENDER_NAMESPACE
