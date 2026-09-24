// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PrimitiveBuilder implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/PrimitiveBuilder.ts
//              core/frontend/src/common/render/GraphicAssembler.ts (addXXX wiring L159-297)
//
// Each addXXX accumulates a Geometry record (placement + range + DisplayParams + currentFeature) into
// m_accumulator (1:1 GraphicAssembler[_accumulator]). finish() tessellates via chord tolerance into a
// MeshList; PR F routes each Mesh through RenderSystem::createMeshGraphics into a GraphicBranch.
#include "PrimitiveBuilder.h"

#include "DisplayParams.h"
#include "Graphic.h"
#include "MeshGraphic.h"

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

// ---------------------------------------------------------------------------
// PrimitiveBuilder
// ---------------------------------------------------------------------------

// 1:1 PrimitiveBuilder constructor (PrimitiveBuilder.ts:31-36) + GraphicAssembler ctor (L80-96) which
// builds the GeometryAccumulator. DanQing has no viewport/analysisStyle, so the accumulator options carry
// only the initial feature (pickable path) — pickable lands with the activateFeature wiring.
PrimitiveBuilder::PrimitiveBuilder(RenderSystem& system, rhi::Driver& driver, GraphicBuilderOptions const& options)
    : GraphicBuilder(options)
    , m_system(system)
    , m_driver(driver)
    , m_computeChordTolerance(options.computeChordTolerance)
{
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
        auto mg = MeshRenderGeometry::create(m_driver, *mesh);
        if (mg)
            array->add(std::move(mg));  // GraphicsArray owns the MeshGraphic (a Graphic).
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
