// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl document parsing (header → feature table → glTF)
// Ported from: itwinjs-core core/frontend/src/common/imdl/ParseImdlDocument.ts
//              (:1269-1327 parseImdlDocument — GltfHeader validation, JSON
//              scene extraction, binary section)
//              core/common/src/tile/TileMetadata.ts decodeTileContentDescription
//              (:880-940 — the content-description subset: skip feature table,
//              leaf/sizeMultiplier description)
#pragma once

#include "../Export.h"
#include "ImdlHeader.h"

#include <dqBase/RefCounted.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Range3d.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// The description of an imdl tile's content, derived from its header.
// Ported from: itwinjs-core TileContentDescription (TileMetadata.ts:880-940 —
// the decoded header summary ImdlReader consumes before parsing graphics).
struct DQ_RENDER_EXPORT ImdlContentDescription {
    dqGeom::Range3d contentRange;
    bool isLeaf = false;
    double sizeMultiplier = 1.0;
    uint32_t emptySubRangeMask = 0;
};

// A parsed imdl document: the glTF JSON scene + its binary section.
// Ported from: itwinjs-core Imdl.Document (ParseImdlDocument.ts — the subset
// the graphics creator reads: sceneJson/binary; textures/materials arrive
// with the graphics-creation pass, G-D3).
struct DQ_RENDER_EXPORT ImdlDocument {
    std::string sceneJson;             // glTF JSON chunk (UTF-8)
    std::vector<uint8_t> binary;       // glTF BIN chunk
    uint32_t featureCount = 0;         // feature table count (0 = none)
    uint32_t featureTableLength = 0;   // bytes the feature table occupies
    std::vector<uint32_t> featureData; // packed features: 3×u32 per feature
                                       // (elementId lo/hi + subCategory,
                                       // PackedFeatureTable.ts:139-141)

    // Material fill color from the scene's first material (0x00RRGGBB int —
    // the reference's fillColor JSON field; single-material minimum face).
    uint32_t materialFillColor = 0;
    bool hasMaterialFillColor = false;
};

// Decode the content description from a stream positioned right after the
// ImdlHeader: reads + skips the feature table, applies the leaf heuristic.
// Ported from: decodeTileContentDescription (TileMetadata.ts:880-940).
//   - empty tile or volume classifier → leaf (:904-906);
//   - magnification allowed + complete + few elements + no curves → leaf
//     with sizeMultiplier 1 (:910-928);
// Returns nullopt on malformed data (truncation).
std::optional<ImdlContentDescription> DQ_RENDER_EXPORT
decodeImdlContentDescription(ImdlHeader const& header, ImdlByteStream& stream);

// Header-only description (for callers that consumed the feature table in
// between — the description derives purely from the header).
std::optional<ImdlContentDescription> DQ_RENDER_EXPORT
decodeImdlContentDescriptionHeaderOnly(ImdlHeader const& header);

// Parse the imdl document (glTF section) from a stream positioned right
// after the feature table. Returns nullopt on a malformed glTF section.
// Ported from: parseImdlDocument (ParseImdlDocument.ts:1269-1327 —
// GltfHeader magic/version checks, JSON chunk, BIN chunk; DanQing subset:
// no meshopt/texture resolution yet — those land with the graphics pass).
std::optional<ImdlDocument> DQ_RENDER_EXPORT
parseImdlDocument(ImdlByteStream& stream,
                  ImdlFeatureTableHeader const* featureHeader = nullptr,
                  std::vector<uint32_t> const* featureWords = nullptr);

// Decode the imdl document's meshes into polyfaces (the minimal graphics
// pass: quantized vertex tables → IndexedPolyface; per-vertex colors/normals
// land with the full graphics pass, uniform color carried by the props).
// Ported from: ImdlGraphicsCreator.decodeImdlGraphics
// (ImdlGraphicsCreator.ts:414-437) — the DanQing CPU-decoding equivalent.
std::vector<dqBase::RefPtr<dqGeom::IndexedPolyface>> DQ_RENDER_EXPORT
decodeImdlGraphics(ImdlDocument const& doc);

END_DQ_RENDER_NAMESPACE
