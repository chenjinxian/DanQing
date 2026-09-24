// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceBuilder implementation
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceBuilder.ts
#include "dqGeom/PolyfaceBuilder.h"
#include "dqGeom/Angle.h"
#include "dqGeom/BilinearPatch.h"
#include "dqGeom/Box.h"
#include "dqGeom/Cone.h"
#include "dqGeom/CurveCollection.h"
#include "dqGeom/CurvePrimitive.h"
#include "dqGeom/Geometry.h"
#include "dqGeom/LineString3d.h"
#include "dqGeom/LinearSweep.h"
#include "dqGeom/Plane3dByOriginAndVectors.h"
#include "dqGeom/PolygonOps.h"
#include "dqGeom/RotationalSweep.h"
#include "dqGeom/RuledSweep.h"
#include "dqGeom/Sphere.h"
#include "dqGeom/TorusPipe.h"
#include "dqGeom/Transform.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

std::unique_ptr<PolyfaceBuilder> PolyfaceBuilder::create(StrokeOptions const& options)
{
    auto builder = std::unique_ptr<PolyfaceBuilder>(new PolyfaceBuilder());
    builder->m_options = options;
    builder->m_polyface = IndexedPolyface::create(options.needNormals, options.needColors,
                                                   options.needTwoSided, options.needParams);
    return builder;
}

// Ported from: PolyfaceBuilder.ts addGeometryQuery. Dispatch a GeometryQuery to its per-type emitter.
// Solids switch on solidPrimitiveType → AddBox/AddCone/AddSphere/AddTorusPipe; the 3 sweep types +
// non-solid geometry are Phase-N TODO (sweep tessellation needs SweepContour.emitFacets).
void PolyfaceBuilder::AddGeometryQuery(GeometryQuery const& geom)
{
    if (geom.Category() != GeometryCategory::Solid) {
        // TODO Phase-N: non-solid GeometryQuery tessellation.
        return;
    }
    auto const& solid = static_cast<SolidPrimitive const&>(geom);
    switch (solid.GetSolidPrimitiveType()) {
        case SolidPrimitiveType::Box:        AddBox(static_cast<Box const&>(solid)); break;
        case SolidPrimitiveType::Cone:       AddCone(static_cast<Cone const&>(solid)); break;
        case SolidPrimitiveType::Sphere:     AddSphere(static_cast<Sphere const&>(solid)); break;
        case SolidPrimitiveType::TorusPipe:  AddTorusPipe(static_cast<TorusPipe const&>(solid)); break;
        case SolidPrimitiveType::LinearSweep:
            AddLinearSweep(static_cast<LinearSweep const&>(solid)); break;
        case SolidPrimitiveType::RotationalSweep:
            AddRotationalSweep(static_cast<RotationalSweep const&>(solid)); break;
        case SolidPrimitiveType::RuledSweep:
            AddRuledSweep(static_cast<RuledSweep const&>(solid)); break;
    }
}

dqBase::RefPtr<IndexedPolyface> PolyfaceBuilder::ClaimPolyface(bool compress)
{
    if (compress && m_polyface) {
        m_polyface->Data().Compress();
    }
    auto result = m_polyface;
    m_polyface = nullptr;
    return result;
}

// ---------------------------------------------------------------------------
// File-static helpers（参考 PolyfaceBuilder 的 private static 工作变量族）
// ---------------------------------------------------------------------------
namespace {

// Ported from: PolyfaceBuilder.ts getUVTransformForTriangleFacet (:538-542) —
// uv→world 刚体框（原点在 pointA，X 沿 AB，Y 由 AC 正交化）；其逆把面上点映为 UV。
std::optional<Transform> GetUVTransformForTriangleFacet(Point3d const& pointA,
                                                        Point3d const& pointB,
                                                        Point3d const& pointC)
{
    Vector3d const vectorAB = Vector3d::FromStartEnd(pointA, pointB);
    Vector3d const vectorAC = Vector3d::FromStartEnd(pointA, pointC);
    auto matrix = Matrix3d::CreateRigidFromColumns(vectorAB, vectorAC, AxisOrder::XYZ);
    if (!matrix.has_value())
        return std::nullopt;
    return Transform::CreateOriginAndMatrix(pointA, *matrix);
}

// Ported from: Transform.ts multiplyInversePoint3dAsPoint2d (:520-527) —
// 逆变换后取 (x, y)。退化时回退 (0,0)（参考返回 undefined 由调用处可选性表达——
// 这里与参考 addTriangleFacet/addQuadFacet 的 `paramTransform?.` 短路保持一致：
// 只有拿到 transform 才会调用本函数）。
Point2d MultiplyInversePoint3dAsPoint2d(Transform const& transform, Point3d const& point)
{
    Point3d r;
    if (!transform.MultiplyInversePoint3d(point, r))
        return Point2d::From(0.0, 0.0);
    return Point2d::From(r.x, r.y);
}

// Ported from: PolyfaceBuilder.ts getNormalForTriangularFacet (:544-547) —
// (B-A)×(C-A) 单位化；退化时参考回退零向量。
Vector3d GetNormalForTriangularFacet(Point3d const& pointA, Point3d const& pointB, Point3d const& pointC)
{
    Vector3d normal = Vector3d::FromCrossProduct(
        Vector3d::FromStartEnd(pointA, pointB), Vector3d::FromStartEnd(pointA, pointC));
    if (normal.Normalize() == 0.0)
        return Vector3d::From(0.0, 0.0, 0.0);
    return normal;
}

}  // namespace

// Ported from: itwinjs PolyfaceBuilder.addTriangleFacet (:770-819)
// needParams：外部 params 优先，否则按 facet 平面局部框计算（距离参数）。
// needNormals：外部 normals 逐角 3 次 AddNormal；否则**单次** AddNormal、
// 三角共享同一索引（idx2=idx1=idx0——computeUVParams 的 planar 判定依赖
// normalIndex[0]==[1]==[2]，TextureMapping.ts:252）。
void PolyfaceBuilder::AddTriangleFacet(Point3d const* points, Point2d const* params, Vector3d const* normals)
{
    if (!m_polyface) return;

    if (m_options.needParams) {
        int32_t i0, i1, i2;
        if (params) {
            i0 = m_polyface->AddParam(params[0]);
            i1 = m_polyface->AddParam(params[1]);
            i2 = m_polyface->AddParam(params[2]);
        } else {
            auto t = GetUVTransformForTriangleFacet(points[0], points[1], points[2]);
            Point2d p0 = Point2d::From(0.0, 0.0), p1 = p0, p2 = p0;
            if (t.has_value()) {
                p0 = MultiplyInversePoint3dAsPoint2d(*t, points[0]);
                p1 = MultiplyInversePoint3dAsPoint2d(*t, points[1]);
                p2 = MultiplyInversePoint3dAsPoint2d(*t, points[2]);
            }
            i0 = m_polyface->AddParam(p0);
            i1 = m_polyface->AddParam(p1);
            i2 = m_polyface->AddParam(p2);
        }
        m_polyface->AddParamIndex(i0);
        m_polyface->AddParamIndex(i1);
        m_polyface->AddParamIndex(i2);
    }
    if (m_options.needNormals) {
        int32_t i0, i1, i2;
        if (normals) {
            i0 = m_polyface->AddNormal(normals[0]);
            i1 = m_polyface->AddNormal(normals[1]);
            i2 = m_polyface->AddNormal(normals[2]);
        } else {
            i2 = i1 = i0 = m_polyface->AddNormal(GetNormalForTriangularFacet(points[0], points[1], points[2]));
        }
        m_polyface->AddNormalIndex(i0);
        m_polyface->AddNormalIndex(i1);
        m_polyface->AddNormalIndex(i2);
    }
    int32_t const p0 = m_polyface->AddPoint(points[0]);
    int32_t const p1 = m_polyface->AddPoint(points[1]);
    int32_t const p2 = m_polyface->AddPoint(points[2]);
    m_polyface->AddPointIndex(p0, true);
    m_polyface->AddPointIndex(p1, true);
    m_polyface->AddPointIndex(p2, true);
    m_polyface->TerminateFacet();
}

// Ported from: itwinjs PolyfaceBuilder.addQuadFacet (:568-700)
// 参数变换跳过 points[2]（平行四边形常态使 param2=(1,1)——参考 :595 注释）。
// 计算法线时逐角 4 次 AddNormal（值同、索引异——参考如此，quad 面因此走
// computeUVParams 的 parametric 分支消费距离参数，TextureMapping.ts:252-254）。
void PolyfaceBuilder::AddQuadFacet(Point3d const* points, Point2d const* params, Vector3d const* normals)
{
    if (!m_polyface) return;

    if (m_options.shouldTriangulate) {
        // Split quad into 2 triangles along the shorter diagonal
        // （参考 :626-656：magSquaredAC + smallFloatingPoint >= magSquaredBD → 对角线 0-2）
        double const d02 = Vector3d::FromStartEnd(points[0], points[2]).MagnitudeSquared();
        double const d13 = Vector3d::FromStartEnd(points[1], points[3]).MagnitudeSquared();

        if (d02 + kSmallFloatingPoint >= d13) {
            Point3d const tri0[3] = {points[0], points[1], points[2]};
            Point3d const tri1[3] = {points[0], points[2], points[3]};
            Point2d const uv0[3] = {params ? params[0] : Point2d(), params ? params[1] : Point2d(), params ? params[2] : Point2d()};
            Point2d const uv1[3] = {params ? params[0] : Point2d(), params ? params[2] : Point2d(), params ? params[3] : Point2d()};
            Vector3d const n0[3] = {normals ? normals[0] : Vector3d(), normals ? normals[1] : Vector3d(), normals ? normals[2] : Vector3d()};
            Vector3d const n1[3] = {normals ? normals[0] : Vector3d(), normals ? normals[2] : Vector3d(), normals ? normals[3] : Vector3d()};
            AddTriangleFacet(tri0, params ? uv0 : nullptr, normals ? n0 : nullptr);
            AddTriangleFacet(tri1, params ? uv1 : nullptr, normals ? n1 : nullptr);
        } else {
            Point3d const tri0[3] = {points[0], points[1], points[3]};
            Point3d const tri1[3] = {points[1], points[2], points[3]};
            Point2d const uv0[3] = {params ? params[0] : Point2d(), params ? params[1] : Point2d(), params ? params[3] : Point2d()};
            Point2d const uv1[3] = {params ? params[1] : Point2d(), params ? params[2] : Point2d(), params ? params[3] : Point2d()};
            Vector3d const n0[3] = {normals ? normals[0] : Vector3d(), normals ? normals[1] : Vector3d(), normals ? normals[3] : Vector3d()};
            Vector3d const n1[3] = {normals ? normals[1] : Vector3d(), normals ? normals[2] : Vector3d(), normals ? normals[3] : Vector3d()};
            AddTriangleFacet(tri0, params ? uv0 : nullptr, normals ? n0 : nullptr);
            AddTriangleFacet(tri1, params ? uv1 : nullptr, normals ? n1 : nullptr);
        }
        return;
    }

    if (m_options.needParams) {
        int32_t i0, i1, i2, i3;
        if (params) {
            i0 = m_polyface->AddParam(params[0]);
            i1 = m_polyface->AddParam(params[1]);
            i2 = m_polyface->AddParam(params[2]);
            i3 = m_polyface->AddParam(params[3]);
        } else {
            // skip points[2] when computing transform so that the parallelogram
            // common case gets param2 = (1,1)（参考 :595-603）
            auto t = GetUVTransformForTriangleFacet(points[0], points[1], points[3]);
            Point2d p0 = Point2d::From(0.0, 0.0), p1 = p0, p2 = p0, p3 = p0;
            if (t.has_value()) {
                p0 = MultiplyInversePoint3dAsPoint2d(*t, points[0]);
                p1 = MultiplyInversePoint3dAsPoint2d(*t, points[1]);
                p2 = MultiplyInversePoint3dAsPoint2d(*t, points[2]);
                p3 = MultiplyInversePoint3dAsPoint2d(*t, points[3]);
            }
            i0 = m_polyface->AddParam(p0);
            i1 = m_polyface->AddParam(p1);
            i2 = m_polyface->AddParam(p2);
            i3 = m_polyface->AddParam(p3);
        }
        m_polyface->AddParamIndex(i0);
        m_polyface->AddParamIndex(i1);
        m_polyface->AddParamIndex(i2);
        m_polyface->AddParamIndex(i3);
    }
    if (m_options.needNormals) {
        int32_t i0, i1, i2, i3;
        if (normals) {
            i0 = m_polyface->AddNormal(normals[0]);
            i1 = m_polyface->AddNormal(normals[1]);
            i2 = m_polyface->AddNormal(normals[2]);
            i3 = m_polyface->AddNormal(normals[3]);
        } else {
            Vector3d const n = GetNormalForTriangularFacet(points[0], points[1], points[2]);
            i0 = m_polyface->AddNormal(n);
            i1 = m_polyface->AddNormal(n);
            i2 = m_polyface->AddNormal(n);
            i3 = m_polyface->AddNormal(n);
        }
        m_polyface->AddNormalIndex(i0);
        m_polyface->AddNormalIndex(i1);
        m_polyface->AddNormalIndex(i2);
        m_polyface->AddNormalIndex(i3);
    }
    int32_t const p0 = m_polyface->AddPoint(points[0]);
    int32_t const p1 = m_polyface->AddPoint(points[1]);
    int32_t const p2 = m_polyface->AddPoint(points[2]);
    int32_t const p3 = m_polyface->AddPoint(points[3]);
    m_polyface->AddPointIndex(p0, true);
    m_polyface->AddPointIndex(p1, true);
    m_polyface->AddPointIndex(p2, true);
    m_polyface->AddPointIndex(p3, true);
    m_polyface->TerminateFacet();
}

// Ported from: itwinjs PolyfaceBuilder.addPolygon — DanQing 扩展：needNormals 时
// 写单一 facet 法线（全角共享索引）；needParams 时按首三点局部框写距离参数
// （planar 纹理路径会以法线索引判平面并覆盖为投影 UV——TextureMapping.ts:246-257）。
void PolyfaceBuilder::AddPolygon(std::vector<Point3d> const& points)
{
    if (!m_polyface || points.size() < 3) return;

    if (m_options.shouldTriangulate && points.size() > 3) {
        // Fan triangulation
        for (size_t i = 1; i + 1 < points.size(); ++i) {
            Point3d const tri[3] = {points[0], points[i], points[i + 1]};
            AddTriangleFacet(tri);
        }
        return;
    }

    int32_t normalIndex = -1;
    if (m_options.needNormals)
        normalIndex = m_polyface->AddNormal(GetNormalForTriangularFacet(points[0], points[1], points[2]));
    bool const writeParams = m_options.needParams;
    std::optional<Transform> paramTransform;
    if (writeParams)
        paramTransform = GetUVTransformForTriangleFacet(points[0], points[1], points[2]);

    for (auto const& p : points) {
        if (writeParams) {
            Point2d const uv = paramTransform.has_value()
                ? MultiplyInversePoint3dAsPoint2d(*paramTransform, p)
                : Point2d::From(0.0, 0.0);
            m_polyface->AddParamIndex(m_polyface->AddParam(uv));
        }
        if (normalIndex > 0)
            m_polyface->AddNormalIndex(normalIndex);
        int32_t const idx = m_polyface->AddPoint(p);
        m_polyface->AddPointIndex(idx, true);
    }
    m_polyface->TerminateFacet();
}

void PolyfaceBuilder::AddCurveStroke(CurvePrimitive const& curve)
{
    AddCurveStroke(curve, m_options);
}

// Ported from: itwinjs CurvePrimitive.emitStrokes → PolyfaceBuilder
void PolyfaceBuilder::AddCurveStroke(CurvePrimitive const& curve, StrokeOptions const& options)
{
    if (!m_polyface) return;

    LineString3d linestring;
    curve.EmitStrokes(linestring, options);

    // Add the polyline points as a single face (line strip)
    for (size_t i = 0; i < linestring.PointCount(); ++i) {
        int32_t idx = m_polyface->AddPoint(linestring.Points()[static_cast<int>(i)]);
        m_polyface->AddPointIndex(idx, true);
    }
    m_polyface->TerminateFacet();
}

// Ported from: itwinjs-core PolyfaceBuilder.addUVGridBody (:1887-1984) — UV 网格体：
// (numU+1)×(numV+1) 采样 UVSurface.UVFractionToPointAndTangents，点每格点一次
// AddPoint（行间共享），needNormals → vectorU×vectorV 单位法线逐顶点，needParams →
// uMap/vMap 映射的距离参数（缺省 0..1 分数）。参考的 _reversed 恒 false（未启用）。
void PolyfaceBuilder::AddUVGridBody(UVSurface const& surface, int numU, int numV,
                                    Segment1d const* uMap, Segment1d const* vMap)
{
    if (!m_polyface || numU < 1 || numV < 1) return;
    bool const needNormals = m_options.needNormals;
    bool const needParams = m_options.needParams;

    std::vector<int32_t> idx0, idx1, nidx0, nidx1, pidx0, pidx1;
    idx0.reserve(static_cast<size_t>(numU) + 1);
    idx1.reserve(static_cast<size_t>(numU) + 1);
    if (needNormals) { nidx0.reserve(static_cast<size_t>(numU) + 1); nidx1.reserve(static_cast<size_t>(numU) + 1); }
    if (needParams) { pidx0.reserve(static_cast<size_t>(numU) + 1); pidx1.reserve(static_cast<size_t>(numU) + 1); }

    double const du = 1.0 / numU;
    double const dv = 1.0 / numV;
    for (int v = 0; v <= numV; ++v) {
        idx1.clear();
        nidx1.clear();
        pidx1.clear();
        for (int u = 0; u <= numU; ++u) {
            double const uFrac = u * du;
            double const vFrac = v * dv;
            Plane3dByOriginAndVectors const plane = surface.UVFractionToPointAndTangents(uFrac, vFrac);
            idx1.push_back(m_polyface->AddPoint(plane.origin));
            if (needNormals) {
                Vector3d normal = Vector3d::FromCrossProduct(plane.vectorU, plane.vectorV);
                normal.Normalize();
                nidx1.push_back(m_polyface->AddNormal(normal));
            }
            if (needParams) {
                pidx1.push_back(m_polyface->AddParam(Point2d::From(
                    uMap ? uMap->FractionToPoint(uFrac) : uFrac,
                    vMap ? vMap->FractionToPoint(vFrac) : vFrac)));
            }
        }
        if (v > 0) {
            for (int u = 0; u < numU; ++u) {
                // 发射序 (A0, A1, B1, B0)（addIndexedQuadPointIndexes 非 reversed 分支）
                if (!m_options.shouldTriangulate) {
                    m_polyface->AddPointIndex(idx0[u], true);
                    m_polyface->AddPointIndex(idx0[u + 1], true);
                    m_polyface->AddPointIndex(idx1[u + 1], true);
                    m_polyface->AddPointIndex(idx1[u], true);
                    if (needNormals) {
                        m_polyface->AddNormalIndex(nidx0[u]);
                        m_polyface->AddNormalIndex(nidx0[u + 1]);
                        m_polyface->AddNormalIndex(nidx1[u + 1]);
                        m_polyface->AddNormalIndex(nidx1[u]);
                    }
                    if (needParams) {
                        m_polyface->AddParamIndex(pidx0[u]);
                        m_polyface->AddParamIndex(pidx0[u + 1]);
                        m_polyface->AddParamIndex(pidx1[u + 1]);
                        m_polyface->AddParamIndex(pidx1[u]);
                    }
                    m_polyface->TerminateFacet();
                } else {
                    // 三角化分支（:1945-1983）：(A0, A1, B0) + (B0, A1, B1)
                    m_polyface->AddPointIndex(idx0[u], true);
                    m_polyface->AddPointIndex(idx0[u + 1], true);
                    m_polyface->AddPointIndex(idx1[u], true);
                    if (needNormals) {
                        m_polyface->AddNormalIndex(nidx0[u]);
                        m_polyface->AddNormalIndex(nidx0[u + 1]);
                        m_polyface->AddNormalIndex(nidx1[u]);
                    }
                    if (needParams) {
                        m_polyface->AddParamIndex(pidx0[u]);
                        m_polyface->AddParamIndex(pidx0[u + 1]);
                        m_polyface->AddParamIndex(pidx1[u]);
                    }
                    m_polyface->TerminateFacet();
                    m_polyface->AddPointIndex(idx1[u], true);
                    m_polyface->AddPointIndex(idx0[u + 1], true);
                    m_polyface->AddPointIndex(idx1[u + 1], true);
                    if (needNormals) {
                        m_polyface->AddNormalIndex(nidx1[u]);
                        m_polyface->AddNormalIndex(nidx0[u + 1]);
                        m_polyface->AddNormalIndex(nidx1[u + 1]);
                    }
                    if (needParams) {
                        m_polyface->AddParamIndex(pidx1[u]);
                        m_polyface->AddParamIndex(pidx0[u + 1]);
                        m_polyface->AddParamIndex(pidx1[u + 1]);
                    }
                    m_polyface->TerminateFacet();
                }
            }
        }
        std::swap(idx0, idx1);
        std::swap(nidx0, nidx1);
        std::swap(pidx0, pidx1);
    }
}

// Ported from: itwinjs-core PolyfaceBuilder.addBox (:1436-1481) — 六面各一个
// BilinearPatch UV-grid；四个侧面共享连续环绕的 u 距离参数（x→y→-x→-y 前进），
// 上下盖面各自 (0..xLen)×(0..yLen)。applyMaxEdgeLength 默认不激活 → 每面 1×1 网格。
void PolyfaceBuilder::AddBox(Box const& box)
{
    if (!m_polyface) return;
    auto const corners = box.GetCorners(); // 0-3 base (z=0), 4-7 top (z=1); x-fastest then y then z.
    // 参考 :1438-1443（maxXY(baseX,baseX) 的"复制"逐字保留）
    double const xLength = std::max(box.GetBaseX(), box.GetBaseX());
    double const yLength = std::max(box.GetBaseY(), box.GetTopY());
    double zLength = 0.0;
    for (int i = 0; i < 4; ++i) {
        double const d = corners[static_cast<size_t>(i)].Distance(corners[static_cast<size_t>(i + 4)]);
        if (d > zLength) zLength = d;
    }
    int const numX = m_options.ApplyMaxEdgeLength(1, xLength);
    int const numY = m_options.ApplyMaxEdgeLength(1, yLength);
    int const numZ = m_options.ApplyMaxEdgeLength(1, zLength);
    // wrap the 4 out-of-plane faces as a single parameter space with "distance"
    // advancing in x, then y, then negative x, then negative y（参考 :1447-1464）
    Segment1d uParamRange = Segment1d::Create(0.0, xLength);
    Segment1d const vParamRange = Segment1d::Create(0.0, zLength);
    {
        BilinearPatch const face = BilinearPatch::Create(corners[0], corners[1], corners[4], corners[5]);
        AddUVGridBody(face, numX, numZ, &uParamRange, &vParamRange);
    }
    uParamRange.Shift(xLength);
    {
        BilinearPatch const face = BilinearPatch::Create(corners[1], corners[3], corners[5], corners[7]);
        AddUVGridBody(face, numY, numZ, &uParamRange, &vParamRange);
    }
    uParamRange.Shift(yLength);
    {
        BilinearPatch const face = BilinearPatch::Create(corners[3], corners[2], corners[7], corners[6]);
        AddUVGridBody(face, numX, numZ, &uParamRange, &vParamRange);
    }
    uParamRange.Shift(xLength);
    {
        BilinearPatch const face = BilinearPatch::Create(corners[2], corners[0], corners[6], corners[4]);
        AddUVGridBody(face, numY, numZ, &uParamRange, &vParamRange);
    }
    // finally end that wraparound face（参考 :1465 endFace——DanQing 每面已在 grid 内 TerminateFacet）
    if (box.Capped()) {
        uParamRange.Set(0.0, xLength);
        Segment1d const vCapRange = Segment1d::Create(0.0, yLength);
        {
            BilinearPatch const face = BilinearPatch::Create(corners[4], corners[5], corners[6], corners[7]);
            AddUVGridBody(face, numX, numY, &uParamRange, &vCapRange);
        }
        {
            BilinearPatch const face = BilinearPatch::Create(corners[2], corners[3], corners[0], corners[1]);
            AddUVGridBody(face, numX, numY, &uParamRange, &vCapRange);
        }
    }
}

// Ported from: itwinjs-core PolyfaceBuilder.addCone (:1099-1126) — 侧面 UV-grid
// （平滑法线 + 距离参数），端盖为环点 fan 三角（单一法线索引 → computeUVParams
// planar 路径；packedUVParams 通道未移植，盖面 UV 由写回阶段的 planar 投影覆盖）。
void PolyfaceBuilder::AddCone(Cone const& cone, int strokeCount)
{
    if (!m_polyface) return;
    // 参考 :1100-1103：strokeCount = options.applyTolerancesToArc(cone.getMaxRadius())
    int const n = strokeCount > 0 ? strokeCount : m_options.ApplyTolerancesToArc(cone.GetMaxRadius(), Angle::k2Pi);
    if (n < 3) return;
    // 参考 :1105-1113：axisStrokeCount = applyMaxEdgeLength(1, vDistanceRange.low)；
    // maxEdgeLength 未激活（默认 0）→ 恒 1。
    int const axisStrokeCount = 1;

    Point2d const sizes = cone.MaxIsoParametricDistance();
    Segment1d const uMap = Segment1d::Create(0.0, sizes.x);
    Segment1d const vMap = Segment1d::Create(0.0, sizes.y);
    AddUVGridBody(cone, n, axisStrokeCount, &uMap, &vMap);

    // 端盖（:1115-1125 addTrianglesInUncheckedConvexPolygon ×2）：环点 (0..n 闭合)
    // fan（p0, pi, pi+1）；统一单位法线（单次 AddNormal——同索引，planar 判定通过）。
    if (cone.Capped()) {
        auto ring = [&](double vFrac) {
            std::vector<Point3d> pts(static_cast<size_t>(n) + 1);
            for (int i = 0; i <= n; ++i)
                pts[static_cast<size_t>(i)] = cone.UVFractionToPoint(static_cast<double>(i) / n, vFrac);
            return pts;
        };
        auto fanCap = [&](std::vector<Point3d> const& pts, bool reverse) {
            // quickUnitNormal（LineString3d）→ areaNormalGo 等价物；reverse 翻负。
            auto areaNormal = PolygonOps::areaNormalGo(pts);
            if (!areaNormal.has_value()) return;
            Vector3d normal = *areaNormal;
            if (reverse) { normal.x = -normal.x; normal.y = -normal.y; normal.z = -normal.z; }
            int32_t const normalIndex = m_options.needNormals ? m_polyface->AddNormal(normal) : -1;
            int32_t const i0 = m_polyface->AddPoint(pts[0]);
            int32_t prev = m_polyface->AddPoint(pts[1]);
            for (size_t i = 2; i + 1 < pts.size(); ++i) {
                int32_t const next = m_polyface->AddPoint(pts[i]);
                if (reverse) {
                    // toggleReversedFacetFlag 下的 addIndexedTrianglePointIndexes：(A, C, B)
                    m_polyface->AddPointIndex(i0, true);
                    m_polyface->AddPointIndex(next, true);
                    m_polyface->AddPointIndex(prev, true);
                } else {
                    m_polyface->AddPointIndex(i0, true);
                    m_polyface->AddPointIndex(prev, true);
                    m_polyface->AddPointIndex(next, true);
                }
                if (normalIndex > 0) {
                    m_polyface->AddNormalIndex(normalIndex);
                    m_polyface->AddNormalIndex(normalIndex);
                    m_polyface->AddNormalIndex(normalIndex);
                }
                m_polyface->TerminateFacet();
                prev = next;
            }
        };
        if (cone.GetRadiusA() > 0.0) fanCap(ring(0.0), /*reverse=*/true);   // 下端盖翻转
        if (cone.GetRadiusB() > 0.0) fanCap(ring(1.0), /*reverse=*/false);
    }
}

// Ported from: itwinjs-core PolyfaceBuilder.addSphere (:1414-1433) — 经纬 UV-grid
// （平滑法线 + 距离参数）。端盖走 strokeConstantVSection（未移植）——全球 sweep
// 无端盖（参考 lineString 长度≈0 同样跳过）；非全球 sweep 的端盖为 TODO。
void PolyfaceBuilder::AddSphere(Sphere const& sphere, int strokeCount)
{
    if (!m_polyface) return;
    // 参考 :1415-1416：numStrokeTheta = strokeCount ?? options.applyTolerancesToArc(maxAxisRadius)
    int numStrokeTheta = strokeCount > 0 ? strokeCount
                                         : m_options.ApplyTolerancesToArc(sphere.MaxAxisRadius(), Angle::k2Pi);
    if (numStrokeTheta % 2 != 0) numStrokeTheta += 1;  // Geometry.isOdd → +1
    // latitudeSweepFraction = sweepRadians / π（Sphere.ts:95）
    double const sweepFraction = sphere.CloneLatitudeSweep().SweepRadians() / Angle::kPi;
    int const hi = static_cast<int>(std::ceil(numStrokeTheta * 0.5));
    int numStrokePhi = static_cast<int>(std::fabs(static_cast<double>(numStrokeTheta) * sweepFraction));
    if (numStrokePhi < 1) numStrokePhi = 1;
    if (numStrokePhi > hi) numStrokePhi = hi;
    if (numStrokePhi < 1) numStrokePhi = 1;

    // TODO: 非全球 sweep 端盖（strokeConstantVSection + addTrianglesInUncheckedConvexPolygon，
    // Sphere::ConstantVSection 当前返回 nullptr —— 见 Sphere.h TODO commit-N）。
    Point2d const sizes = sphere.MaxIsoParametricDistance();
    Segment1d const uMap = Segment1d::Create(0.0, sizes.x);
    Segment1d const vMap = Segment1d::Create(0.0, sizes.y);
    AddUVGridBody(sphere, numStrokeTheta, numStrokePhi, &uMap, &vMap);
}

// Ported from: itwinjs-core PolyfaceBuilder.addTorusPipe (1128) — torus grid of quads.
// (Reference uses UV-grid via addUVGridBody + cap transforms; here we sample the torus directly.
//  Local point = ((R + r·cosφ)cosθ, (R + r·cosφ)sinθ, r·sinφ); world via localToWorld.)
void PolyfaceBuilder::AddTorusPipe(TorusPipe const& torusPipe, int numPhi, int numTheta)
{
    if (!m_polyface) return;
    int nPhi = numPhi > 0 ? numPhi : 8;
    double thetaFraction = torusPipe.GetThetaFraction();
    int nTheta = numTheta > 0 ? numTheta : static_cast<int>(std::ceil(16.0 * thetaFraction));
    if (nTheta < 2) nTheta = 2;

    Transform const& t = torusPipe.GetLocalToWorld();
    double R = torusPipe.GetRadiusA(); // major (local)
    double r = torusPipe.GetRadiusB(); // minor (local)
    double sweep = torusPipe.GetSweepRadians();

    auto pt = [&](int i, int j) {
        double phi = Angle::k2Pi * static_cast<double>(i) / nPhi;
        double theta = sweep * static_cast<double>(j) / nTheta;
        double cphi = std::cos(phi), sphi = std::sin(phi);
        double cth = std::cos(theta), sth = std::sin(theta);
        double ringR = R + r * cphi;
        return t.MultiplyXYZ(ringR * cth, ringR * sth, r * sphi);
    };
    for (int j = 0; j < nTheta; ++j) {
        for (int i = 0; i < nPhi; ++i) {
            Point3d q[4] = {
                pt(i, j),
                pt(i + 1, j),
                pt(i + 1, j + 1),
                pt(i, j + 1),
            };
            AddQuadFacet(q);
        }
    }
    // TODO commit-N: caps for partial-sweep capped torusPipe (end ring caps).
}

// ===========================================================================
// Sweep tessellation (direct path — SweepContour is TS-only, not ported).
// Each sweep strokes its cross-section to a polyline, emits side quads between
// consecutive stroke sets, and optionally triangulates two cap polygons.
// ===========================================================================
namespace {

// Stroke a CurveChain (Loop/Path) cross-section to a concatenated polyline. For a Loop the closure
// point is appended (first point repeated) so side-quad iteration i=1..n-1 closes the loop. Parity/
// Union regions return empty (Phase-N: needs per-loop handling + ear-clip caps).
std::vector<Point3d> strokeContour(CurveCollection const& cc, StrokeOptions const& opts)
{
    std::vector<Point3d> pts;
    const auto type = cc.GetCurveCollectionType();
    if (type != CurveCollectionType::Loop && type != CurveCollectionType::Path)
        return pts;
    auto const& chain = static_cast<CurveChain const&>(cc);
    auto ls = LineString3d::create(std::vector<Point3d>{});
    for (auto const& child : chain.Curves()) {
        if (child) child->EmitStrokes(*ls, opts);
    }
    pts = ls->Points();
    if (type == CurveCollectionType::Loop && !pts.empty() && !pts.front().AlmostEqual(pts.back()))
        pts.push_back(pts.front());
    return pts;
}

// Emit side quads between two matched stroke sets a/b (same point count): one quad per edge i=1..n-1.
void emitSideQuads(PolyfaceBuilder& builder, std::vector<Point3d> const& a, std::vector<Point3d> const& b)
{
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 1; i < n; ++i) {
        Point3d q[4] = {a[i - 1], a[i], b[i], b[i - 1]};  // A0,A1,B1,B0 — outward per AddBox winding
        builder.AddQuadFacet(q);
    }
}

}  // namespace

// Ported from: PolyfaceBuilder.ts addLinearSweep (1363).
void PolyfaceBuilder::AddLinearSweep(LinearSweep const& sweep)
{
    if (!m_polyface) return;
    auto const& curves = sweep.GetCurves();
    if (!curves) return;
    std::vector<Point3d> ptsA = strokeContour(*curves, m_options);
    if (ptsA.size() < 2) return;

    const Vector3d sweepVec = sweep.CloneSweepVector();
    std::vector<Point3d> ptsB = ptsA;
    Transform xlate = Transform::CreateTranslation(sweepVec);
    xlate.MultiplyPoint3dArrayInPlace(ptsB);

    // Winding pre-normalize: make the contour area normal point opposite the sweep vector so side
    // quad normals (edgeDir × sweep) face outward. (1:1 reference contourNormalAgreesWithSweepDir.)
    if (const auto an = PolygonOps::areaNormalGo(ptsA)) {
        if (an->DotProduct(sweepVec) > 0.0) {
            std::reverse(ptsA.begin(), ptsA.end());
            std::reverse(ptsB.begin(), ptsB.end());
        }
    }

    emitSideQuads(*this, ptsA, ptsB);

    if (sweep.Capped() && curves->isAnyRegionType()) {
        AddPolygon(ptsA);                       // bottom cap (normal −sweep → outward)
        std::vector<Point3d> top = ptsB;
        std::reverse(top.begin(), top.end());   // top cap reversed (normal +sweep → outward)
        AddPolygon(top);
    }
}

// Ported from: PolyfaceBuilder.ts addRotationalSweep (1211).
void PolyfaceBuilder::AddRotationalSweep(RotationalSweep const& sweep)
{
    if (!m_polyface) return;
    auto const& curves = sweep.GetCurves();
    if (!curves) return;
    std::vector<Point3d> ptsA = strokeContour(*curves, m_options);
    if (ptsA.size() < 2) return;

    const Point3d axisOrigin = sweep.GetAxisOrigin();
    const Vector3d axisDir = sweep.GetAxisDirection();
    const double sweepRadians = sweep.GetSweepAngle().Radians();
    const double axisLen = axisDir.Magnitude();
    if (axisLen < kSmallMetricDistance || std::abs(sweepRadians) < kSmallMetricDistance)
        return;

    // Max radius from the axis → step count via the arc-tolerance helper.
    double maxRadius = 0.0;
    for (auto const& p : ptsA) {
        const Vector3d radial = Vector3d::FromStartEnd(axisOrigin, p);
        const double r = Vector3d::FromCrossProduct(radial, axisDir).Magnitude() / axisLen;
        if (r > maxRadius) maxRadius = r;
    }
    const size_t numStep = m_options.ApplyTolerancesToArc(maxRadius, sweepRadians);
    const double angleStep = sweepRadians / static_cast<double>(numStep);

    std::vector<Point3d> prev = ptsA;
    std::vector<Point3d> curr;
    for (size_t step = 1; step <= numStep; ++step) {
        Transform rot = Transform::CreateRotationAboutAxis(axisOrigin, axisDir, angleStep * static_cast<double>(step));
        curr = ptsA;
        rot.MultiplyPoint3dArrayInPlace(curr);
        emitSideQuads(*this, prev, curr);
        prev = curr;
    }

    if (sweep.Capped() && curves->isAnyRegionType()) {
        AddPolygon(ptsA);                       // start cap
        std::vector<Point3d> end = curr;
        std::reverse(end.begin(), end.end());   // end cap reversed
        AddPolygon(end);
    }
}

// Ported from: PolyfaceBuilder.ts addRuledSweep (1381).
void PolyfaceBuilder::AddRuledSweep(RuledSweep const& sweep)
{
    if (!m_polyface) return;
    auto const& contours = sweep.GetCurves();
    if (contours.size() < 2) return;

    // Stroke each profile. Require matching point counts between consecutive profiles (graceful skip
    // on mismatch — faithful StrokeCountSection reconciliation is Phase-N).
    std::vector<std::vector<Point3d>> strokes;
    strokes.reserve(contours.size());
    for (auto const& cc : contours) {
        if (!cc) return;
        strokes.push_back(strokeContour(*cc, m_options));
    }

    for (size_t k = 1; k < strokes.size(); ++k) {
        if (strokes[k - 1].size() == strokes[k].size() && strokes[k].size() >= 2)
            emitSideQuads(*this, strokes[k - 1], strokes[k]);
        // else: mismatched profile point counts — Phase-N (StrokeCountSection).
    }

    if (sweep.Capped() && contours.front() && contours.front()->isAnyRegionType()) {
        if (!strokes.front().empty()) AddPolygon(strokes.front());                       // bottom cap
        if (!strokes.back().empty()) {
            std::vector<Point3d> top = strokes.back();
            std::reverse(top.begin(), top.end());
            AddPolygon(top);                                                              // top cap (reversed)
        }
    }
}

END_DQ_GEOM_NAMESPACE
