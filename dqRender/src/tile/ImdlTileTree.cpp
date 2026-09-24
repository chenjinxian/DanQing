// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — iModel tile tree implementation
// Ported from: itwinjs-core core/common/src/tile/TileMetadata.ts
//              (computeChildTileProps :777-853, ContentIdProvider V1
//              :665-674, bisectTileRange3d/2d :724-743)
//              core/frontend/src/internal/tile/IModelTile.ts (:153-169
//              _loadChildren; :90-131 request/read content)
#include "dqRender/tile/ImdlTileTree.h"

#include "dqRender/RenderGraphic.h"
#include "dqRender/RenderSystem.h"
#include "dqRender/tile/ImdlHeader.h"
#include "dqRender/tile/TileFormat.h"

#include "dqRender/tile/ImdlDocument.h"
#include "dqRender/tile/ITileFetcher.h"
#include "dqRender/tile/TileAdmin.h"

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Bisect a range along one axis. Ported from: bisectTileRange3d/2d
// (TileMetadata.ts:724-743 — the axis' low/high midpoint split, keep half).
void bisectRange(dqGeom::Range3d& range, int axis, bool keepLow)
{
    double* lo = &range.low.x;
    double* hi = &range.high.x;
    double const mid = 0.5 * (lo[axis] + hi[axis]);
    if (keepLow)
        hi[axis] = mid;
    else
        lo[axis] = mid;
}

}  // namespace

ImdlContentIdSpec parseImdlContentId(std::string const& id)
{
    // V1: "<depth>/<i>/<j>/<k>" (+"[/<mult>]" for magnification,
    // TileMetadata.ts:665-674).
    ImdlContentIdSpec spec;
    std::istringstream ss(id);
    std::string token;
    uint32_t values[5] = {0, 0, 0, 0, 0};
    int n = 0;
    while (n < 5 && std::getline(ss, token, '/')) {
        values[n++] = static_cast<uint32_t>(std::strtoul(token.c_str(), nullptr, 10));
    }
    if (n >= 4) {
        spec.depth = values[0];
        spec.i = values[1];
        spec.j = values[2];
        spec.k = values[3];
        if (n >= 5)
            spec.mult = values[4];
    }
    return spec;
}

std::string formatImdlContentId(ImdlContentIdSpec const& spec)
{
    std::ostringstream ss;
    ss << spec.depth << '/' << spec.i << '/' << spec.j << '/' << spec.k;
    if (spec.mult != 0)
        ss << '/' << spec.mult;
    return ss.str();
}

std::vector<ImdlChildTileProps> computeImdlChildTileProps(
    ImdlTileMetadata const& parent, ImdlTreeMetadata const& root)
{
    // Ported from: computeChildTileProps (TileMetadata.ts:777-853).
    std::vector<ImdlChildTileProps> children;
    if (parent.isLeaf)
        return children;

    // Magnification: one child, same volume, doubled multiplier (:785-799).
    if (parent.sizeMultiplier > 0.0) {
        double const multiplier = parent.sizeMultiplier * 2.0;
        ImdlContentIdSpec spec = parseImdlContentId(parent.contentId);
        spec.mult = static_cast<uint32_t>(multiplier);
        ImdlChildTileProps child;
        child.contentId = formatImdlContentId(spec);
        child.range = parent.range;
        child.sizeMultiplier = multiplier;
        child.isLeaf = false;
        children.push_back(std::move(child));
        return children;
    }

    // Sub-divide into 4 (2d) or 8 (3d) children (:801-848).
    ImdlContentIdSpec parentSpec = parseImdlContentId(parent.contentId);
    uint32_t const emptyMask = parent.emptySubRangeMask;

    // Model-range rejection (:816-820): only test children when the parent
    // is not wholly inside the model range.
    bool testContentRange = !root.contentRange.isNull()
                            && !root.contentRange.ContainsRange(parent.range);

    for (uint32_t i = 0; i < 2; ++i) {
        for (uint32_t j = 0; j < 2; ++j) {
            for (uint32_t k = 0; k < (root.is2d ? 1u : 2u); ++k) {
                uint32_t const emptyBit = 1u << (i + j * 2 + k * 4);
                if (0 != (emptyMask & emptyBit))
                    continue;  // known-empty sub-volume (:826-830)

                dqGeom::Range3d range = parent.range;
                bisectRange(range, 0, 0 == i);
                bisectRange(range, 1, 0 == j);
                if (!root.is2d)
                    bisectRange(range, 2, 0 == k);

                if (testContentRange && !range.IntersectsRange(root.contentRange))
                    continue;  // outside model range (:841-845)

                ImdlContentIdSpec childSpec = parentSpec;
                childSpec.depth = parentSpec.depth + 1;
                childSpec.i = parentSpec.i * 2 + i;
                childSpec.j = parentSpec.j * 2 + j;
                childSpec.k = parentSpec.k * 2 + k;

                ImdlChildTileProps child;
                child.contentId = formatImdlContentId(childSpec);
                child.range = range;
                children.push_back(std::move(child));
            }
        }
    }

    return children;
}

// ---------------------------------------------------------------------------
// ImdlTile
// ---------------------------------------------------------------------------

ImdlTile::ImdlTile(ImdlTileTree& tree, Tile* parent,
                   std::string contentId, dqGeom::Range3d const& range,
                   double sizeMultiplier)
    : Tile(tree, parent, range, parent ? parent->getDepth() + 1 : 0)
    , m_contentId(std::move(contentId))
    , m_sizeMultiplier(sizeMultiplier)
{
    if (m_contentId.empty())
        setIsLeaf(true);
}

bool ImdlTile::requestContent()
{
    // Content acquisition: the reference routes through TileAdmin.
    // generateTileContent → RPC/cache (TileAdmin.ts:684-702); DanQing composes
    // a treeId/contentId URL for the injected fetcher (the production seam).
    if (m_contentId.empty())
        return false;

    auto& tree = static_cast<ImdlTileTree&>(getTree());
    std::string const url = tree.contentUrl(m_contentId);
    auto& fetcher = TileAdmin::instance().getFetcher();
    fetcher.fetch(
        url, *this,
        [](Tile& tile, std::vector<uint8_t> const& data) {
            TileAdmin::instance().deliverTileContent(tile, data);
        },
        [](Tile& tile, std::string const& error) {
            TileAdmin::instance().reportTileFetchError(tile, error);
        });
    return true;
}

TileContent ImdlTile::readContent(uint8_t const* data, size_t dataSize)
{
    // Ported from: IModelTile.readContent (IModelTile.ts:94-131) — magic
    // check then the imdl decode chain (header → description → document →
    // graphics; metadata verified by the ImdlDocument/ImdlGraphics suites).
    TileContent content;
    if (!data || dataSize < 4)
        return content;
    if (DetectTileFormat(data, dataSize) != TileFormat::IModel)
        return content;

    ImdlByteStream stream(data, dataSize);
    auto const header = ImdlHeader::readFrom(stream);
    if (!header.isValid())
        return content;

    auto const desc = decodeImdlContentDescription(header, stream);
    if (!desc.has_value())
        return content;
    content.contentRange = desc->contentRange;
    content.isLeaf = desc->isLeaf;

    auto doc = parseImdlDocument(stream);
    if (!doc.has_value())
        return content;

    auto meshes = decodeImdlGraphics(*doc);
    if (meshes.empty())
        return content;

    RenderSystem* system = getTree().getRenderSystem();
    if (!system)
        return content;

    std::vector<RenderGraphic*> graphics;
    for (auto& polyface : meshes) {
        if (auto* graphic = system->createGraphicFromPolyface(polyface.Get(), 0xFFFFFFFFu, 0))
            graphics.push_back(graphic);
    }
    if (!graphics.empty())
        content.graphic.reset(system->createGraphicList(std::move(graphics)));
    return content;
}

void ImdlTile::loadChildren()
{
    // Ported from: IModelTile._loadChildren (IModelTile.ts:153-169) — pure
    // frontend computation, no request.
    auto& tree = static_cast<ImdlTileTree&>(getTree());
    ImdlTileMetadata parent;
    parent.contentId = m_contentId;
    parent.range = getRange();
    parent.contentRange = getRange();
    parent.isLeaf = isLeaf();
    parent.sizeMultiplier = m_sizeMultiplier;

    auto children = tree.childPropsFor(parent);
    if (children.empty()) {
        setIsLeaf(true);
        return;
    }

    std::vector<std::unique_ptr<Tile>> childTiles;
    for (auto const& child : children) {
        childTiles.push_back(std::make_unique<ImdlTile>(
            tree, this, child.contentId, child.range, child.sizeMultiplier));
    }
    setChildren(std::move(childTiles));
}

// ---------------------------------------------------------------------------
// ImdlTileTree
// ---------------------------------------------------------------------------

ImdlTileTree::ImdlTileTree(std::string treeId, std::string rootContentId,
                           dqGeom::Range3d const& rootRange,
                           ImdlTreeMetadata treeMetadata)
    : TileTree(nullptr)
    , m_treeId(std::move(treeId))
    , m_metadata(treeMetadata)
{
    // Root tile from the tree props (the reference's requestTileTreeProps
    // rootTile; IModelTileTree constructor :396-410).
    auto root = std::make_unique<ImdlTile>(*this, nullptr,
                                           std::move(rootContentId),
                                           rootRange, 0.0);
    setRootTile(std::move(root));
}

TileVisibility ImdlTileTree::computeVisibility(TileDrawArgs& args, Tile* tile)
{
    // Same SSE metric as the reality tree (RealityTile.ts:535-542) — the
    // iModel tree's substitute metric (maximumSize/pixelSize, Tile.ts
    // meetsScreenSpaceError :459-463) maps to the same SSE form via
    // tileScreenSize; DanQing uses the geometric-error form directly with the
    // root content range diagonal-derived error (registered simplification
    // until tree props carry real geometric errors).
    if (!tile)
        return TileVisibility::OutsideFrustum;
    if (!tile->hasContent())
        return TileVisibility::TooCoarse;

    if (args.frustumPlanes.isValid()) {
        auto const& bs = tile->getBoundingSphere();
        dqGeom::Point3d const center(bs.center[0], bs.center[1], bs.center[2]);
        if (args.frustumPlanes.computeContainment(
                tile->getRange(), &center, static_cast<double>(bs.radius))
            == dqCommon::FrustumPlanes::Containment::Outside)
            return TileVisibility::OutsideFrustum;
    }

    if (tile->isLeaf())
        return TileVisibility::Visible;

    // Depth-based error: each level halves the world error; compare against
    // the SSE threshold via pixelSize (approximation of the reference's
    // maximumSize test — registered; see header note).
    float pixelSize = args.getPixelSizeRatio();
    if (args.cameraOn && args.perspectiveScale > 0.0f) {
        auto const& bs = tile->getBoundingSphere();
        float const dx = bs.center[0] - args.cameraEye[0];
        float const dy = bs.center[1] - args.cameraEye[1];
        float const dz = bs.center[2] - args.cameraEye[2];
        float const dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        pixelSize = std::max(dist - bs.radius, 0.01f) * args.perspectiveScale;
    }
    double const depth = tile->getDepth();
    double const geometricError = m_metadata.tileScreenSize / (1u << std::min<uint32_t>(static_cast<uint32_t>(depth), 20u));
    double const sse = pixelSize > 0.0f ? geometricError / pixelSize : 0.0;
    return sse <= TileDrawArgs::kMaximumScreenSpaceError
        ? TileVisibility::Visible
        : TileVisibility::TooCoarse;
}

std::string ImdlTileTree::contentUrl(std::string const& contentId) const
{
    // Composition point for the fetch layer (FileTileFetcher resolves local
    // paths; the production RPC/cache routing is the registered TODO).
    return m_treeId + "/" + contentId;
}

END_DQ_RENDER_NAMESPACE
