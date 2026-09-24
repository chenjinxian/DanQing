// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RealityTile implementation
// Ported from: itwinjs-core core/frontend/src/tile/RealityTile.ts
#include "dqRender/tile/RealityTile.h"
#include "dqRender/tile/RealityTileTree.h"

#include "BatchTableHierarchy.h"
#include "dqRender/CreateTextureArgs.h"
#include "dqRender/GltfReader.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/RenderSystem.h"
#include "dqRender/tile/ITileFetcher.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/tile/ImdlDocument.h"
#include "dqRender/tile/TileFormat.h"

#include <dqCommon/FeatureTable.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
bool tileTraceEnabled()
{
    static bool const enabled = std::getenv("DANQING_TILE_TRACE") != nullptr;
    return enabled;
}
}  // namespace

BEGIN_DQ_RENDER_NAMESPACE

RealityTile::RealityTile(RealityTileTree& tree, Tile* parent,
                         BoundingVolume const& boundingVolume,
                         float geometricError,
                         std::string const& contentUri,
                         dqGeom::Range3d const& range)
    : Tile(tree, parent, range)
    , m_boundingVolume(boundingVolume)
    , m_geometricError(geometricError)
    , m_contentUri(contentUri)
{
    if (m_contentUri.empty())
        setIsLeaf(true);
}

bool RealityTile::requestContent()
{
    // Initiate async HTTP fetch for remote tile content.
    // Ported from: itwinjs-core RealityTileLoader.requestContent()
    if (m_contentUri.empty())
        return false;

    // Resolve content URI relative to tileset base URL.
    auto& tree = static_cast<RealityTileTree&>(getTree());
    std::string resolvedUrl = tree.resolveContentUri(m_contentUri);

    if (tileTraceEnabled())
        std::fprintf(stderr, "[TILE] requestContent uri=%s -> %s system=%p\n",
                     m_contentUri.c_str(), resolvedUrl.c_str(),
                     static_cast<void*>(getTree().getRenderSystem()));

    // Use the TileAdmin's fetcher for async HTTP GET. Completion goes through
    // TileAdmin's sink (deliverTileContent) which settles the channel request
    // then runs readContent→setContent — the reference does the same inside
    // TileRequest.handleResponse (TileRequest.ts:156-195).
    auto& fetcher = TileAdmin::instance().getFetcher();
    fetcher.fetch(
        resolvedUrl,
        *this,
        // On complete: deliver bytes to the completion sink
        [](Tile& tile, std::vector<uint8_t> const& data) {
            TileAdmin::instance().deliverTileContent(tile, data);
        },
        // On error: settle + mark tile as not found
        [](Tile& tile, std::string const& error) {
            TileAdmin::instance().reportTileFetchError(tile, error);
        });

    return true;
}

// ---------------------------------------------------------------------------
// readContent — parse tile content (b3dm/i3dm/gltf)
// Ported from: itwinjs-core RealityTileLoader.loadGraphicsFromStream()
// ---------------------------------------------------------------------------
TileContent RealityTile::readContent(uint8_t const* data, size_t dataSize)
{
    TileContent content;
    content.isLeaf = false;

    if (!data || dataSize < 4)
        return content;

    // Detect format by magic number.
    TileFormat format = DetectTileFormat(data, dataSize);

    if (tileTraceEnabled())
        std::fprintf(stderr, "[TILE] readContent size=%zu format=%d system=%p\n",
                     dataSize, static_cast<int>(format),
                     static_cast<void*>(getTree().getRenderSystem()));

    uint8_t const* gltfData = nullptr;
    size_t gltfSize = 0;

    switch (format) {
        case TileFormat::B3dm: {
            if (dataSize < sizeof(B3dmHeader))
                return content;

            B3dmHeader header;
            std::memcpy(&header, data, sizeof(header));

            if (!ValidateB3dmHeader(header))
                return content;

            size_t gltfOffset = sizeof(B3dmHeader)
                + header.featureTableJsonLength
                + header.featureTableBinaryLength
                + header.batchTableJsonLength
                + header.batchTableBinaryLength;

            if (gltfOffset >= dataSize)
                return content;

            // Parse batch table for 3DTILES_batch_table_hierarchy extension.
            if (header.batchTableJsonLength > 0) {
                char const* batchTableJson = reinterpret_cast<char const*>(
                    data + sizeof(B3dmHeader)
                    + header.featureTableJsonLength
                    + header.featureTableBinaryLength);
                auto hierarchy = parseBatchTableHierarchy(
                    batchTableJson, header.batchTableJsonLength);
                if (hierarchy.isValid()) {
                    // Create FeatureTable from hierarchy element IDs.
                    // Ported from: itwinjs-core B3dmReader.read() feature table creation
                    auto featureTable = std::make_unique<dqCommon::FeatureTable>(
                        static_cast<int>(hierarchy.instancesLength));
                    for (uint32_t i = 0; i < hierarchy.instancesLength; ++i) {
                        uint64_t elemId = hierarchy.getElementId(i);
                        uint64_t subCatId = hierarchy.getSubCategoryId(i);
                        dqBase::DqId eid(elemId);
                        dqBase::DqId sid(subCatId);
                        dqCommon::Feature feature(eid, sid);
                        featureTable->insert(feature);
                    }
                    // Store the feature table for later use by the render system.
                    // The feature table will be packed into a GPU texture for
                    // per-feature color/symbology overrides.
                    content.featureTable = std::move(featureTable);
                }
            }

            gltfData = data + gltfOffset;
            gltfSize = dataSize - gltfOffset;
            break;
        }

        case TileFormat::I3dm: {
            if (dataSize < sizeof(I3dmHeader))
                return content;

            I3dmHeader header;
            std::memcpy(&header, data, sizeof(header));

            if (!ValidateI3dmHeader(header))
                return content;

            size_t gltfOffset = sizeof(I3dmHeader)
                + header.featureTableJsonLength
                + header.featureTableBinaryLength
                + header.batchTableJsonLength
                + header.batchTableBinaryLength;

            if (gltfOffset >= dataSize)
                return content;

            gltfData = data + gltfOffset;
            gltfSize = dataSize - gltfOffset;
            break;
        }

        case TileFormat::Gltf:
            gltfData = data;
            gltfSize = dataSize;
            break;

        case TileFormat::IModel: {
            // iMdl content: header → content description → glTF-section
            // document. The metadata closes the loop into TileContent; the
            // graphics pass (quantized VertexTable + meshopt decoding,
            // ParseImdlDocument.ts's Parser + ImdlGraphicsCreator.ts ≈ 2000+
            // lines) is a separately scoped follow-up — TODO(imdl-graphics).
            // Ported from: ImdlReader.readImdlContent (ImdlReader.ts:74-140 —
            // the decodeTileContentDescription + parseImdlDocument stages).
            dqRender::ImdlByteStream imdlStream(data, dataSize);
            auto const imdlHeader = ImdlHeader::readFrom(imdlStream);
            if (imdlHeader.isValid()) {
                // Feature table words between the 12-byte header and the glTF
                // section (the reference's convertFeatureTable input,
                // ParseImdlDocument.ts:1259-1267; PackedFeatureTable layout
                // 3×u32/feature — PackedFeatureTable.ts:139-141).
                ImdlFeatureTableHeader ftHeader;
                bool const hasFt = ImdlFeatureTableHeader::readFrom(imdlStream, ftHeader);
                std::vector<uint32_t> featureWords;
                if (hasFt && ftHeader.length > sizeof(ImdlFeatureTableHeader)) {
                    size_t const words = (ftHeader.length - sizeof(ImdlFeatureTableHeader)) / 4;
                    featureWords.resize(words);
                    imdlStream.readBytes(featureWords.data(), words * 4);
                }
                if (auto desc = decodeImdlContentDescriptionHeaderOnly(imdlHeader)) {
                    content.contentRange = desc->contentRange;
                    content.isLeaf = desc->isLeaf;
                    // Graphics pass (ImdlReader.ts:104-110 decodeImdlGraphics
                    // → system.createBatch): decode meshes → polyface
                    // graphics via the tree's injected render system.
                    auto doc = parseImdlDocument(imdlStream, hasFt ? &ftHeader : nullptr, &featureWords);
                    if (doc.has_value()) {
                        auto meshes = decodeImdlGraphics(*doc);
                        if (tileTraceEnabled()) {
                            for (auto const& pf : meshes) {
                                std::fprintf(stderr, "[TILE] imdl mesh pts=%zu facets=%zu:",
                                             pf->Data().PointCount(), pf->FacetCount());
                                for (size_t i = 0; i < pf->Data().PointCount() && i < 6; ++i)
                                    std::fprintf(stderr, " (%.2f,%.2f)",
                                                 pf->Data().GetPoint(static_cast<int32_t>(i + 1)).x,
                                                 pf->Data().GetPoint(static_cast<int32_t>(i + 1)).y);
                                std::fprintf(stderr, "\\n");
                            }
                        }
                        if (!meshes.empty()) {
                            RenderSystem* system = getTree().getRenderSystem();
                            if (system) {
                                std::vector<RenderGraphic*> graphics;
                                for (auto& polyface : meshes) {
                                    if (auto* graphic = system->createGraphicFromPolyface(
                                            polyface.Get(), 0xFF00FF00u /*green uniform (fixture)*/, 0)) {
                                        graphics.push_back(graphic);
                                    }
                                }
                                if (!graphics.empty())
                                    content.graphic.reset(system->createGraphicList(std::move(graphics)));
                            }
                        }
                    }
                }
            }
            return content;
        }

        default:
            return content;
    }

    // Parse glTF content using GltfReader.
    if (!gltfData || gltfSize == 0)
        return content;

    auto scene = GltfReader::LoadFromMemory(gltfData, gltfSize);
    if (!scene || scene->meshes.empty())
        return content;

    if (tileTraceEnabled())
        std::fprintf(stderr, "[TILE] readContent gltf meshes=%zu bounds=(%.1f..%.1f)\n",
                     scene->meshes.size(),
                     scene->bounds.low.x, scene->bounds.high.x);

    // Create RenderGraphics from the scene.
    // Ported from: itwinjs-core GltfReader.read() graphic creation. The
    // system comes from the tree (reference passes `system` into readContent
    // explicitly — Tile.ts:429; DanQing's per-viewport system is injected into
    // the tree by the owning Viewport, see TileTree::setRenderSystem).
    RenderSystem* renderSystem = getTree().getRenderSystem();
    if (!renderSystem)
        return content;
    std::vector<RenderGraphic*> graphics;

    for (auto const& mesh : scene->meshes) {
        if (!mesh.polyface)
            continue;

        // Compute packed RGBA from baseColorFactor.
        uint32_t r = static_cast<uint32_t>(mesh.baseColorFactor[0] * 255.0f);
        uint32_t g = static_cast<uint32_t>(mesh.baseColorFactor[1] * 255.0f);
        uint32_t b = static_cast<uint32_t>(mesh.baseColorFactor[2] * 255.0f);
        uint32_t a = static_cast<uint32_t>(mesh.baseColorFactor[3] * 255.0f);
        uint32_t color = (a << 24) | (b << 16) | (g << 8) | r;

        // BaseColor texture upload — reference mechanism: the glTF readers
        // resolve named textures at graphic-creation time (GltfReader.ts
        // :2487-2565 resolveImage/resolveTexture → RenderSystem.createTexture),
        // which for b3dm/glb tiles happens inside readContent's decode chain
        // (RealityTileLoader.loadGraphicsFromStream → GltfReader.read). Same
        // end state as the app-side GltfDecoration texture path.
        dqRender::rhi::TextureHandle texture{};
        if (mesh.baseColorTexture.has_value() && mesh.baseColorTexture->width > 0) {
            dqRender::CreateTextureArgs texArgs{};
            texArgs.imageBuffer = *mesh.baseColorTexture;
            texture = renderSystem->createTexture(texArgs);
        }

        auto* graphic = renderSystem->createGraphicFromPolyface(
            mesh.polyface.Get(), color, 0, texture);
        if (graphic)
            graphics.push_back(graphic);
    }

    if (!graphics.empty())
        content.graphic.reset(renderSystem->createGraphicList(std::move(graphics)));

    content.contentRange = scene->bounds;
    content.isLeaf = false;  // 3D Tiles tiles may have children.

    return content;
}

void RealityTile::loadChildren()
{
    // Children are created during tileset.json parsing.
}

END_DQ_RENDER_NAMESPACE
