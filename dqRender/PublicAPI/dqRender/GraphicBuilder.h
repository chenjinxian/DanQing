// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GraphicBuilder (builder pattern for creating graphics)
//
// Ported from: itwinjs-core core/frontend/src/render/GraphicBuilder.ts
// Builder pattern for constructing RenderGraphics.
#pragma once

#include "Export.h"
#include "GraphicBranch.h"
#include "GraphicPrimitive.h"
#include "RenderGraphic.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/GraphicParams.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif
#include <dqCommon/Frustum.h>
#include <dqCommon/LinePixels.h>
#include <dqCommon/Npc.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/CurveCollection.h>
#include <dqGeom/CurvePrimitive.h>
#include <dqGeom/Path.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Polyface.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/SolidPrimitive.h>
#include <dqGeom/Transform.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// Type of graphic being built.
// Ported from: itwinjs-core GraphicType
enum class GraphicType : uint8_t {
    ViewBackground = 0,   // ← GraphicType.ViewBackground: drawn first, view units, no z-buffer
    Scene = 1,            // ← GraphicType.Scene: drawn with z-buffer and scene lighting
    WorldDecoration = 2,  // ← GraphicType.WorldDecoration: drawn with z-buffer, default lighting
    WorldOverlay = 3,     // ← GraphicType.WorldOverlay: overlay mode, world units
    ViewOverlay = 4,      // ← GraphicType.ViewOverlay: overlay mode, view units
};

// Options for creating a GraphicBuilder.
// Ported from: itwinjs-core GraphicBuilderOptions
struct GraphicBuilderOptions {
    GraphicType type = GraphicType::Scene;
    dqGeom::Transform placement = dqGeom::Transform::CreateIdentity();
    // 1:1 GraphicBuilderOptions.{preserveOrder,wantNormals,generateEdges}? — optional so the ctor can
    // apply the reference's type-based defaults via `??` when left unset (GraphicBuilder.ts:150-152).
    std::optional<bool> preserveOrder;
    std::optional<bool> wantNormals;
    std::optional<bool> generateEdges;
    // 1:1 CustomGraphicBuilderOptions.computeChordTolerance (GraphicBuilder.ts:108-116). A viewport-based
    // factory computes it from the viewport's pixel size; a custom factory supplies it directly. DanQing has
    // no viewport in GraphicBuilderOptions, so the closure is supplied by the caller (system factory).
    // Required to produce LOD-correct tessellation in finish().
    std::function<double()> computeChordTolerance;
};

// Abstract base class for building RenderGraphics.
// Ported from: itwinjs-core GraphicBuilder
class DQ_RENDER_EXPORT GraphicBuilder {
public:
    virtual ~GraphicBuilder() = default;

    // Get the placement transform.
    const dqGeom::Transform& getPlacement() const noexcept { return m_placement; }

    // Get the graphic type.
    GraphicType getType() const noexcept { return m_type; }

    // --- Read-only builder state (1:1 GraphicAssembler.ts properties, computed once in the ctor) ---
    // If true, normal vectors are generated for surfaces (1:1 wantNormals).
    bool wantNormals() const noexcept { return m_wantNormals; }
    // If true, edges are generated for surfaces (1:1 wantEdges).
    bool wantEdges() const noexcept { return m_wantEdges; }
    // If true, geometry draw order is preserved (1:1 preserveOrder).
    bool preserveOrder() const noexcept { return m_preserveOrder; }

    // --- GraphicType predicates (1:1 GraphicAssembler.ts:102-124) ---
    // Geometry defined in CoordSystem.View coordinates (ViewBackground | ViewOverlay).
    bool isViewCoordinates() const noexcept {
        return m_type == GraphicType::ViewBackground || m_type == GraphicType::ViewOverlay;
    }
    // Geometry defined in CoordSystem.World coordinates — inverse of isViewCoordinates.
    bool isWorldCoordinates() const noexcept { return !isViewCoordinates(); }
    // True if the builder produces a GraphicType.Scene graphic.
    bool isSceneGraphic() const noexcept { return m_type == GraphicType::Scene; }
    // True if the builder produces a GraphicType.ViewBackground graphic.
    bool isViewBackground() const noexcept { return m_type == GraphicType::ViewBackground; }
    // True if the builder produces a GraphicType.ViewOverlay or WorldOverlay graphic.
    bool isOverlay() const noexcept {
        return m_type == GraphicType::ViewOverlay || m_type == GraphicType::WorldOverlay;
    }

    // Set the active symbology for subsequently-added geometry (1:1 GraphicAssembler.activateGraphicParams:130-132).
    void activateGraphicParams(const dqCommon::GraphicParams& graphicParams) {
        m_graphicParams = graphicParams.clone();
    }

    // Set the active symbology to a blanking fill before adding planar regions
    // (1:1 GraphicAssembler.setBlankingFill:445).
    void setBlankingFill(const dqCommon::ColorDef& fillColor) {
        activateGraphicParams(dqCommon::GraphicParams::fromBlankingFill(fillColor));
    }

    // Set current symbology — concrete; delegates to activateGraphicParams
    // (1:1 GraphicAssembler.setSymbology:435-437).
    void setSymbology(const dqCommon::ColorDef& lineColor, const dqCommon::ColorDef& fillColor,
                      uint32_t weight, dqCommon::LinePixels linePixels = dqCommon::LinePixels::Solid) {
        activateGraphicParams(dqCommon::GraphicParams::fromSymbology(
            lineColor, fillColor, static_cast<int>(weight), linePixels));
    }

    // add a point string.
    virtual void addPointString(const dqGeom::Point3d* points, size_t count) = 0;

    // add a line string.
    virtual void addLineString(const dqGeom::Point3d* points, size_t count) = 0;

    // add a shape (closed line string).
    virtual void addShape(const dqGeom::Point3d* points, size_t count) = 0;

    // add a 3d open path (chain of curves, stroked). Ported from: GraphicAssembler.addPath
    virtual void addPath(const dqGeom::Path& path) = 0;

    // add a 3d closed planar region (filled). Ported from: GraphicAssembler.addLoop
    virtual void addLoop(const dqGeom::Loop& loop) = 0;

    // add a mesh (polyface). Ported from: GraphicAssembler.addPolyface (filled unused per reference).
    virtual void addPolyface(const dqGeom::Polyface& meshData, bool filled = false) = 0;

    // add a solid primitive (tessellated to a mesh). Ported from: GraphicAssembler.addSolidPrimitive.
    virtual void addSolidPrimitive(const dqGeom::SolidPrimitive& primitive) = 0;

    // Append any graphic primitive, dispatching to the matching addXXX by variant type.
    // Ported from: GraphicAssembler.addPrimitive (GraphicAssembler.ts:302-341). Non-virtual: concrete dispatcher.
    void addPrimitive(const GraphicPrimitive& primitive);

    // --- 2d variants: lift 2d points onto the z=zDepth plane, then delegate to the 3d virtuals.
    // Ported from: itwinjs-core core/frontend/src/common/render/GraphicAssembler.ts
    //              addLineString2d / addPointString2d / addShape2d (virtual; default body calls add* via copy2dTo3d).
    //   Virtual (matching the reference's overridable methods) so subclasses such as the addPrimitive
    //   dispatch test can observe the 2d points + zDepth before the lift (GraphicPrimitive.test.ts). ---
    virtual void addLineString2d(const dqGeom::Point2d* points, size_t count, double zDepth) {
        const auto pts3d = copy2dTo3d(points, count, zDepth);
        addLineString(pts3d.data(), pts3d.size());
    }
    virtual void addPointString2d(const dqGeom::Point2d* points, size_t count, double zDepth) {
        const auto pts3d = copy2dTo3d(points, count, zDepth);
        addPointString(pts3d.data(), pts3d.size());
    }
    virtual void addShape2d(const dqGeom::Point2d* points, size_t count, double zDepth) {
        const auto pts3d = copy2dTo3d(points, count, zDepth);
        addShape(pts3d.data(), pts3d.size());
    }

    // --- Curve / arc / range methods (non-virtual; defined in GraphicBuilder.cpp).
    // Ported from: itwinjs-core core/frontend/src/common/render/GraphicAssembler.ts ---
    // Append a 3d open arc or closed ellipse. (GraphicAssembler.addArc) — virtual (reference overridable).
    virtual void addArc(const dqGeom::Arc3d& ellipse, bool isEllipse, bool filled);
    // Append a 2d open arc / closed ellipse at z=zDepth. (GraphicAssembler.addArc2d) — virtual.
    virtual void addArc2d(const dqGeom::Arc3d& ellipse, bool isEllipse, bool filled, double zDepth);
    // Append any CurvePrimitive (dispatch by curve type). (GraphicAssembler.addCurvePrimitive)
    void addCurvePrimitive(const dqGeom::CurvePrimitive& curve);
    // Add a box representing a volume of space (solid branch needs SolidPrimitive — Phase 3 TODO).
    // (GraphicAssembler.addRangeBox)
    void addRangeBox(const dqGeom::Range3d& range, bool solid = false);
    // Add Frustum edges (debug). (GraphicAssembler.addFrustum)
    void addFrustum(const dqCommon::Frustum& frustum);
    // Add Frustum sides (debug). (GraphicAssembler.addFrustumSides)
    void addFrustumSides(const dqCommon::Frustum& frustum);
    // Add range edges from 8 corner points (Npc order). (GraphicAssembler.addRangeBoxFromCorners)
    void addRangeBoxFromCorners(const dqGeom::Point3d* corners);
    // Add range sides from 8 corner points (Npc order). (GraphicAssembler.addRangeBoxSidesFromCorners)
    void addRangeBoxSidesFromCorners(const dqGeom::Point3d* corners);

    // finish building and return the graphic.
    virtual RenderGraphic* finish() = 0;

protected:
    // Ported from: itwinjs-core GraphicBuilder.ts:147-152 (constructor wantEdges/wantNormals/preserveOrder
    // resolution). DanQing has no viewport in GraphicBuilderOptions, so the `viewFlags.edgesRequired()`
    // branch collapses to its default (true) → wantEdges default = isSceneGraphic(); and there is no
    // `computeChordTolerance`/`iModel` wiring here (those live on the viewport-based factory path).
    explicit GraphicBuilder(const GraphicBuilderOptions& options)
        : m_type(options.type),
          m_placement(options.placement),
          // wantEdges = options.generateEdges ?? (type === Scene && (!vp || vp.viewFlags.edgesRequired()))
          m_wantEdges(options.generateEdges.value_or(m_type == GraphicType::Scene)),
          // wantNormals = options.wantNormals ?? (wantEdges || type === Scene)
          m_wantNormals(options.wantNormals.value_or(m_wantEdges || m_type == GraphicType::Scene)),
          // preserveOrder = options.preserveOrder ?? (ViewOverlay | WorldOverlay | ViewBackground)
          m_preserveOrder(options.preserveOrder.value_or(
              m_type == GraphicType::ViewOverlay || m_type == GraphicType::WorldOverlay ||
              m_type == GraphicType::ViewBackground))
    {
    }

    // Current active symbology (1:1 GraphicAssembler._graphicParams). Protected so PrimitiveBuilder can
    // read it in its addXXX; the reference accesses _graphicParams via getLinearDisplayParams/
    // getMeshDisplayParams → DisplayParams (not yet ported), so this raw accessor is the DanQing adaptation.
    const dqCommon::GraphicParams& graphicParams() const noexcept { return m_graphicParams; }

private:
    // Ported from: itwinjs-core GraphicAssembler.copy2dTo3d (private helper)
    // Lift a 2d point array to 3d by setting each point's z to zDepth.
    static std::vector<dqGeom::Point3d> copy2dTo3d(const dqGeom::Point2d* points, size_t count, double zDepth) {
        std::vector<dqGeom::Point3d> pts3d;
        pts3d.reserve(count);
        for (size_t i = 0; i < count; ++i)
            pts3d.push_back(dqGeom::Point3d::From(points[i].x, points[i].y, zDepth));
        return pts3d;
    }

    GraphicType m_type;
    dqGeom::Transform m_placement;
    bool m_wantEdges = false;     // 1:1 GraphicAssembler.wantEdges
    bool m_wantNormals = false;   // 1:1 GraphicAssembler.wantNormals
    bool m_preserveOrder = false; // 1:1 GraphicAssembler.preserveOrder
    dqCommon::GraphicParams m_graphicParams; // 1:1 GraphicAssembler._graphicParams (current symbology)
};

END_DQ_RENDER_NAMESPACE
