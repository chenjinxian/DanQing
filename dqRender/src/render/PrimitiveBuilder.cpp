// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PrimitiveBuilder implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/PrimitiveBuilder.ts
//              core/frontend/src/common/render/GraphicAssembler.ts (addXXX wiring L159-297)
//
// Each addXXX accumulates a Geometry record (placement + range + DisplayParams + currentFeature) into
// m_accumulator (1:1 GraphicAssembler[_accumulator]). finish() tessellates via chord tolerance into a
// MeshList; PR F routes each Mesh through RenderSystem::createMeshGraphics into a GraphicBranch.
#include "PrimitiveBuilder.h"

#include "AuxChannelLut.h"
#include "AuxChannelTable.h"
#include "DisplayParams.h"
#include "Graphic.h"
#include "MeshGraphic.h"
#include "SurfaceGeometry.h"
#include "VertexTableBuilder.h"

#include "dqRender/RenderSystem.h"

#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Polyface.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/Transform.h>

#include <vector>
#include <cstdio>
#include <cstdlib>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// M-W W-a6：auxData 网格的 LUT 表面生产（PrimitiveBuilder::finish 的 auxChannels 分支
// ——参考 PrimitiveBuilder.ts:115 的 createMeshGraphics → createTriMesh →
// createMeshParams（VertexTableBuilder.ts:78-102）→ MeshData.create（MeshData.ts:97-100）
// → MeshGeometry 链的 DanQing 落形：Mesh 顶点表打包（buildFromMesh）+ auxChannels
// 打包（fromChannels）+ SurfaceGeometry LUT 形态 + AuxChannelLUT）。
// 参考的 a_qPosition=24-bit 索引流承载三角索引（SurfaceGeometry.ts:378-387 +
// drawArrays :150-162——无 element buffer）。
std::unique_ptr<MeshGraphic> createLutMeshGraphic(rhi::Driver& driver, Mesh const& mesh)
{
    auto const* tris = mesh.triangles();
    if (tris == nullptr || tris->indices().empty() || mesh.points().length() == 0)
        return nullptr;

    // 顶点表（Unquantized.MeshBuilder/LitMeshBuilder——VertexTableBuilder.ts）。
    auto const& pts = mesh.points().points;
    auto const& normals = mesh.normals();
    uint32_t const numVertices = static_cast<uint32_t>(pts.size());
    // OctEncodedNormal → u16 值流（dqCommon OctEncodedNormal::value——16 位参考格式）。
    std::vector<uint16_t> octNormals;
    uint16_t const* octPtr = nullptr;
    if (!normals.empty() && normals.size() >= pts.size()) {
        octNormals.reserve(normals.size());
        for (auto const& n : normals)
            octNormals.push_back(static_cast<uint16_t>(n.value));
        octPtr = octNormals.data();
    }
    auto table = VertexTableBuilder::buildFromMesh(pts.data(), numVertices, mesh.points().range,
                                                   octPtr,
                                                   static_cast<uint32_t>(octNormals.size()));

    // LUT 纹理（线上字节原样上传——VertexLUT.ts:93-99 的 createFromVertexTable 等价面；
    // 纹理维度=computeDimensions 产出，VertexLutTexture::create 契约面）。
    VertexLutTexture lut;
    if (!lut.create(driver, table.data.data(), table.width, table.height, table.numVertices,
                    table.numRgbaPerVertex))
        return nullptr;
    lut.setQuantization(table.qOrigin[0], table.qOrigin[1], table.qOrigin[2],
                        table.qScale[0], table.qScale[1], table.qScale[2]);

    // a_qPosition 24-bit 索引流（三角角点索引 → UBYTE3，ImdlGraphics.cpp:528-558
    // 同型面）。
    auto const& triIndices = tris->indices();
    std::vector<uint8_t> idxBytes(static_cast<size_t>(triIndices.size()) * 3u);
    for (size_t i = 0; i < triIndices.size(); ++i) {
        uint32_t const idx = triIndices[i];
        idxBytes[i * 3 + 0] = static_cast<uint8_t>(idx & 0xFFu);
        idxBytes[i * 3 + 1] = static_cast<uint8_t>((idx >> 8) & 0xFFu);
        idxBytes[i * 3 + 2] = static_cast<uint8_t>((idx >> 16) & 0xFFu);
    }
    auto const idxBo = driver.createBufferObject(static_cast<uint32_t>(idxBytes.size()),
                                                 rhi::BufferObjectBinding::VERTEX,
                                                 rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor idxDesc(idxBytes.data(), idxBytes.size());
    driver.updateBufferObject(idxBo, std::move(idxDesc), 0);
    uint32_t const numIndices = static_cast<uint32_t>(triIndices.size());

    // a_qPosition VAO（UBYTE3@0——SurfaceVariantCompiler 量化变体 attribute map；
    // ImdlGraphics.cpp:546-558 同型）。
    rhi::AttributeArray attrs = {};
    attrs[0].buffer = 0;
    attrs[0].offset = 0;
    attrs[0].type = rhi::ElementType::UBYTE3;
    auto const lutVbih = driver.createVertexBufferInfo(1, 1, attrs);
    auto const lutVbh = driver.createVertexBuffer(numIndices, lutVbih);
    driver.setVertexBufferObject(lutVbh, 0, idxBo);
    auto const primitive = driver.createRenderPrimitive(lutVbh, rhi::IndexBufferHandle{},
                                                        rhi::PrimitiveType::TRIANGLES);

    // SurfaceType：Lit 位由法线在场性驱动（VertexTableBuilder.ts:541 的 isLit 判据——
    // SurfaceType.Lit↔DanQing SurfaceType::Opaque[光照面]/Unlit 未在场 →
    // 既有两形态面：Opaque=Lit 等价）；无纹理 → 非 Textured 族。
    bool const isLit = table.isLit;
    auto const surfaceType = isLit ? SurfaceType::Opaque : SurfaceType::Unknown;

    auto geom = std::make_unique<SurfaceGeometry>(driver, std::move(lut), idxBo, numIndices,
                                                  surfaceType, mesh.isPlanar(),
                                                  /*hasTextures*/ false, lutVbh, lutVbih);
    geom->setPrimitive(primitive);
    // 非量化 LUT 表（numRgba=5）——TD-27 变体轴（usesQuantizedPositions=false +
    // usesVertexLut=true → TechniqueFlags.isLutUnquantized）。
    geom->setUsesUnquantizedLut();

    // auxChannels → AuxChannelTable → AuxChannelLUT（AuxChannelTable.ts:186-371 +
    // VertexLUT.ts:21-110；createMeshParams 的 :93 同参直调面）。
    if (mesh.auxChannels().has_value()) {
        if (auto auxTable = AuxChannelTable::fromChannels(*mesh.auxChannels(), numVertices, 2048)) {
            if (auto auxLut = AuxChannelLUT::create(driver, *auxTable))
                geom->setAuxChannels(std::move(auxLut));
        }
    }

    // TEMP-DIAG（M-W W-a6 零内容取证，env 门控 DANQING_AS_TRACE）：LUT 产出面。
    if (std::getenv("DANQING_AS_TRACE")) {
        std::fprintf(stderr,
                     "[AS] createLutMeshGraphic: numVerts=%u numIdx=%u table=%ux%u rgba=%u anim=%d auxTex=%d\n",
                     numVertices, numIndices, table.width, table.height,
                     table.numRgbaPerVertex,
                     geom->hasAnimation() ? 1 : 0,
                     geom->auxChannels() ? (geom->auxChannels()->texture() != rhi::TextureHandle{} ? 1 : 0) : -1);
        if (numVertices > 0) {
            auto const& p0 = pts[0];
            std::fprintf(stderr, "[AS] mesh pt0=(%.6f %.6f %.6f) range z=[%.6f %.6f]\n",
                         p0.x, p0.y, p0.z, mesh.points().range.low.z, mesh.points().range.high.z);
            // 顶点表首顶点 f32 回译（转置布局对拍——CPU 写入 vs GPU 回读）。
            auto rdF32 = [&table](uint32_t texelByte) {
                uint32_t u = static_cast<uint32_t>(table.data[texelByte])
                    | (static_cast<uint32_t>(table.data[texelByte + 1]) << 8)
                    | (static_cast<uint32_t>(table.data[texelByte + 2]) << 16)
                    | (static_cast<uint32_t>(table.data[texelByte + 3]) << 24);
                float f;
                std::memcpy(&f, &u, 4);
                return f;
            };
            // 转置：x = data[0],data[4],data[8],data[12]；y = data[1],data[5],...
            uint32_t ux = static_cast<uint32_t>(table.data[0])
                | (static_cast<uint32_t>(table.data[4]) << 8)
                | (static_cast<uint32_t>(table.data[8]) << 16)
                | (static_cast<uint32_t>(table.data[12]) << 24);
            uint32_t uy = static_cast<uint32_t>(table.data[1])
                | (static_cast<uint32_t>(table.data[5]) << 8)
                | (static_cast<uint32_t>(table.data[9]) << 16)
                | (static_cast<uint32_t>(table.data[13]) << 24);
            uint32_t uz = static_cast<uint32_t>(table.data[2])
                | (static_cast<uint32_t>(table.data[6]) << 8)
                | (static_cast<uint32_t>(table.data[10]) << 16)
                | (static_cast<uint32_t>(table.data[14]) << 24);
            float fx, fy, fz;
            std::memcpy(&fx, &ux, 4);
            std::memcpy(&fy, &uy, 4);
            std::memcpy(&fz, &uz, 4);
            std::fprintf(stderr, "[AS] table v0=(%.6f %.6f %.6f)\n", fx, fy, fz);
            (void)rdF32;
        }
    }

    auto graphic = std::make_unique<MeshGraphic>(driver);
    graphic->addSurface(std::move(geom));
    return graphic;
}

}  // namespace

// ---------------------------------------------------------------------------
// PrimitiveBuilder
// ---------------------------------------------------------------------------

// 1:1 PrimitiveBuilder constructor (PrimitiveBuilder.ts:31-36) + GraphicAssembler ctor (L80-96) which
// builds the GeometryAccumulator. DanQing has no viewport, so the accumulator options carry
// only the initial feature (pickable path) — pickable lands with the activateFeature wiring.
// M-W W-a4：analysisStyleDisplacement 转接归位（GraphicAssembler.ts:90——accumulator 的
// analysisStyleDisplacement = options.analysisStyle?.displacement）。
PrimitiveBuilder::PrimitiveBuilder(RenderSystem& system, rhi::Driver& driver, GraphicBuilderOptions const& options)
    : GraphicBuilder(options)
    , m_system(system)
    , m_driver(driver)
    , m_computeChordTolerance(options.computeChordTolerance)
{
    if (options.analysisStyle.has_value() && options.analysisStyle->displacement.has_value())
        m_accumulator = GeometryAccumulator(GeometryAccumulatorOptions{
            .analysisStyleDisplacement = options.analysisStyle->displacement,
        });
}

// 1:1 GraphicAssembler.addLineString (GraphicAssembler.ts:159-164).
void PrimitiveBuilder::addLineString(const dqGeom::Point3d* points, size_t count)
{
    if (!points || count == 0)
        return;
    const std::vector<dqGeom::Point3d> pts(points, points + count);
    // 1:1: a 2-point line whose endpoints coincide degenerates to a point string.
    if (2 == count && pts[0].AlmostEqual(pts[1]))
        m_accumulator.addPointString(pts, getLinearDisplayParams(), getPlacement());
    else
        m_accumulator.addLineString(pts, getLinearDisplayParams(), getPlacement());
}

// 1:1 GraphicAssembler.addPointString (GraphicAssembler.ts:180-182).
void PrimitiveBuilder::addPointString(const dqGeom::Point3d* points, size_t count)
{
    if (!points || count == 0)
        return;
    const std::vector<dqGeom::Point3d> pts(points, points + count);
    m_accumulator.addPointString(pts, getLinearDisplayParams(), getPlacement());
}

// 1:1 GraphicAssembler.addShape (GraphicAssembler.ts:198-201): wrap points as a Loop (filled region).
void PrimitiveBuilder::addShape(const dqGeom::Point3d* points, size_t count)
{
    if (!points || count < 3)
        return;
    const std::vector<dqGeom::Point3d> pts(points, points + count);
    // 1:1: Loop.create(LineString3d.create(points)); DanQing uses the polygon-loop factory (equivalent).
    auto loop = dqGeom::Loop::CreatePolygon(pts);
    m_accumulator.addLoop(loop, getMeshDisplayParams(), getPlacement(), false);
}

// 1:1 GraphicAssembler.addPath (GraphicAssembler.ts:256-258).
void PrimitiveBuilder::addPath(const dqGeom::Path& path)
{
    // Clone into an owning RefPtr (the borrowed const Path& cannot share ownership into the accumulator).
    auto pathPtr = path.clone().StaticCast<dqGeom::Path>();
    m_accumulator.addPath(pathPtr, getLinearDisplayParams(), getPlacement(), false);
}

// 1:1 GraphicAssembler.addLoop (GraphicAssembler.ts:261-263).
void PrimitiveBuilder::addLoop(const dqGeom::Loop& loop)
{
    auto loopPtr = loop.clone().StaticCast<dqGeom::Loop>();
    m_accumulator.addLoop(loopPtr, getMeshDisplayParams(), getPlacement(), false);
}

// 1:1 GraphicAssembler.addPolyface (GraphicAssembler.ts:290-292). `filled` unused per reference (_filled).
void PrimitiveBuilder::addPolyface(const dqGeom::Polyface& meshData, bool /*filled*/)
{
    // 1:1: `meshData as IndexedPolyface` — IndexedPolyface is the only concrete Polyface form.
    const auto& ipf = static_cast<const dqGeom::IndexedPolyface&>(meshData);
    auto pfPtr = ipf.clone().StaticCast<dqGeom::IndexedPolyface>();
    m_accumulator.addPolyface(pfPtr, getMeshDisplayParams(), getPlacement());
}

// 1:1 GraphicAssembler.addSolidPrimitive (GraphicAssembler.ts:295-297).
void PrimitiveBuilder::addSolidPrimitive(const dqGeom::SolidPrimitive& primitive)
{
    auto spPtr = primitive.clone().StaticCast<dqGeom::SolidPrimitive>();
    m_accumulator.addSolidPrimitive(spPtr, getMeshDisplayParams(), getPlacement());
}

// 1:1 PrimitiveBuilder.finish (PrimitiveBuilder.ts:38-42) → toTemplate(false) → saveToTemplate
// (PrimitiveBuilder.ts:133-204): compute tolerance, accum.toMeshes, per-mesh createMeshGraphics → branch.
RenderGraphic* PrimitiveBuilder::finish()
{
    if (!m_computeChordTolerance)
        return nullptr;  // no LOD closure — nothing to produce.

    const double tolerance = m_computeChordTolerance();
    GeometryOptions opts;
    opts.wantEdges = wantEdges();
    opts.preserveOrder = preserveOrder();

    MeshList meshes = m_accumulator.toMeshes(opts, tolerance, /*pickable=*/std::nullopt);
    m_accumulator.clear();
    // TEMP-DIAG（ACS 盘填充深缩放死亡排查）：per-mesh 类型/图元数/range——
    // 死亡步（ext≈0.012）对照：三角形在 mesh 层就没了 = MeshBuilder 链杀；
    // 三角形在但不上屏 = 渲染命令/合成层杀（env DANQING_GL_TRACE=1）。
    if (std::getenv("DANQING_GL_TRACE")) {
        for (auto& mesh : meshes) {
            size_t tris = 0, polys = 0, ptsN = mesh->points().length();
            if (mesh->triangles()) tris = mesh->triangles()->indices().size() / 3;
            if (mesh->polylines()) polys = mesh->polylines()->size();
            auto const& lo = mesh->points().range.low;
            auto const& hi = mesh->points().range.high;
            std::fprintf(stderr,
                         "[MESHDIAG] type=%d tol=%.3g pts=%zu tris=%zu polys=%zu rangeSpan=(%.6g,%.6g,%.6g)\n",
                         static_cast<int>(mesh->type()), tolerance, ptsN, tris, polys,
                         hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
        }
    }
    if (meshes.isEmpty())
        return nullptr;

    // 1:1 PrimitiveBuilder.ts:83-128: each mesh → createMeshGraphics; collect in a GraphicsArray. The
    // Mesh points are recentered about the shared range center, so wrap the array in a Branch carrying
    // the inverse translation (Transform.createTranslation(transformOrigin)) to recover world placement.
    auto array = std::make_unique<GraphicsArray>();
    dqGeom::Point3d transformOrigin = dqGeom::Point3d::FromZero();
    bool haveOrigin = false;
    for (auto& mesh : meshes) {
        if (!haveOrigin) {
            transformOrigin = mesh->points().range.Center();
            haveOrigin = true;
        }
        // M-W W-a6：auxChannels 在场的网格 → LUT 表面路径（参考 PrimitiveBuilder
        // createMeshGraphics 的 createTriMesh→MeshGeometry LUT 链——分析位移/scalar
        // 动画需 LUT 形态 + auxChannels 纹理；无 auxChannels → 既有 MeshRenderGeometry
        // VBO 路径不变）。
        if (mesh->auxChannels().has_value()) {
            if (auto lg = createLutMeshGraphic(m_driver, *mesh)) {
                array->add(std::move(lg));
                continue;
            }
        }
        auto mg = MeshRenderGeometry::create(m_driver, *mesh);
        if (mg)
            array->add(std::move(mg));  // GraphicsArray owns the MeshGraphic (a Graphic).
    }
    // TEMP-DIAG（M-W W-a6 零内容取证，env 门控）：finish 产出面——
    // 每 mesh 的路由 + 产出。
    if (std::getenv("DANQING_AS_TRACE")) {
        std::fprintf(stderr, "[AS] finish: meshes=%zu arraySize=%zu\n",
                     static_cast<size_t>(0), array->size());
    }

    if (array->size() == 0)
        return nullptr;

    auto branch = std::make_unique<Branch>();
    branch->setLocalToWorld(
        dqGeom::Transform::CreateTranslation(transformOrigin.x, transformOrigin.y, transformOrigin.z));
    branch->setChild(std::move(array));
    // Branch : Graphic : RenderGraphic — RenderGraphicAdapter::addCommands dispatches to Branch::addCommands
    // (defined), and the MeshGraphic leaves are Graphics, so the whole tree is UB-free.
    return branch.release();
}

// 1:1 GraphicAssembler.getMeshDisplayParams (GraphicAssembler.ts:447).
DisplayParams PrimitiveBuilder::getMeshDisplayParams() const
{
    // resolveGradient returns nullptr in D′ (gradient/texture materialization deferred).
    return DisplayParams::createForMesh(graphicParams(), !wantNormals(), nullptr);
}

// 1:1 GraphicAssembler.getLinearDisplayParams (GraphicAssembler.ts:448).
DisplayParams PrimitiveBuilder::getLinearDisplayParams() const
{
    return DisplayParams::createForLinear(graphicParams());
}

END_DQ_RENDER_NAMESPACE
