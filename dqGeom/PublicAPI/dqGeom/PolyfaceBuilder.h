// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceBuilder (mesh construction)
//
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceBuilder.ts
//
// Builds IndexedPolyface incrementally from triangles, quads, polygons,
// and curve strokes.  Uses StrokeOptions to control tessellation quality.
// 替代 Qt QVector，参数用 std::vector。
#pragma once

#include "IndexedPolyface.h"
#include "Segment1d.h"
#include "StrokeOptions.h"

#include "GeometryQuery.h"
#include "SolidPrimitive.h"

#include <memory>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

class CurvePrimitive;
class Box;
class Cone;
class Sphere;
class TorusPipe;
class LinearSweep;
class RotationalSweep;
class RuledSweep;
class UVSurface;

// ---------------------------------------------------------------------------
// PolyfaceBuilder — mesh builder
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT PolyfaceBuilder {
public:
    /// create a builder with the given options.
    static std::unique_ptr<PolyfaceBuilder> create(StrokeOptions const& options = {});

    /// Extract the finished polyface (optionally compress duplicate vertices).
    dqBase::RefPtr<IndexedPolyface> ClaimPolyface(bool compress = true);

    // --- Low-level facet methods ---
    // 参数序对齐参考（points, params, normals——PolyfaceBuilder.ts addTriangleFacet/addQuadFacet）。
    void AddTriangleFacet(Point3d const* points, Point2d const* params = nullptr, Vector3d const* normals = nullptr);
    void AddQuadFacet(Point3d const* points, Point2d const* params = nullptr, Vector3d const* normals = nullptr);
    void AddPolygon(std::vector<Point3d> const& points);
    void AddCurveStroke(CurvePrimitive const& curve);
    void AddCurveStroke(CurvePrimitive const& curve, StrokeOptions const& options);

    // --- UV 网格体（平滑法线 + 距离参数） ---
    // Ported from: PolyfaceBuilder.ts addUVGridBody (:1887-1984)。按 (numU+1)×(numV+1)
    // 网格采样 surface.UVFractionToPointAndTangents：点共享（每格点一次 AddPoint），
    // needNormals → 每顶点 vectorU×vectorV 单位法线，needParams → uMap/vMap 映射的
    // 距离参数（缺省为 0..1 分数）。shouldTriangulate 时拆双三角（参考同构）。
    void AddUVGridBody(UVSurface const& surface, int numU, int numV,
                       Segment1d const* uMap = nullptr, Segment1d const* vMap = nullptr);

    // Dispatch any GeometryQuery to the matching per-type emitter. Ported from:
    // PolyfaceBuilder.ts addGeometryQuery — solid primitives switch on solidPrimitiveType to the
    // per-type Add* method; non-solid + sweep types are Phase-N TODO (SweepContour tessellation).
    void AddGeometryQuery(GeometryQuery const& geom);

    // --- Per-solid-type emissions (port PolyfaceBuilder.ts addBox/addCone/addSphere). ---
    // Ported from: PolyfaceBuilder.ts addBox (1436). Emits 6 cap/side facets directly via AddQuadFacet
    // (UV-grid subdivision per applyMaxEdgeLength is a Phase-N refinement; default options → 1 facet/face).
    void AddBox(Box const& box);
    // Ported from: PolyfaceBuilder.ts addCone (1099). Ring-sweep N lateral quads + cap fans (UV-grid phase).
    void AddCone(Cone const& cone, int strokeCount = 0);
    // Ported from: PolyfaceBuilder.ts addSphere (1414). Latitude/longitude grid of quads (UV-grid phase).
    void AddSphere(Sphere const& sphere, int strokeCount = 0);
    // Ported from: PolyfaceBuilder.ts addTorusPipe (1128). Torus grid (minor φ × major θ) of quads (UV-grid phase).
    void AddTorusPipe(TorusPipe const& torusPipe, int numPhi = 0, int numTheta = 0);

    // --- Sweep tessellation (direct path; SweepContour is TS-only, not ported). ---
    // Ported from: PolyfaceBuilder.ts addLinearSweep (1363). Stroke cross-section → translate by sweep
    // vector → side quads between the two stroke sets → optional cap fans. Caps fan (convex-correct);
    // non-convex/hole regions need an ear-clip triangulator (Phase-N).
    void AddLinearSweep(LinearSweep const& sweep);
    // Ported from: PolyfaceBuilder.ts addRotationalSweep (1211). Stroke cross-section → rotate through
    // sweepAngle in N steps (N from ApplyTolerancesToArc) → side quads per step → optional cap fans.
    void AddRotationalSweep(RotationalSweep const& sweep);
    // Ported from: PolyfaceBuilder.ts addRuledSweep (1381). Stroke each contour → side quads between
    // consecutive profiles (require matching point counts — StrokeCountSection reconciliation is Phase-N).
    void AddRuledSweep(RuledSweep const& sweep);

    // --- Accessors ---
    StrokeOptions const& Options() const noexcept { return m_options; }
    IndexedPolyface const* Polyface() const noexcept { return m_polyface.Get(); }

private:
    PolyfaceBuilder() = default;

    StrokeOptions m_options;
    dqBase::RefPtr<IndexedPolyface> m_polyface;
};

END_DQ_GEOM_NAMESPACE
