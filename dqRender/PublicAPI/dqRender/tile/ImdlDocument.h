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
#include <dqCommon/LinePixels.h>
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
    // 0.0 = undefined（DanQing 约定，同 ImdlTileMetadata.sizeMultiplier；
    // 参考 `sizeMultiplier?: number` 的 undefined，TileMetadata.ts:875）。
    double sizeMultiplier = 0.0;
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

// ---------------------------------------------------------------------------
// U11(1)：imdl 边缘参数（字节区间形态——JSON 侧的 bufferView 名由
// TilesetJson.h 的 parseImdlEdges 在解析期换入这里的数据）。
// Ported from: itwinjs-core ImdlModel.ts:63-84（Imdl.SegmentEdgeParams/
//              SilhouetteParams/IndexedEdgeParams/EdgeParams）+
//              internal/render/EdgeParams.ts:22-111（同一组类型的消费侧
//              定义——字段语义注释来源）。
// ---------------------------------------------------------------------------

// imdl bufferView 的字节区间（TS Uint8Array 的 C++ 形态，§3.4 适配——参考无
// C++ 对应物；data 指入 ImdlDocument::binary，binary 存活期内有效）。
struct DQ_RENDER_EXPORT ImdlByteView {
    uint8_t const* data = nullptr;
    size_t byteLength = 0;
};

// 一条硬边（mesh 顶点表中的两顶点连线，与视线无关恒可见）。
// Ported from: ImdlModel.ts:63-66 SegmentEdgeParams（EdgeParams.ts:22-31——
//              indices 为每 quad 顶点的 24-bit 索引；endPointAndQuadIndices
//              每索引 4B：24-bit 段另一端点索引 + 8-bit quad 角标 [0..3]）。
struct DQ_RENDER_EXPORT ImdlSegmentEdgeParams {
    ImdlByteView indices;
    ImdlByteView endPointAndQuadIndices;
};

// 曲面轮廓边（silhouette）——按边法线相对视线方向显隐。
// Ported from: ImdlModel.ts:68-70 SilhouetteParams（extends SegmentEdgeParams；
//              EdgeParams.ts:39-42——normalPairs 每索引 2×16-bit
//              OctEncodedNormal 对）。
struct DQ_RENDER_EXPORT ImdlSilhouetteParams : ImdlSegmentEdgeParams {
    ImdlByteView normalPairs;
};

// 边查找表：下分区为简单段边、上分区为 silhouette 边；两分区之间可能存在
// 一行混合 + 对齐填充字节。
// Ported from: ImdlModel.ts:74 EdgeTable（EdgeParams.ts:53-64——data/width/
//              height/numSegments/silhouettePadding）。
struct DQ_RENDER_EXPORT ImdlEdgeTable {
    ImdlByteView data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t numSegments = 0;
    uint32_t silhouettePadding = 0;
};

// 以查找表描述的边（每边 6 个相同索引构成 quad）。
// Ported from: ImdlModel.ts:72-75 IndexedEdgeParams（EdgeParams.ts:70-76）。
// C++ 所有权适配（§3.4，U11(3)）：`indexed` 直取形态（parseIndexedEdges
// ParseImdlDocument.ts:677-693）的两视图指入 ImdlDocument::binary（同
// segments 的区间语义）；`compact` 兜底展开形态（parseCompactEdges :695-708 →
// CompactEdges.ts indexedEdgeParamsFromCompactEdges）的产物字节在 TS 由 GC
// 持有（:91/:97 new Uint8Array），C++ 由下方两个 owned 向量持有、视图指入
// 其中——vector move 只转移堆缓冲，视图保持有效（parseImdlEdges 按值返回链）。
// 拷贝禁用（Task 6 评审④，M-C 收口）：拷贝会令新对象的 indices/edges.data
// 视图仍指入**被拷贝方**的 owned 向量——被拷贝方是临时对象时即悬空。唯一
// 消费链 parseImdlEdges（TilesetJson.h:596 out.indexed = std::move(ix)）与
// indexedEdgeParamsFromCompactEdges（CompactEdges.cpp:130 return out）均走
// move；移动转移堆缓冲不改 pointee 地址，视图跨 move 有效。
struct DQ_RENDER_EXPORT ImdlIndexedEdgeParams {
    ImdlIndexedEdgeParams() = default;
    ImdlIndexedEdgeParams(ImdlIndexedEdgeParams const&) = delete;
    ImdlIndexedEdgeParams& operator=(ImdlIndexedEdgeParams const&) = delete;
    ImdlIndexedEdgeParams(ImdlIndexedEdgeParams&&) = default;
    ImdlIndexedEdgeParams& operator=(ImdlIndexedEdgeParams&&) = default;

    ImdlByteView indices;
    ImdlEdgeTable edges;
    // compact 展开产物所有权（直取形态为空——视图指入 doc.binary）。
    std::vector<uint8_t> ownedIndices;
    std::vector<uint8_t> ownedEdgeTable;
};

// 一个 mesh 的边参数（weight = displayParams.width 像素宽；linePixels 线型）。
// Ported from: ImdlModel.ts:77-84 EdgeParams（EdgeParams.ts:98-111）；
//              polylineGroups（EdgeParams.ts:108 polyline 边）随 polyline
//              图元落 Task 6（TODO ParseImdlDocument.ts:716）。
struct DQ_RENDER_EXPORT ImdlEdgeParams {
    uint32_t weight = 0;
    dqCommon::LinePixels linePixels = dqCommon::LinePixels::Solid;
    std::optional<ImdlSegmentEdgeParams> segments;
    std::optional<ImdlSilhouetteParams> silhouettes;
    std::optional<ImdlIndexedEdgeParams> indexed;
};

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
