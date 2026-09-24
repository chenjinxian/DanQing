// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl document parsing implementation
// Ported from: itwinjs-core core/common/src/tile/TileMetadata.ts
//              (decodeTileContentDescription :880-940)
//              core/common/src/tile/GltfTileIO.ts (GltfHeader)
//              core/frontend/src/common/imdl/ParseImdlDocument.ts
//              (:1280-1327 — the glTF section extraction subset)
#include "dqRender/tile/ImdlDocument.h"

#include <cstring>

#include "TilesetJson.h"

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Ported from: GltfVersions (GltfTileIO.ts:15-20).
enum class GltfVersions : uint32_t {
    Version1 = 1,
    Version2 = 2,
    Gltf1SceneFormat = 0,
};

// Ported from: GltfV2ChunkTypes (GltfTileIO.ts:23-26).
enum class GltfV2ChunkTypes : uint32_t {
    JSON = 0x4E4F534a,    // 'JSON'
    Binary = 0x004E4942,  // 'BIN\0'
};

// Ported from: GltfHeader (GltfTileIO.ts — the embedded glTF section header;
// 20-byte Bentley variant, NOT a standard GLB header: TileHeader base
// format+version, gltfLength, sceneStrLength, chunk-type word, then the JSON
// scene; v2 additionally carries a binary chunk header after the scene).
struct GltfHeader {
    uint32_t format = 0;
    uint32_t version = 0;
    uint32_t gltfLength = 0;
    size_t scenePosition = 0;
    uint32_t sceneStrLength = 0;
    size_t binaryPosition = 0;
    bool isValid = false;

    explicit GltfHeader(ImdlByteStream& stream)
    {
        format = stream.readUint32();
        version = stream.readUint32();
        gltfLength = stream.readUint32();
        sceneStrLength = stream.readUint32();
        uint32_t const value5 = stream.readUint32();

        // Early publishers put version 2 with a v1 scene format word — treat
        // those as v1 (GltfTileIO.ts:31-33).
        if (version == static_cast<uint32_t>(GltfVersions::Version2)
            && value5 == static_cast<uint32_t>(GltfVersions::Gltf1SceneFormat))
            version = static_cast<uint32_t>(GltfVersions::Version1);

        if (version == static_cast<uint32_t>(GltfVersions::Version1)) {
            // v1: value5 must be the scene-format sentinel.
            if (value5 != static_cast<uint32_t>(GltfVersions::Gltf1SceneFormat))
                return;
            scenePosition = stream.curPos();
            binaryPosition = scenePosition + sceneStrLength;
            isValid = true;
        } else if (version == static_cast<uint32_t>(GltfVersions::Version2)) {
            // v2: value5 is the JSON chunk type; after the scene, a binary
            // chunk header (length + type) precedes the binary section.
            scenePosition = stream.curPos();
            stream.advance(sceneStrLength);
            uint32_t const binaryLength = stream.readUint32();
            uint32_t const binaryChunkType = stream.readUint32();
            if (value5 != static_cast<uint32_t>(GltfV2ChunkTypes::JSON)
                || binaryChunkType != static_cast<uint32_t>(GltfV2ChunkTypes::Binary)
                || 0 == binaryLength)
                return;
            binaryPosition = stream.curPos();
            isValid = true;
        }
    }
};

double constexpr kMaxLeafTolerance = 1.0;   // TileMetadata.ts:910
uint32_t constexpr kMinElementsPerTile = 100;  // TileMetadata.ts:919

}  // namespace

std::optional<ImdlContentDescription> decodeImdlContentDescriptionHeaderOnly(
    ImdlHeader const& header)
{
    // The leaf/subdivision heuristic of decodeTileContentDescription
    // (TileMetadata.ts:907-934) — pure header function.
    if (!header.isValid() || !header.isReadableVersion())
        return std::nullopt;

    bool isLeaf = false;
    double sizeMultiplier = 1.0;

    bool const completeTile =
        0 == (static_cast<uint32_t>(header.flags) & static_cast<uint32_t>(ImdlFlags::Incomplete));
    bool const emptyTile = completeTile && 0 == header.numElementsIncluded
                                              && 0 == header.numElementsExcluded;
    isLeaf = emptyTile;

    if (!isLeaf) {
        bool canSkipSubdivision =
            0 == (static_cast<uint32_t>(header.flags) & static_cast<uint32_t>(ImdlFlags::DisallowMagnification));
        canSkipSubdivision = canSkipSubdivision && header.tolerance <= kMaxLeafTolerance;
        canSkipSubdivision = canSkipSubdivision && completeTile;

        if (canSkipSubdivision) {
            if (completeTile && 0 == header.numElementsExcluded
                && header.numElementsIncluded <= kMinElementsPerTile) {
                bool const containsCurves =
                    0 != (static_cast<uint32_t>(header.flags) & static_cast<uint32_t>(ImdlFlags::ContainsCurves));
                if (!containsCurves)
                    isLeaf = true;
                else
                    sizeMultiplier = 1.0;
            } else if (header.numElementsIncluded + header.numElementsExcluded <= kMinElementsPerTile) {
                sizeMultiplier = 1.0;
            }
        }
    }

    ImdlContentDescription desc;
    desc.contentRange = header.contentRange;
    desc.isLeaf = isLeaf;
    desc.sizeMultiplier = sizeMultiplier;
    desc.emptySubRangeMask = header.emptySubRanges;
    return desc;
}

std::optional<ImdlContentDescription> decodeImdlContentDescription(
    ImdlHeader const& header, ImdlByteStream& stream)
{
    // Ported from: decodeTileContentDescription (TileMetadata.ts:880-940).
    // The caller has already consumed the header; skip the feature table
    // (:897-902 — read 12-byte header, jump startPos + length).
    if (!header.isValid() || !header.isReadableVersion())
        return std::nullopt;

    size_t const featureTableStartPos = stream.curPos();
    ImdlFeatureTableHeader ftHeader;
    if (!ImdlFeatureTableHeader::readFrom(stream, ftHeader))
        return std::nullopt;

    stream.advance(featureTableStartPos + ftHeader.length - stream.curPos());

    bool isLeaf = false;
    double sizeMultiplier = 1.0;

    // Determine subdivision from header data (:907-934).
    bool const completeTile =
        0 == (static_cast<uint32_t>(header.flags) & static_cast<uint32_t>(ImdlFlags::Incomplete));
    bool const emptyTile = completeTile && 0 == header.numElementsIncluded
                                              && 0 == header.numElementsExcluded;
    isLeaf = emptyTile;

    if (!isLeaf) {
        bool canSkipSubdivision =
            0 == (static_cast<uint32_t>(header.flags) & static_cast<uint32_t>(ImdlFlags::DisallowMagnification));
        // DanQing subset: is3d callers only (reality/iModel 3d content);
        // disableMagnification / alwaysSubdivideIncompleteTiles options not
        // surfaced yet (registered — TileAdminProps plumbing pending).
        canSkipSubdivision = canSkipSubdivision && header.tolerance <= kMaxLeafTolerance;
        canSkipSubdivision = canSkipSubdivision && completeTile;

        if (canSkipSubdivision) {
            if (completeTile && 0 == header.numElementsExcluded
                && header.numElementsIncluded <= kMinElementsPerTile) {
                bool const containsCurves =
                    0 != (static_cast<uint32_t>(header.flags) & static_cast<uint32_t>(ImdlFlags::ContainsCurves));
                if (!containsCurves)
                    isLeaf = true;      // :927
                else
                    sizeMultiplier = 1.0;  // :925-928
            } else if (header.numElementsIncluded + header.numElementsExcluded <= kMinElementsPerTile) {
                sizeMultiplier = 1.0;   // :929-931
            }
        }
    }

    ImdlContentDescription desc;
    desc.contentRange = header.contentRange;
    desc.isLeaf = isLeaf;
    desc.sizeMultiplier = sizeMultiplier;
    desc.emptySubRangeMask = header.emptySubRanges;
    return desc;
}

std::optional<ImdlDocument> parseImdlDocument(ImdlByteStream& stream,
                                              ImdlFeatureTableHeader const* featureHeader,
                                              std::vector<uint32_t> const* featureWords)
{
    // Ported from: parseImdlDocument's glTF-section subset
    // (ParseImdlDocument.ts:1287-1320 — GltfHeader validation, JSON scene
    // extraction, binary section; the JSON→typed-Document walk and the
    // graphics pass land with G-D3).
    GltfHeader const gltf(stream);
    if (!gltf.isValid)
        return std::nullopt;

    // The JSON scene sits at scenePosition (the stream is already there for
    // both v1 and v2 — the v2 branch consumed the 20-byte header).
    stream.advance(gltf.scenePosition - stream.curPos());

    ImdlDocument doc;
    // Feature table data (caller pre-read — the words between the 12-byte
    // header and the glTF section; PackedFeatureTable 3×u32/feature layout).
    if (featureHeader && featureWords) {
        doc.featureCount = featureHeader->count;
        doc.featureTableLength = featureHeader->length;
        doc.featureData = *featureWords;
    }
    doc.sceneJson.resize(gltf.sceneStrLength);
    if (gltf.sceneStrLength > 0) {
        stream.readBytes(doc.sceneJson.data(), gltf.sceneStrLength);
        if (stream.isPastTheEnd())
            return std::nullopt;  // InvalidScene (truncated JSON)
    }

    // The binary section (v2: after the binary chunk header).
    if (gltf.binaryPosition > stream.curPos())
        stream.advance(gltf.binaryPosition - stream.curPos());
    if (!stream.isPastTheEnd() && stream.remainingSize() > 0) {
        doc.binary.assign(stream.remainingData(), stream.remainingData() + stream.remainingSize());
    }

    // Material fill color (single-material minimum: the first material's
    // fillColor, ParseImdlDocument.ts displayParamsFromJson :1210-1215).
    if (auto json = tilejson::parseJsonDocument(doc.sceneJson)) {
        if (tilejson::JsonValue const* mats = json->find("materials")) {
            for (auto const& mat : mats->obj) {
                if (tilejson::JsonValue const* fc = mat.second.find("fillColor")) {
                    doc.materialFillColor = static_cast<uint32_t>(fc->number);
                    doc.hasMaterialFillColor = true;
                    break;
                }
            }
        }
    }

    return doc;
}

END_DQ_RENDER_NAMESPACE
