// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Primitive builder (GraphicBuilder implementation)
// Ported from: itwinjs-core core/frontend/src/internal/render/PrimitiveBuilder.ts
//              core/frontend/src/common/render/GraphicAssembler.ts (addXXX wiring)
//
// Bridges between the geometry accumulator and the render system: each addXXX accumulates a Geometry
// record (placement + range + DisplayParams + Feature) into a GeometryAccumulator; finish() tessellates
// via chord tolerance into a MeshList (GeometryAccumulator.toMeshes → MeshBuilderMap → MeshBuilder → Mesh)
// then routes each Mesh through RenderSystem::createMeshGraphics (PR F) into a GraphicBranch.
//
// D′ rewires addXXX through the accumulator and DELETES the prior direct-emit path (MeshGraphicRender —
// the UB source: it inherited public RenderGraphic with no addCommands, then got static_cast<Graphic*>'d
// by RenderGraphicAdapter, so nothing drew). finish() returns nullptr until PR F wires createMeshGraphics.
#pragma once

#include "dqRender/GraphicBuilder.h"
#include "dqRender/RenderGraphic.h"
#include "dqRender/rhi/Driver.h"

#include "GeometryAccumulator.h"

#include <functional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class RenderSystem;

// ---------------------------------------------------------------------------
// PrimitiveBuilder — GraphicBuilder implementation
// (Ported from: itwinjs-core PrimitiveBuilder.ts + GraphicAssembler addXXX)
// ---------------------------------------------------------------------------
class PrimitiveBuilder : public GraphicBuilder {
public:
    PrimitiveBuilder(RenderSystem& system, rhi::Driver& driver, GraphicBuilderOptions const& options);
    ~PrimitiveBuilder() override = default;

    // --- GraphicBuilder interface ---
    // setSymbology/activateGraphicParams/setBlankingFill inherited (concrete on GraphicBuilder, 1:1
    // GraphicAssembler). addXXX read the base's current GraphicParams via get{Linear,Mesh}DisplayParams.
    void addPointString(const dqGeom::Point3d* points, size_t count) override;
    void addLineString(const dqGeom::Point3d* points, size_t count) override;
    void addShape(const dqGeom::Point3d* points, size_t count) override;
    // Ported from: GraphicAssembler.addPath — accumulate path as linear (strokes).
    void addPath(const dqGeom::Path& path) override;
    // Ported from: GraphicAssembler.addLoop — accumulate loop as mesh (filled region).
    void addLoop(const dqGeom::Loop& loop) override;
    // Ported from: GraphicAssembler.addPolyface — accumulate polyface as mesh.
    void addPolyface(const dqGeom::Polyface& meshData, bool filled = false) override;
    // Ported from: GraphicAssembler.addSolidPrimitive — accumulate solid as mesh.
    void addSolidPrimitive(const dqGeom::SolidPrimitive& primitive) override;
    RenderGraphic* finish() override;

    // The accumulator (1:1 GraphicAssembler[_accumulator]). Exposed so activateFeature /
    // activatePickableId can set accumulator.currentFeature (GraphicAssembler.ts:138-153) — those
    // methods land with the pickable path; the accessor is in place for them.
    GeometryAccumulator& accumulator() noexcept { return m_accumulator; }

    // 1:1 PrimitiveBuilder.system (PrimitiveBuilder.ts:26) — the RenderSystem that produced this builder.
    RenderSystem& system() noexcept { return m_system; }

private:
    // 1:1 GraphicAssembler.getMeshDisplayParams / getLinearDisplayParams (GraphicAssembler.ts:447-448).
    // resolveGradient (GraphicBuilder.ts:64-66 → system.getGradientTexture) returns nullptr in D′
    // (gradient/texture materialization deferred).
    DisplayParams getMeshDisplayParams() const;
    DisplayParams getLinearDisplayParams() const;

    RenderSystem& m_system;
    rhi::Driver& m_driver;  // PR F: GPU upload for create(driver, Mesh) in finish().
    GeometryAccumulator m_accumulator;
    std::function<double()> m_computeChordTolerance;
};

END_DQ_RENDER_NAMESPACE
