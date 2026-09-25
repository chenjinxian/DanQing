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
#include "../RenderGraphic.h"
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

class RenderSystem;

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
// 保留为无 GL 环境（桩 RenderSystem）与调试对照通道；生产路径走
// createImdlLutGraphics（U7 LUT 直传）。
std::vector<dqBase::RefPtr<dqGeom::IndexedPolyface>> DQ_RENDER_EXPORT
decodeImdlGraphics(ImdlDocument const& doc);

// imdl 量化顶点表的 LUT 直传创建（零 CPU 逐顶点解码——线上 RGBA8 顶点表
// 即 LUT texel 布局，JSON width/height 选纹理尺寸后原样上传；
// VertexTable.ts:53-81 computeDimensions 为 width/height 缺失时的回退）。
// Ported from: itwinjs-core VertexLUT.ts（:93-99 直传 + VertexTable.ts:53-81
//              computeDimensions + ParseImdlDocument.ts:1029-1042 parseVertexTable
//              ——data 即线上 bufferView、width/height 原样自 JSON）+
//              ImdlGraphicsCreator.decodeImdlGraphics（:414-437 的 node walk
//              形态——每 mesh primitive 一个 graphic）。
// 返回的 graphic 链与 decodeImdlGraphics 等价（每 mesh primitive 一个
// MeshGraphic，由调用方 createGraphicList + createBatch 包裹）；裸指针
// 所有权交调用方（readContent → system.createGraphicList）。
// system.driver() == nullptr（桩/无 GL 系统）→ 返回空，调用方回退 polyface 路径。
// TODO（后续里程碑，ParseImdlDocument.ts:969-1003）：meshopt 压缩顶点表
// （compressedSize 分支）；surface.uvParams → textured 变体（hasTextures，
// TexturedLitMeshBuilder 布局 octNormal@6-7/qUV@12-15，
// VertexTableBuilder.ts:346-383）；12B SimpleBuilder 无光照网格（numRgba=3，
// 量化 shader pre-read 会采到下一顶点 texel0，需 unlit 变体配合）；
// json.featureID uniform 语义（uniformFeatureID :1011）与非均匀
// featureIndexType 的 LUT 颜色表采样。
std::vector<RenderGraphic*> DQ_RENDER_EXPORT
createImdlLutGraphics(ImdlDocument const& doc, RenderSystem& system);

END_DQ_RENDER_NAMESPACE
