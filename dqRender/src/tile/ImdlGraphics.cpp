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
#include "render/LineCode.h"
#include "render/SurfaceGeometry.h"
#include "render/IndexedEdgeGeometry.h"
#include "render/InstancedGeometry.h"
#include "render/InstanceBuffers.h"
#include "render/VertexLutTexture.h"
#include "render/VertexTableBuilder.h"

#include "TilesetJson.h"

#include "dqRender/rhi/BufferDescriptor.h"
#include "dqRender/rhi/Driver.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/IndexedPolyface.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

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
        if (!tilejson::findBufferView(*json, prim.vertices.bufferView, doc.binary, vertData, vertSize))
            continue;
        size_t const bytesPerVertex = prim.vertices.numRgbaPerVertex * 4;
        if (bytesPerVertex == 0 || vertSize < prim.vertices.count * bytesPerVertex)
            continue;

        uint8_t const* indexData = nullptr;
        size_t indexSize = 0;
        if (!tilejson::findBufferView(*json, prim.surface.indicesView, doc.binary, indexData, indexSize))
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
        // 布局守卫（2026-09-25 评审）：octNormal@12-13 仅 16B LitMeshBuilder
        // 布局（numRgbaPerVertex==4，VertexTableBuilder.ts:385-398——position
        // 6B + colorIndex 2B + featureIndex 4B + octNormal 2B + unused 2B）
        // 合法。12B SimpleBuilder（numRgba==3，VertexTableBuilder.ts:224-279）
        // 顶点表无 octNormal——无条件读 base[12]/[13] 会越入下一顶点的位置
        // 字节，顶点表居 BIN 末尾时对末顶点构成 1-2 字节正式 OOB。12B 时
        // 跳过法线（无光照网格本无 octNormal；下游 buildFromPolyface 的
        // hasNormals=false 走 facet 法线路径，PolyfaceGraphic.cpp:77-86）。
        // 16B textured 布局又不同（TexturedLitMeshBuilder：octNormal@6-7、
        // qUV@12-15，VertexTableBuilder.ts:346-383）——textured 消费已
        // deferred（createImdlLutGraphics TODO），届时按参考布局重审。
        bool const hasOctNormal = bytesPerVertex >= 16u;
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

            // Oct-encoded normal：量化 mesh 顶点表 16B LitMesh 布局
            // （bytes 12-13 = texel3.xy；LUT 主路径法线经 shader 读
            // g_vertLutData3.xy，Surface.ts:529-566；2026-09-25 修正旧误读
            // bytes 6-7——实为 colorIndex）。仅 hasOctNormal（16B）布局读取，
            // 12B 表无 octNormal 数据（布局守卫见上）。
            if (hasOctNormal) {
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
            // Per-corner normal indices (1-based, same as points)——与法线
            // 同一布局守卫：12B 表无法线数据，normalIndex 必须留空（否则
            // 下游 buildFromPolyface 按索引取到不存在的法线）。
            if (hasOctNormal) {
                polyface->AddNormalIndex(static_cast<int32_t>(ia) + 1);
                polyface->AddNormalIndex(static_cast<int32_t>(ib) + 1);
                polyface->AddNormalIndex(static_cast<int32_t>(ic) + 1);
            }
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
        if (!tilejson::findBufferView(*json, prim.vertices.bufferView, doc.binary, vertData, vertSize))
            continue;
        uint32_t const numRgba = prim.vertices.numRgbaPerVertex;
        uint32_t const count = prim.vertices.count;
        // 量化 LitMesh 顶点表为 16B（numRgba=4，Quantized.LitMeshBuilder）；
        // 12B SimpleBuilder（numRgba=3，无光照网格）本期拒绝——LUT 链路对
        // 12B 不安全：Task 3 量化 shader 的 pre-read 采样 g_vertLutData3
        // （12B 表下采到下一顶点 texel0）、Task 4 LUT 构造器恒 FillFlags::Lit
        // （评审 2026-09-25；参考侧无光照网格本不读法线，glsl/Surface.ts
        // addNormal 仅在 wantNormals 时）。unlit 12B 接线登记 TODO。
        if (numRgba != 4u || count == 0u)
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
        // 注意（刻意的健壮性扩展，参考 failure mode 不同）：参考在 JSON
        // width/height 缺失/不一致时直接丢 mesh（VertexLUT.ts:93-96
        // createFromVertexTable → undefined → 该 primitive 无 graphic）；此处
        // 选择保活 mesh——离线/转制容器的防御差异，登记于此（评审 2026-09-25）。
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
        if (!tilejson::findBufferView(*json, prim.surface.indicesView, doc.binary, idxData, idxSize)
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

        // 色信息：ColorInfo.createFromVertexTable 两形态（VertexLUT.ts:97——
        // uniformColor 有 → createFromColorDef（u_color 源，ParseImdlDocument
        // .ts:1020 ColorDef.fromJSON）；无 → createNonUniform（ColorInfo.ts:35
        // 色表形态——色表字节已随顶点表 LUT 直传在内，shader 侧
        // kShaderBit_NonUniformColor 位选 lutColor 采样，Color.ts:16-26；
        // 此前此处曾强制白 0xFFFFFFFF——M-I(3) 归位参考语义）。
        if (prim.vertices.hasUniformColor)
            geom->setColor(dqCommon::ColorDef::create(prim.vertices.uniformColor));
        else
            geom->setNonUniformColor();

        // -------------------------------------------------------------------
        // TD-25：instances 修饰消费（ImdlSchema.ts:160-166 → InstancedGraphicParams
        // 链）。参考 getModifiers（ImdlGraphicsCreator.ts:243-255 mod.type ==
        // "instances" → InstancedGraphicParams.fromProps）+ createPrimitiveGraphic
        //（:317-321 system.createRenderGraphic(geometry, mods.instances)）+
        // webgl System.ts:577-590（InstanceBuffers.fromParams →
        // MeshGraphic.create(geom, buffers)）。消费点复现的归零语义
        //（ParseImdlDocument.ts:1045-1087 的可达子集）：count<=0 + 三
        // bufferView 缺失/尺寸不符（featureIds 3B/实例、transforms 48B/实例
        // = 参考 :1060-1071 findBuffer + %12 assert 的 C++ 显式防御）→ 无
        // 实例化（按原有几何绘制）。参考 :1054-1056 的 transformCenter 长度
        // ≠3 放弃语义在解析层发生，DanQing 提取层已丢失原始长度（零填充
        // double[3]）——不可复现，登记（采集资产均带合法 3 分量，当前不可达）。
        // 带 instances 的 primitive 其 edges 亦应共享实例缓冲（MeshGraphic.
        // create 的 _instances 覆盖全部图元——Mesh.ts:123-143）；本资产
        // prim1 边缘 numVisible=0（无边缘几何）未触发——接线归后续
        // （边缘实例化需 edge 变体的实例化 attribute 组，TODO 登记）。
        // -------------------------------------------------------------------
        InstanceBuffers* instanceBuffers = nullptr;
        if (prim.instances) {
            auto const& inst = *prim.instances;
            uint8_t const* tdata = nullptr;
            size_t tsize = 0;
            uint8_t const* fiddata = nullptr;
            size_t fidsize = 0;
            uint8_t const* symdata = nullptr;
            size_t symsize = 0;
            bool const viewsOk =
                inst.count > 0
                && tilejson::findBufferView(*json, inst.transformsView, doc.binary,
                                            tdata, tsize)
                && tilejson::findBufferView(*json, inst.featureIdsView, doc.binary,
                                            fiddata, fidsize)
                && (tsize % (12u * sizeof(float)) == 0u)
                && (tsize / (12u * sizeof(float)) == inst.count)
                && (fidsize == static_cast<size_t>(inst.count) * 3u);
            bool const symOk = !inst.symbologyOverridesView
                || tilejson::findBufferView(*json, *inst.symbologyOverridesView,
                                            doc.binary, symdata, symsize);
            if (viewsOk && symOk
                && (!symdata || symsize == static_cast<size_t>(inst.count) * 8u)) {
                float const tc[3] = {
                    static_cast<float>(inst.transformCenter[0]),
                    static_cast<float>(inst.transformCenter[1]),
                    static_cast<float>(inst.transformCenter[2]),
                };
                instanceBuffers = InstanceBuffers::create(
                    *driver, inst.count,
                    reinterpret_cast<float const*>(tdata), tc,
                    fiddata, symdata);
            }
            if (getenv("DANQING_INST_TRACE")) {
                std::fprintf(stderr,
                             "[INST] primitive instances: count=%u views=%d sym=%d "
                             "-> buffers=%p\n",
                             inst.count, viewsOk ? 1 : 0, symOk ? 1 : 0,
                             static_cast<void*>(instanceBuffers));
            }
        }

        // 每 mesh primitive 一个 MeshGraphic（decodeImdlGraphics :414-437
        // 的 graphic 链等价；调用方 createGraphicList + createBatch 包裹）。
        auto* meshGraphic = new MeshGraphic(*driver);
        // edge 几何以非拥有方式观察 surface 持有的 LUT（unique_ptr 迁移不改
        // pointee 地址——观察引用跨 move 稳定；生命周期见 EdgeGeometry 注）。
        VertexLutTexture const& lutView = geom->getLut();
        dqCommon::ColorDef const meshColor = prim.vertices.hasUniformColor
            ? dqCommon::ColorDef::create(prim.vertices.uniformColor)
            : dqCommon::ColorDef::create(0xFFFFFFFFu);
        // 注：非均匀 prim 的 meshColor 仅剩 edge 路径消费（EdgeGeometry/
        // SilhouetteEdgeGeometry/indexed LUT 的 u_color——M-I(3) 后 surface
        // 已走色表采样，不再消费此值）。参考侧 edge 基色 = computeEdgeColor
        //（Target.ts:607-610——无 EdgeSettings 覆盖时透传非均匀 colorInfo），
        // edge 变体的色表采样随 Task 4 边线两段接线（登记于该任务）。
        // instances 修饰：InstancedGeometry 包裹 LUT surface（wrapper 非拥有
        // 观察 geom——所有权经 addInstancedSurface 入 MeshGraphic）。
        if (instanceBuffers) {
            meshGraphic->addInstancedSurface(std::move(geom), instanceBuffers);
        } else {
            meshGraphic->addSurface(std::move(geom));
        }

        // -------------------------------------------------------------------
        // U11(2)：edges → edge graphics（与 surface 并列，Batch 包裹不变）。
        // Ported from: itwinjs-core Mesh.ts:47-53 MeshRenderGeometry ctor——
        // silhouetteEdges 先于 segmentEdges 创建；EdgeGeometry.ts
        // create/createSilhouettes（字节流原样入 BO，零 CPU 重排——形态同
        // 顶点表 LUT 直传）+ MeshData.ts:93-94（edgeWidth/edgeLineCode）。
        // -------------------------------------------------------------------
        if (auto edges = tilejson::parseImdlEdges(*json, doc, prim)) {
            if (getenv("DANQING_EDGE_TRACE"))
                std::fprintf(stderr,
                             "[EDGE] primitive edges: seg=%d sil=%d w=%u\n",
                             edges->segments ? 1 : 0, edges->silhouettes ? 1 : 0,
                             edges->weight);
            // segment quad 展开的顶点数 = 索引流字节/3（每顶点 24-bit 索引）；
            // 对端点流必须逐顶点 4B（24-bit 对端点索引 + 8-bit quad 角标），
            // silhouette 的 normalPairs 逐顶点 4B（2×16-bit oct 法线对）。
            // 参考侧由 VertexIndices 构造隐含对齐（EdgeParams.ts:100-141 展开
            // 端每索引 3B/4B/4B）；线载荷不齐时丢弃该成员（参考 assert 语义的
            // 生产侧防御——参考 dev assert 在 release 直接未定义，此处显式跳过）。
            auto uploadBo = [&driver](ImdlByteView const& view) {
                auto bo = driver->createBufferObject(
                    static_cast<uint32_t>(view.byteLength),
                    rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
                driver->updateBufferObject(
                    bo, rhi::BufferDescriptor(view.data, view.byteLength), 0);
                return bo;
            };

            // silhouettes 先建（Mesh.ts:49）——SilhouetteEdgeGeometry：第三条
            // a_normals 流 + SilhouetteEdge technique。
            if (edges->silhouettes) {
                auto const& sil = *edges->silhouettes;
                size_t const silVertCount = sil.indices.byteLength / 3;
                if (silVertCount > 0 && sil.indices.byteLength % 3 == 0
                    && sil.endPointAndQuadIndices.byteLength == silVertCount * 4
                    && sil.normalPairs.byteLength == silVertCount * 4) {
                    auto const silIdxBo = uploadBo(sil.indices);
                    auto const silEpqBo = uploadBo(sil.endPointAndQuadIndices);
                    auto const silNpBo = uploadBo(sil.normalPairs);
                    rhi::AttributeArray silAttrs = {};
                    silAttrs[0].buffer = 0;  // a_pos（24-bit 顶点表索引）
                    silAttrs[0].offset = 0;
                    silAttrs[0].type = rhi::ElementType::UBYTE3;
                    silAttrs[1].buffer = 1;  // a_endPointAndQuadIndices
                    silAttrs[1].offset = 0;
                    silAttrs[1].type = rhi::ElementType::UBYTE4;
                    silAttrs[2].buffer = 2;  // a_normals（silhouette 专属）
                    silAttrs[2].offset = 0;
                    silAttrs[2].type = rhi::ElementType::UBYTE4;
                    auto const silVbih = driver->createVertexBufferInfo(3, 3, silAttrs);
                    auto const silVbh = driver->createVertexBuffer(
                        static_cast<uint32_t>(silVertCount), silVbih);
                    driver->setVertexBufferObject(silVbh, 0, silIdxBo);
                    driver->setVertexBufferObject(silVbh, 1, silEpqBo);
                    driver->setVertexBufferObject(silVbh, 2, silNpBo);
                    auto const silPrim = driver->createRenderPrimitive(
                        silVbh, rhi::IndexBufferHandle{}, rhi::PrimitiveType::TRIANGLES);
                    auto g = std::make_unique<SilhouetteEdgeGeometry>(
                        *driver, silIdxBo, silEpqBo, silNpBo,
                        static_cast<uint32_t>(silVertCount), lutView,
                        static_cast<float>(edges->weight),
                        LineCode::valueFromLinePixels(edges->linePixels), meshColor,
                        prim.isPlanar);
                    g->setPrimitive(silPrim);
                    g->setVbhResources(silVbh, silVbih);
                    meshGraphic->addEdge(std::move(g));
                }
            }
            // segments 后建（Mesh.ts:52）——EdgeGeometry：双流 + Edge technique。
            if (edges->segments) {
                auto const& seg = *edges->segments;
                size_t const segVertCount = seg.indices.byteLength / 3;
                if (segVertCount > 0 && seg.indices.byteLength % 3 == 0
                    && seg.endPointAndQuadIndices.byteLength == segVertCount * 4) {
                    auto const segIdxBo = uploadBo(seg.indices);
                    auto const segEpqBo = uploadBo(seg.endPointAndQuadIndices);
                    rhi::AttributeArray segAttrs = {};
                    segAttrs[0].buffer = 0;
                    segAttrs[0].offset = 0;
                    segAttrs[0].type = rhi::ElementType::UBYTE3;
                    segAttrs[1].buffer = 1;
                    segAttrs[1].offset = 0;
                    segAttrs[1].type = rhi::ElementType::UBYTE4;
                    auto const segVbih = driver->createVertexBufferInfo(2, 2, segAttrs);
                    auto const segVbh = driver->createVertexBuffer(
                        static_cast<uint32_t>(segVertCount), segVbih);
                    driver->setVertexBufferObject(segVbh, 0, segIdxBo);
                    driver->setVertexBufferObject(segVbh, 1, segEpqBo);
                    auto const segPrim = driver->createRenderPrimitive(
                        segVbh, rhi::IndexBufferHandle{}, rhi::PrimitiveType::TRIANGLES);
                    auto g = std::make_unique<EdgeGeometry>(
                        *driver, segIdxBo, segEpqBo, static_cast<uint32_t>(segVertCount),
                        lutView, static_cast<float>(edges->weight),
                        LineCode::valueFromLinePixels(edges->linePixels), meshColor,
                        prim.isPlanar);
                    g->setPrimitive(segPrim);
                    g->setVbhResources(segVbh, segVbih);
                    meshGraphic->addEdge(std::move(g));
                }
            }

            // ---------------------------------------------------------------
            // U11(3)：indexed edges → EdgeLUT 纹理 + 索引 BO + IndexedEdgeGeometry
            //（compact 兜底展开已在 parseImdlEdges 内完成——ParseImdlDocument.ts
            // :719-720）。
            // Ported from: itwinjs-core Mesh.ts:64-65（indexedEdges 与 surface/
            // segments/silhouettes 同一 MeshRenderGeometry，创建序最后）+
            // IndexedEdgeGeometry.ts create (:98-102——indexBuffer =
            // createArrayBuffer(params.indices.data)、lut = EdgeLUT.create(
            // params.edges)、numIndices = params.indices.length) + EdgeLUT.create
            // (:46-49 TextureHandle.createForData(table.width, table.height,
            // table.data)) + ctor (:74-86——a_pos UBYTE3 单流 VAO、width/lineCode
            // ← appearance ?? MeshData.edgeWidth/edgeLineCode、colorInfo ←
            // mesh.lut.colorInfo)。
            // ---------------------------------------------------------------
            if (edges->indexed) {
                auto const& ix = *edges->indexed;
                size_t const ixVertCount = ix.indices.byteLength / 3;
                if (ixVertCount > 0 && ix.indices.byteLength % 3 == 0) {
                    auto edgeLut = EdgeLUT::create(
                        *driver, ix.edges.data.data, ix.edges.width, ix.edges.height,
                        ix.edges.numSegments, ix.edges.silhouettePadding);
                    if (edgeLut.isValid()) {
                        auto const ixBo = uploadBo(ix.indices);
                        rhi::AttributeArray ixAttrs = {};
                        ixAttrs[0].buffer = 0;  // a_pos = 24-bit 边查找表索引
                        ixAttrs[0].offset = 0;
                        ixAttrs[0].type = rhi::ElementType::UBYTE3;
                        auto const ixVbih = driver->createVertexBufferInfo(1, 1, ixAttrs);
                        auto const ixVbh = driver->createVertexBuffer(
                            static_cast<uint32_t>(ixVertCount), ixVbih);
                        driver->setVertexBufferObject(ixVbh, 0, ixBo);
                        auto const ixPrim = driver->createRenderPrimitive(
                            ixVbh, rhi::IndexBufferHandle{}, rhi::PrimitiveType::TRIANGLES);
                        // colorInfo ← mesh.lut.colorInfo（IndexedEdgeGeometry.ts:83）
                        // ——DanQing imdl 通路顶点表为均匀色（ColorInfo uniform 路径
                        // 与参考 ColorInfo.createFromVertexTable 的 uniform 分支等价；
                        // 非均匀色表登记 TODO，Surface 量化变体同策略）。
                        dqCommon::ColorComponents const mc = meshColor.getColors();
                        ColorInfo const colorInfo = ColorInfo::fromUniform(
                            (static_cast<uint32_t>(mc.r) << 16)
                                | (static_cast<uint32_t>(mc.g) << 8)
                                | static_cast<uint32_t>(mc.b),
                            static_cast<uint8_t>(255 - mc.t));
                        auto g = std::make_unique<IndexedEdgeGeometry>(
                            *driver, std::move(edgeLut), ixBo,
                            static_cast<uint32_t>(ixVertCount), lutView,
                            static_cast<float>(edges->weight),
                            LineCode::valueFromLinePixels(edges->linePixels), colorInfo,
                            prim.isPlanar);
                        g->setPrimitive(ixPrim);
                        g->setVbhResources(ixVbh, ixVbih);
                        meshGraphic->addIndexedEdge(std::move(g));
                    }
                }
            }
        }

        graphics.push_back(meshGraphic);
    }

    return graphics;
}

END_DQ_RENDER_NAMESPACE
