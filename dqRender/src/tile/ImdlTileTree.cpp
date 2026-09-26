// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — iModel tile tree implementation
// Ported from: itwinjs-core core/common/src/tile/TileMetadata.ts
//              (computeChildTileProps :777-853, ContentIdProvider V1
//              :665-674, bisectTileRange3d/2d :724-743)
//              core/frontend/src/internal/tile/IModelTile.ts (:153-169
//              _loadChildren; :90-131 request/read content)
#include "dqRender/tile/ImdlTileTree.h"

#include <dqCommon/PackedFeatureTable.h>

#include "dqRender/RenderGraphic.h"
#include "dqRender/RenderSystem.h"
#include "dqRender/tile/ImdlHeader.h"
#include "dqRender/tile/TileFormat.h"

#include "dqRender/tile/ImdlDocument.h"
#include "dqRender/tile/ITileFetcher.h"
#include "dqRender/tile/TileAdmin.h"

#include <cstdlib>
#include <memory>
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
        child.maximumSize = static_cast<double>(root.tileScreenSize);  // :795
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
                child.maximumSize = static_cast<double>(root.tileScreenSize);  // :847
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
                   double sizeMultiplier, double maximumSize)
    : Tile(tree, parent, range, parent ? parent->getDepth() + 1 : 0, maximumSize)
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
    // Ported from: IModelTile.readContent (IModelTile.ts:90-131) + ImdlReader
    // (.ts:104-123 — decode → convertFeatureTable → createBatch). DanQing
    // synchronous subset: no worker/async; rtcCenter not consumed this pass
    // (the offline fixtures carry none — JSON scene has no rtcCenter field;
    // TODO: rtcCenter branch, ImdlReader.ts:126-130).
    TileContent content;
    if (!data || dataSize < 4)
        return content;
    if (DetectTileFormat(data, dataSize) != TileFormat::IModel)
        return content;

    ImdlByteStream stream(data, dataSize);
    auto const header = ImdlHeader::readFrom(stream);
    if (!header.isValid())
        return content;

    // Feature table: read the 12-byte header + the packed words (3×u32/feature
    // + 2×u32/subcategory tail) instead of skipping them — the stream ends up
    // at the same position as the reference's skip-then-seek-back.
    // Ported from: ParseImdlDocument.ts:1278-1284 (ftStartPos +
    // FeatureTableHeader.readFrom + seek to ftStartPos + length) +
    // FeatureTableHeader.ts layout (length includes the 12-byte header).
    ImdlFeatureTableHeader ftHeader;
    if (!ImdlFeatureTableHeader::readFrom(stream, ftHeader))
        return content;  // InvalidFeatureTable (ParseImdlDocument.ts:1281-1282)
    std::vector<uint32_t> featureWords;
    size_t const ftBodyBytes = ftHeader.length >= ImdlFeatureTableHeader::sizeInBytes
        ? ftHeader.length - ImdlFeatureTableHeader::sizeInBytes : 0;
    if (ftBodyBytes > 0) {
        featureWords.resize(ftBodyBytes / sizeof(uint32_t));
        stream.readBytes(featureWords.data(), featureWords.size() * sizeof(uint32_t));
        if (stream.isPastTheEnd())
            return content;
    }

    // Header-only description (the caller consumed the feature table — the
    // leaf heuristic derives purely from the imdl header).
    auto const desc = decodeImdlContentDescriptionHeaderOnly(header);
    if (!desc.has_value())
        return content;
    content.contentRange = desc->contentRange;
    content.isLeaf = desc->isLeaf;

    auto doc = parseImdlDocument(stream, &ftHeader, &featureWords);
    if (!doc.has_value())
        return content;

    RenderSystem* system = getTree().getRenderSystem();
    if (!system)
        return content;

    // imdl 顶点消费主路径：LUT 直传（U7 归位，createImdlLutGraphics）。
    // EQUIVALENCE: 参考源=itwinjs-core VertexLUT.ts:93-99（线上顶点表直传纹理）+
    //   Vertex.ts computeVertexPosition（shader 侧 f32 解量化）。发散=旧路径 CPU
    //   f64 解量化后截 f32，新路径 shader f32 解量化，量化域内差异 ≤1ulp、
    //   屏幕像素不可辨；验证法=TileTreeRender 8 项像素锁全绿 +
    //   LutPathUploadsVertexTableVerbatim 的字节级直传断言。
    std::vector<RenderGraphic*> graphics;
    if (system->driver()) {
        graphics = createImdlLutGraphics(*doc, *system);
    } else {
        // 无 GL 桩系统（dqRenderTest 的 ReadContentStubSystem）回退：polyface
        // 形态保留为对照/调试通道（旧 decodeImdlGraphics——ImdlGraphics 既有
        // 测试亦直接测它）。
        auto meshes = decodeImdlGraphics(*doc);
        for (auto& polyface : meshes) {
            if (auto* graphic = system->createGraphicFromPolyface(polyface.Get(), 0xFFFFFFFFu, 0))
                graphics.push_back(graphic);
        }
    }
    if (graphics.empty())
        return content;
    RenderGraphic* list = system->createGraphicList(std::move(graphics));

    // convertFeatureTable (ParseImdlDocument.ts:1259-1266): on-the-wire words
    // → PackedFeatureTable (DanQing single-model subset: BatchType::Primary,
    // modelId 0 — offline tilesets carry no model semantics).
    // TODO(deferred): MultiModelPackedFeatureTable — when
    // ImdlFlags::MultiModelFeatureTable is set the reference builds a
    // multi-model table; until then such tiles fall through to the unbatched
    // path below (registered, mirrors the reference's branch at
    // ParseImdlDocument.ts:1260-1262).
    bool const multiModel =
        0 != (static_cast<uint32_t>(header.flags)
              & static_cast<uint32_t>(ImdlFlags::MultiModelFeatureTable));
    if (!multiModel && doc->featureCount > 0 && !doc->featureData.empty()) {
        dqCommon::PackedFeatureTable packed(doc->featureData, /*modelId*/ 0,
                                            doc->featureCount,
                                            dqCommon::BatchType::Primary);
        content.featureTable =
            std::make_unique<dqCommon::FeatureTable>(packed.unpack());
        // ImdlReader.ts:122-123: graphic = system.createBatch(graphic,
        // featureTable, content.contentRange).
        content.graphic.reset(system->createBatch(list, content.featureTable.get(),
                                                  content.contentRange));
    } else {
        content.graphic.reset(list);
    }

    // maximumSize 回填：内容到达且从未获得 maximumSize（0 = undisplayable）
    // 时取树的 tileScreenSize。
    // Ported from: IModelTile.setContent (IModelTile.ts:140-142):
    //   if (undefined !== content.graphic && 0 === this.maximumSize)
    //     this._maximumSize = this.iModelTree.tileScreenSize;
    // DanQing adaptation: the backfill lives here (readContent) — TileAdmin::
    // deliverTileContent runs readContent immediately before setContent and
    // setContent is non-virtual, so the reference's IModelTile.setContent
    // override has no C++ dispatch path; the observable state at the point of
    // consumption is the same.
    if (content.graphic && 0.0 == getMaximumSize()) {
        auto& imdlTree = static_cast<ImdlTileTree&>(getTree());
        m_maximumSize = static_cast<double>(imdlTree.metadata().tileScreenSize);
    }
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
            tree, this, child.contentId, child.range, child.sizeMultiplier,
            child.maximumSize));
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
    // rootTile; IModelTileTree constructor :396-410). maximumSize =
    // tileScreenSize: the children fill the same value (TileMetadata.ts:795/
    // :847 maximumSize: root.tileScreenSize) and the reference's root props
    // carry it via requestTileTreeProps — DanQing's id-encoded props have no
    // separate carrier for a root maximumSize, so tileScreenSize stands in
    // (the same value IModelTile.setContent backfills, IModelTile.ts:140-142).
    auto root = std::make_unique<ImdlTile>(*this, nullptr,
                                           std::move(rootContentId),
                                           rootRange, 0.0,
                                           static_cast<double>(treeMetadata.tileScreenSize));
    setRootTile(std::move(root));
}

TileVisibility ImdlTileTree::computeVisibility(TileDrawArgs& args, Tile* tile)
{
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

    // Leaf branch retained per reference computeVisibility (Tile.ts:445-449):
    // a leaf passes culling → Visible (after content culling, which DanQing's
    // subset does not port yet) — it never takes the SSE branch.
    if (tile->isLeaf())
        return TileVisibility::Visible;

    // Ported from: Tile.ts meetsScreenSpaceError (:459-463):
    //   pixelSize = args.getPixelSize(this) * args.pixelSizeScaleFactor;
    //   maxSize = this.maximumSize * args.tileSizeModifier;
    //   return pixelSize <= maxSize;
    double const pixelSize = args.getPixelSize(*tile) * args.pixelSizeScaleFactor;
    double const maxSize = tile->getMaximumSize() * args.tileSizeModifier;
    return pixelSize <= maxSize
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
