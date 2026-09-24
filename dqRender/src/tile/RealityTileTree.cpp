// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RealityTileTree implementation
// Ported from: itwinjs-core core/frontend/src/tile/RealityTileTree.ts
#include "dqRender/tile/RealityTileTree.h"

#include "TilesetJson.h"

#include <cmath>
#include <cstring>
#include <sstream>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// TileDrawArgs::computeScreenSize — approximate screen-space size of a tile
// Ported from: itwinjs-core TileDrawArgs.computeScreenSize()
// ---------------------------------------------------------------------------
float TileDrawArgs::computeScreenSize(Tile const& tile) const
{
    // Simple distance-based approximation:
    // screenSize = boundingSphere.diameter * pixelSizeRatio / distance
    auto const& bs = tile.getBoundingSphere();
    float dx = bs.center[0] - eyePos[0];
    float dy = bs.center[1] - eyePos[1];
    float dz = bs.center[2] - eyePos[2];
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist < 1e-6f)
        return 1e6f;  // Very close — very large screen size.

    return (bs.radius * 2.0f * pixelSizeRatio) / dist;
}

RealityTileTree::RealityTileTree(std::unique_ptr<Tile> rootTile,
                                 std::string const& tilesetUrl)
    : TileTree(std::move(rootTile))
    , m_tilesetUrl(tilesetUrl)
{
}

// ---------------------------------------------------------------------------
// resolveContentUri — resolve relative URI against tileset base URL
// Ported from: itwinjs-core resolveContentUrl()
// ---------------------------------------------------------------------------
std::string RealityTileTree::resolveContentUri(std::string const& contentUri) const
{
    // If URI is already absolute (http/https or starts with /), return as-is.
    if (contentUri.empty())
        return contentUri;

    if (contentUri.substr(0, 7) == "http://" ||
        contentUri.substr(0, 8) == "https://") {
        return contentUri;
    }

    // Relative URI: resolve against tileset base directory.
    // Find the last '/' in the tileset URL to get the base directory.
    auto lastSlash = m_tilesetUrl.rfind('/');
    if (lastSlash == std::string::npos)
        return contentUri;  // No base directory, return as-is.

    return m_tilesetUrl.substr(0, lastSlash + 1) + contentUri;
}

TileVisibility RealityTileTree::computeVisibility(TileDrawArgs& args, Tile* tile)
{
    // Ported from: RealityTile.computeVisibilityFactor (RealityTile.ts
    // :516-545) composed with Tile.computeVisibility's return shape
    // (Tile.ts:429-453). Factor < 0 -> OutsideFrustum; structure nodes
    // (no content) -> TooCoarse (descend); SSE test decides the rest:
    // SSE = geometricError / pixelSize (world units per pixel,
    // TileDrawArgs.ts:138), refine when SSE exceeds the maximum
    // screen-space error (16 px, TileDrawArgs.ts:279).
    if (!tile)
        return TileVisibility::OutsideFrustum;

    auto* realityTile = static_cast<RealityTile*>(tile);

    // Content-less intermediate tile (a legal 3D Tiles LOD form): pure
    // structure -- TooCoarse so the traversal descends into children.
    if (!realityTile->hasContent())
        return TileVisibility::TooCoarse;

    // Frustum culling -- tiles entirely outside the view frustum are neither
    // requested nor displayed. Ported from: RealityTile.computeVisibilityFactor
    // (RealityTile.ts:516-527 -- isFrustumCulled via range corners +
    // FrustumPlanes.computeContainment -> -1). The bounding sphere provides
    // the cheap early-out (FrustumPlanes.ts:173-183).
    if (args.frustumPlanes.isValid()) {
        dqGeom::Point3d const sphereCenter(
            realityTile->getBoundingSphere().center[0],
            realityTile->getBoundingSphere().center[1],
            realityTile->getBoundingSphere().center[2]);
        auto const containment = args.frustumPlanes.computeContainment(
            tile->getRange(), &sphereCenter,
            static_cast<double>(realityTile->getBoundingSphere().radius));
        if (containment == dqCommon::FrustumPlanes::Containment::Outside)
            return TileVisibility::OutsideFrustum;
    }

    if (realityTile->isLeaf())
        return TileVisibility::Visible;

    // Pixel size: world-units-per-pixel at the closest point of the tile's
    // bounding sphere. Orthographic: uniform pixelSizeRatio. Perspective
    // (camera on): dist(closest sphere point to eye) * perspectiveScale —
    // Ported from: TileDrawArgs.computePixelSizeInMetersAtClosestPoint
    // (TileDrawArgs.ts:190-206; near-plane clamp keeps near tiles finite).
    float pixelSize = args.getPixelSizeRatio();
    if (args.cameraOn && args.perspectiveScale > 0.0f) {
        auto const& bs = realityTile->getBoundingSphere();
        float const dx = bs.center[0] - args.cameraEye[0];
        float const dy = bs.center[1] - args.cameraEye[1];
        float const dz = bs.center[2] - args.cameraEye[2];
        float const dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float const closest = std::max(dist - bs.radius, 0.01f);
        pixelSize = closest * args.perspectiveScale;
    }

    float const sse = pixelSize > 0.0f
        ? realityTile->getGeometricError() / pixelSize
        : 0.0f;
    return sse <= TileDrawArgs::kMaximumScreenSpaceError
        ? TileVisibility::Visible
        : TileVisibility::TooCoarse;
}

// ---------------------------------------------------------------------------
// Structured tileset.json parsing.
//
// The previous implementation scanned the raw text for the FIRST occurrence of
// each key inside a node's substring — a contentless root adopted its first
// child's "content", so it stopped descending and later siblings never loaded
// (2026-09-21, caught by TileTreeRender.LodTilesetRendersChildren).
//
// The reference parses tileset.json structurally (browser JSON.parse) and then
// walks the object tree (RealityModelTileTree.getChildrenProps /
// findTileInJson, RealityModelTileTree.ts:523-551). This is the DanQing
// equivalent: a small recursive-descent parser for the JSON grammar subset a
// tileset uses (RFC 8259 objects/arrays/strings/numbers/literals), then
// per-node field extraction from the parsed tree — fields can no longer leak
// across nesting levels.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// loadTileset — parse tileset.json and create a RealityTileTree
// Ported from: itwinjs-core RealityTileLoader.loadTileset()
// ---------------------------------------------------------------------------
std::unique_ptr<RealityTileTree> RealityTileTree::loadTileset(
    std::string const& tilesetUrl,
    uint8_t const* jsonData, size_t jsonSize)
{
    if (!jsonData || jsonSize == 0)
        return nullptr;

    std::string_view json(reinterpret_cast<char const*>(jsonData), jsonSize);

    // Parse the document structurally (reference: JSON.parse), then walk to
    // the root node — field extraction can no longer leak across levels.
    auto doc = tilejson::parseJsonDocument(json);
    if (!doc)
        return nullptr;

    tilejson::JsonValue const* root = doc->find("root");
    if (!root || root->type != tilejson::JsonValue::Type::Object)
        return nullptr;

    TilesetNode rootNode = tilejson::parseTilesetNode(*root);

    // Create root tile.
    auto tree = std::make_unique<RealityTileTree>(nullptr, tilesetUrl);
    auto rootTile = createTile(*tree, nullptr, rootNode);
    tree->setRootTile(std::move(rootTile));

    return tree;
}

// ---------------------------------------------------------------------------
// createTile — recursively create tiles from a tileset node
// ---------------------------------------------------------------------------
std::unique_ptr<RealityTile> RealityTileTree::createTile(
    RealityTileTree& tree, Tile* parent,
    TilesetNode const& node)
{
    auto range = tilejson::rangeFromBoundingVolume(node.boundingVolume);

    auto tile = std::make_unique<RealityTile>(
        tree, parent,
        node.boundingVolume,
        node.geometricError,
        node.contentUri,
        range);

    // Recursively create children.
    if (!node.children.empty()) {
        std::vector<std::unique_ptr<Tile>> children;
        for (auto const& childNode : node.children) {
            children.push_back(createTile(tree, tile.get(), childNode));
        }
        tile->setChildren(std::move(children));
    }

    return tile;
}

END_DQ_RENDER_NAMESPACE
