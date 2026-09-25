// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl graphics creation (minimal pass)
// Ported from: itwinjs-core core/frontend/src/common/imdl/ImdlGraphicsCreator.ts
//              (decodeImdlGraphics :414-437 — node walk → mesh graphics) and
//              the VertexTable quantized-position decoding the reference
//              performs on the GPU (VertexTable.ts layout); the DanQing pass
//              decodes on the CPU into IndexedPolyface (H-3, layout verified
//              byte-level against the recorded v1.1 fixtures 2026-09-23:
//              first three u16 per vertex = quantized x/y/z, surface indices
//              24-bit LE, world = decodedMin + q*(decodedMax-min)/65535).
//              createImdlLutGraphics 为 U7 归位主路径：零 CPU 逐顶点解码，
//              线上 RGBA8 顶点表原样上传 LUT 纹理（VertexLUT.ts:93-99），
//              shader 侧采样解量化（glsl/Vertex.ts computeVertexPosition）。
#include "dqRender/tile/ImdlDocument.h"
#include "dqRender/tile/ImdlHeader.h"
#include "dqRender/tile/RealityTileTree.h"
#include "dqRender/RenderSystem.h"

#include "render/MeshGraphic.h"
#include "render/SurfaceGeometry.h"
#include "render/VertexLutTexture.h"
#include "render/VertexTableBuilder.h"

#include "TilesetJson.h"

#include "dqRender/rhi/BufferDescriptor.h"
#include "dqRender/rhi/Driver.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/IndexedPolyface.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Locate a bufferView's byte span inside the imdl document's binary section.
bool findBufferView(tilejson::JsonValue const& doc, std::string const& name,
                    std::vector<uint8_t> const& binary,
                    uint8_t const*& outData, size_t& outSize)
{
    tilejson::JsonValue const* views = doc.find("bufferViews");
    if (!views)
        return false;
    tilejson::JsonValue const* view = views->find(name.c_str());
    if (!view)
        return false;
    tilejson::JsonValue const* off = view->find("byteOffset");
    tilejson::JsonValue const* len = view->find("byteLength");
    if (!off || !len)
        return false;
    size_t const offset = static_cast<size_t>(off->number);
    size_t const length = static_cast<size_t>(len->number);
    if (offset + length > binary.size())
        return false;
    outData = binary.data() + offset;
    outSize = length;
    return true;
}

}  // namespace

std::vector<dqBase::RefPtr<dqGeom::IndexedPolyface>> DQ_RENDER_EXPORT
decodeImdlGraphics(ImdlDocument const& doc)
{
    std::vector<dqBase::RefPtr<dqGeom::IndexedPolyface>> meshes;

    auto json = tilejson::parseJsonDocument(doc.sceneJson);
    if (!json)
        return meshes;

    // Reparse the JSON: the document's sceneJson is re-read by find callers;
    // parseImdlMeshPrimitives wants the parsed tree.
    auto primitives = tilejson::parseImdlMeshPrimitives(*json);
    for (auto const& prim : primitives) {
        uint8_t const* vertData = nullptr;
        size_t vertSize = 0;
        if (!findBufferView(*json, prim.vertices.bufferView, doc.binary, vertData, vertSize))
            continue;
        size_t const bytesPerVertex = prim.vertices.numRgbaPerVertex * 4;
        if (bytesPerVertex == 0 || vertSize < prim.vertices.count * bytesPerVertex)
            continue;

        uint8_t const* indexData = nullptr;
        size_t indexSize = 0;
        if (!findBufferView(*json, prim.surface.indicesView, doc.binary, indexData, indexSize))
            continue;
        size_t const numIndices = indexSize / 3;  // 24-bit LE each
        if (numIndices < 3 || numIndices % 3 != 0)
            continue;

        auto polyface = dqGeom::IndexedPolyface::create(true /*needNormals*/);
        // Quantized positions → world (decodedMin/decodedMax affine) + oct normals.
        double const range[3] = {
            prim.vertices.decodedMax[0] - prim.vertices.decodedMin[0],
            prim.vertices.decodedMax[1] - prim.vertices.decodedMin[1],
            prim.vertices.decodedMax[2] - prim.vertices.decodedMin[2],
        };
        for (uint32_t v = 0; v < prim.vertices.count; ++v) {
            uint8_t const* base = vertData + v * bytesPerVertex;
            uint16_t const q[3] = {
                static_cast<uint16_t>(base[0] | (base[1] << 8)),
                static_cast<uint16_t>(base[2] | (base[3] << 8)),
                static_cast<uint16_t>(base[4] | (base[5] << 8)),
            };
            double const p[3] = {
                prim.vertices.decodedMin[0] + range[0] * q[0] / 65535.0,
                prim.vertices.decodedMin[1] + range[1] * q[1] / 65535.0,
                prim.vertices.decodedMin[2] + range[2] * q[2] / 65535.0,
            };
            polyface->AddPoint(dqGeom::Point3d(p[0], p[1], p[2]));

            // Oct-encoded normal：量化 mesh 顶点表 16B 布局（Quantized.
            // LitMeshBuilder，VertexTableBuilder.ts:385-398——position 6B +
            // colorIndex 2B + featureIndex 4B + octNormal 2B + unused 2B）：
            // 法线在 bytes 12-13（texel3.xy）。LUT 主路径（法线经 shader 读
            // g_vertLutData3.xy，Surface.ts:529-566）即此布局；2026-09-25 修正
            // 旧误读（bytes 6-7 实为 colorIndex——录制夹具 uniform 色下 color
            // 表为空恒 0，旧误读产出恒定错法线而像素锁未检出）。
            double const ex = base[12] / 255.0 * 2.0 - 1.0;
            double const ey = base[13] / 255.0 * 2.0 - 1.0;
            double nx = ex, ny = ey;
            double nz = 1.0 - std::abs(nx) - std::abs(ny);
            if (nz < 0.0) {
                // Hemisphere fix (Surface.ts:389-392).
                double const sx = nx >= 0.0 ? 1.0 : -1.0;
                double const sy = ny >= 0.0 ? 1.0 : -1.0;
                double const tx = (1.0 - std::abs(ny)) * sx;
                double const ty = (1.0 - std::abs(nx)) * sy;
                nx = tx;
                ny = ty;
                nz = 0.0;
            }
            double const len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-12)
                polyface->AddNormal(dqGeom::Vector3d(nx / len, ny / len, nz / len));
        }
        // Surface indices: triangles of consecutive 24-bit LE indices, each
        // facet terminated via point-index chain (AddPointIndex).
        size_t const numTriangles = numIndices / 3;
        for (size_t t = 0; t < numTriangles; ++t) {
            size_t const base = t * 9;
            uint32_t const ia = indexData[base] | (indexData[base + 1] << 8) | (static_cast<uint32_t>(indexData[base + 2]) << 16);
            uint32_t const ib = indexData[base + 3] | (indexData[base + 4] << 8) | (static_cast<uint32_t>(indexData[base + 5]) << 16);
            uint32_t const ic = indexData[base + 6] | (indexData[base + 7] << 8) | (static_cast<uint32_t>(indexData[base + 8]) << 16);
            // 24-bit surface indices are 0-based; IndexedPolyface uses
            // 1-based point indices (same conversion as the glTF reader —
            // GltfReader.cpp "cgltf indices are 0-based" note; index 0 is
            // the skip sentinel in PolyfaceGraphic::buildFromPolyface, the
            // TD-19 root cause: 0-based indices skipped EVERY corner).
            polyface->AddPointIndex(static_cast<int32_t>(ia) + 1);
            polyface->AddPointIndex(static_cast<int32_t>(ib) + 1);
            polyface->AddPointIndex(static_cast<int32_t>(ic) + 1);
            // Per-corner normal indices (1-based, same as points).
            polyface->AddNormalIndex(static_cast<int32_t>(ia) + 1);
            polyface->AddNormalIndex(static_cast<int32_t>(ib) + 1);
            polyface->AddNormalIndex(static_cast<int32_t>(ic) + 1);
            polyface->TerminateFacet();
        }
        if (polyface && polyface->FacetCount() > 0)
            meshes.push_back(std::move(polyface));
    }

    return meshes;
}

// ---------------------------------------------------------------------------
// createImdlLutGraphics —— imdl 顶点表 LUT 直传主路径（U7 归位）。
// Ported from: itwinjs-core VertexLUT.ts createFromVertexTable (:93-99——
//              TextureHandle.createForData(vt.width, vt.height, vt.data)
//              直传) + VertexTable.ts computeDimensions (:53-81——纹理尺寸
//              选择的回退路径) + ParseImdlDocument.ts parseVertexTable
//              (:1005-1008 无压缩时 bytes 即线上 bufferView 子段；
//              :1013-1018 QParams3d.fromRange——origin=decodedMin、
//              scale=(max-min)/65535；:1029-1042 width/height 原样自 JSON) +
//              ImdlGraphicsCreator.decodeImdlGraphics (:414-437 的 node walk
//              形态——每 mesh primitive 一个 graphic)。
// 线上字节即 LUT texel 布局（Quantized.LitMeshBuilder，VertexTableBuilder.ts
// :342-398：texel0=qx|qy、texel1=qz|colorIndex、texel2=featureIdx24|matIdx8、
// texel3=octNormal|unused——vertex shader 的 kPreReadVertexDataQuantized /
// computeVertexPosition 直接消费），零 CPU 逐顶点工作。
// ---------------------------------------------------------------------------
std::vector<RenderGraphic*> DQ_RENDER_EXPORT
createImdlLutGraphics(ImdlDocument const& doc, RenderSystem& system)
{
    std::vector<RenderGraphic*> graphics;

    rhi::Driver* driver = system.driver();
    if (!driver)
        return graphics;  // 桩/无 GL 系统——调用方回退 polyface 对照通道

    auto json = tilejson::parseJsonDocument(doc.sceneJson);
    if (!json)
        return graphics;

    auto primitives = tilejson::parseImdlMeshPrimitives(*json);
    for (auto const& prim : primitives) {
        uint8_t const* vertData = nullptr;
        size_t vertSize = 0;
        if (!findBufferView(*json, prim.vertices.bufferView, doc.binary, vertData, vertSize))
            continue;
        uint32_t const numRgba = prim.vertices.numRgbaPerVertex;
        uint32_t const count = prim.vertices.count;
        // 量化 mesh 顶点表为 12B（SimpleBuilder numRgba=3）或 16B（LitMesh
        // numRgba=4）；其余形态本期不消费（纹理/meshopt 登记 TODO，见头文件）。
        if ((numRgba != 3u && numRgba != 4u) || count == 0u)
            continue;
        if (vertSize < size_t(count) * numRgba * 4u)
            continue;

        // 纹理尺寸：JSON width/height 优先（ParseImdlDocument.ts:1033-1034
        // 原样进 VertexTable——生产端 tile 生成时已按 computeDimensions 写好；
        // 录制夹具即此形态，byteLength == width*height*4）。width/height 缺失
        // 时回退 VertexTable.ts:53-81 computeDimensions（1:1 移植复用
        // VertexTableBuilder::computeDimensions，勿重写）；回退且末行填充时
        // 需要 width*height*4 的 staging 拷贝（直传语义的保形字节拷贝，非逐
        // 顶点解码）。
        uint32_t texWidth = prim.vertices.width;
        uint32_t texHeight = prim.vertices.height;
        std::vector<uint8_t> staging;
        if (texWidth == 0u || texHeight == 0u
            || size_t(texWidth) * texHeight * 4u > vertSize) {
            // maxSize 2048：VertexTable.test.ts 的默认 maxDimension；itwinjs
            // 生产侧来源 IModelApp.renderSystem.maxTextureSize
            // （ImdlGraphicsCreator 的 maxVertexTableSize 选项）——GL 上限
            // 接线登记 TODO。
            VertexTableBuilder::computeDimensions(count, numRgba, 0u, 2048u,
                                                  texWidth, texHeight);
        }
        size_t const texBytes = size_t(texWidth) * texHeight * 4u;
        uint8_t const* uploadData = vertData;
        if (texBytes > vertSize) {
            staging.assign(vertData, vertData + vertSize);
            staging.resize(texBytes, 0u);  // 末行零填充（对齐参考 assert：
                                           // width*height >= nRgba）
            uploadData = staging.data();
        }

        // LUT 纹理：线上字节原样上传（VertexLUT.ts:93-99）。
        VertexLutTexture lut;
        if (!lut.create(*driver, uploadData, texWidth, texHeight, count, numRgba))
            continue;  // create 失败内部不自持纹理（nullid），无半成品
        // QParams3d（ParseImdlDocument.ts:1013-1018）。
        lut.setQuantization(
            static_cast<float>(prim.vertices.decodedMin[0]),
            static_cast<float>(prim.vertices.decodedMin[1]),
            static_cast<float>(prim.vertices.decodedMin[2]),
            static_cast<float>((prim.vertices.decodedMax[0] - prim.vertices.decodedMin[0]) / 65535.0),
            static_cast<float>((prim.vertices.decodedMax[1] - prim.vertices.decodedMin[1]) / 65535.0),
            static_cast<float>((prim.vertices.decodedMax[2] - prim.vertices.decodedMin[2]) / 65535.0));

        // 24-bit 索引流：surface.indices 字节原样入 BufferObject，作为
        // a_qPosition attribute（UBYTE3，stride 3，location 0——量化 Surface
        // 变体 attribute map，SurfaceVariantCompiler）的底层 BO；drawArrays
        // 无 element index buffer（SurfaceGeometry.ts:150-162）。
        uint8_t const* idxData = nullptr;
        size_t idxSize = 0;
        if (!findBufferView(*json, prim.surface.indicesView, doc.binary, idxData, idxSize)
            || idxSize < 3u || idxSize % 3u != 0u) {
            lut.destroy(*driver);  // 半成品清理（§12.9：LUT 已建、几何未成）
            continue;
        }
        auto const idxBo = driver->createBufferObject(
            static_cast<uint32_t>(idxSize), rhi::BufferObjectBinding::VERTEX,
            rhi::BufferUsage::STATIC);
        rhi::BufferDescriptor idxDesc(idxData, idxSize);
        driver->updateBufferObject(idxBo, std::move(idxDesc), 0);
        uint32_t const numIndices = static_cast<uint32_t>(idxSize / 3u);

        // a_qPosition VAO（照 PolylineGeometry corner VAO 创建段，
        // MeshGraphic.cpp:437-460）：单 attribute VBI——attribute 序即
        // location（bindRenderPrimitive 以数组下标为 location），stride 由
        // 驱动按 buffer 内 attribute 尺寸求和（UBYTE3 → 3）。
        rhi::AttributeArray attrs = {};
        attrs[0].buffer = 0;  // a_qPosition (location 0)
        attrs[0].offset = 0;
        attrs[0].type = rhi::ElementType::UBYTE3;
        auto const lutVbih = driver->createVertexBufferInfo(1, 1, attrs);
        auto const lutVbh = driver->createVertexBuffer(numIndices, lutVbih);
        driver->setVertexBufferObject(lutVbh, 0, idxBo);
        auto const primitive = driver->createRenderPrimitive(
            lutVbh, rhi::IndexBufferHandle{}, rhi::PrimitiveType::TRIANGLES);

        // SurfaceType：prim.surface.type 是 itwinjs SurfaceParams.SurfaceType
        // （Unlit=0/Lit=1/Textured=2/TexturedLit=3——SurfaceParams.ts:13-19，
        // 登记信息非 translucency），光照/纹理细分不在 DanQing 简化枚举
        // （Unknown/Opaque/Translucent/Planar）表达面内；translucency 路由
        // （vertices.hasTranslucency → Translucent pass）与 textured 变体同
        // 登记 TODO（见头文件）。非细分恒 Opaque——与旧 polyface 路径
        // （PolyfaceGraphic::getPass 恒 Opaque）像素等价。
        auto const surfaceType = SurfaceType::Opaque;

        auto geom = std::make_unique<SurfaceGeometry>(
            *driver, std::move(lut), idxBo, numIndices, surfaceType,
            prim.isPlanar, /*hasTextures*/ false, lutVbh, lutVbih);
        geom->setPrimitive(primitive);

        // 均匀色：u_color 源（glsl/Color.ts:51-60 ← lutGeom.getColor——
        // ColorInfo.createFromVertexTable 的 uniformColor 路径，
        // VertexLUT.ts:99 → ColorInfo；ParseImdlDocument.ts:1020
        // ColorDef.fromJSON(json.uniformColor)）。无 uniformColor 时
        // 缺省白（ColorInfo 缺省）。
        if (prim.vertices.hasUniformColor)
            geom->setColor(dqCommon::ColorDef::create(prim.vertices.uniformColor));
        else
            geom->setColor(dqCommon::ColorDef::create(0xFFFFFFFFu));

        // 每 mesh primitive 一个 MeshGraphic（decodeImdlGraphics :414-437
        // 的 graphic 链等价；调用方 createGraphicList + createBatch 包裹）。
        auto* meshGraphic = new MeshGraphic(*driver);
        meshGraphic->addSurface(std::move(geom));
        graphics.push_back(meshGraphic);
    }

    return graphics;
}

END_DQ_RENDER_NAMESPACE
