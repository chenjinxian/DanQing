// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/GeometryPrimitives.ts
// DanQing dqRender — Geometry record method bodies (factories / getPolyfaces / getStrokes per subclass).
#include "GeometryPrimitives.h"

#include <algorithm>

#include <dqGeom/CurvePrimitive.h>
#include <dqGeom/LineString3d.h>

using namespace dqGeom;

BEGIN_DQ_RENDER_NAMESPACE

// --- Static factories (1:1 Geometry.createFromXxx) ---
std::unique_ptr<Geometry> Geometry::createFromPointString(const std::vector<Point3d>& pts,
    const Transform& tf, const Range3d& tileRange, const DisplayParams& params,
    std::optional<dqCommon::Feature> feature) {
    return std::make_unique<PrimitivePointStringGeometry>(pts, tf, tileRange, params, std::move(feature));
}
std::unique_ptr<Geometry> Geometry::createFromLineString(const std::vector<Point3d>& pts,
    const Transform& tf, const Range3d& tileRange, const DisplayParams& params,
    std::optional<dqCommon::Feature> feature) {
    return std::make_unique<PrimitiveLineStringGeometry>(pts, tf, tileRange, params, std::move(feature));
}
std::unique_ptr<Geometry> Geometry::createFromLoop(const dqBase::RefPtr<Loop>& loop,
    const Transform& tf, const Range3d& tileRange, const DisplayParams& params,
    bool disjoint, std::optional<dqCommon::Feature> feature) {
    return std::make_unique<PrimitiveLoopGeometry>(loop, tf, tileRange, params, disjoint, std::move(feature));
}
std::unique_ptr<Geometry> Geometry::createFromSolidPrimitive(const dqBase::RefPtr<SolidPrimitive>& primitive,
    const Transform& tf, const Range3d& tileRange, const DisplayParams& params,
    std::optional<dqCommon::Feature> feature) {
    return std::make_unique<SolidPrimitiveGeometry>(primitive, tf, tileRange, params, std::move(feature));
}
std::unique_ptr<Geometry> Geometry::createFromPath(const dqBase::RefPtr<Path>& path,
    const Transform& tf, const Range3d& tileRange, const DisplayParams& params,
    bool disjoint, std::optional<dqCommon::Feature> feature) {
    return std::make_unique<PrimitivePathGeometry>(path, tf, tileRange, params, disjoint, std::move(feature));
}
std::unique_ptr<Geometry> Geometry::createFromPolyface(const dqBase::RefPtr<IndexedPolyface>& ipf,
    const Transform& tf, const Range3d& tileRange, const DisplayParams& params,
    std::optional<dqCommon::Feature> feature) {
    return std::make_unique<PrimitivePolyfaceGeometry>(ipf, tf, tileRange, params, std::move(feature));
}

// DEViation（登记 2026-09-14，ACS 盘深缩放消失修复；用户报告"放大 ~20 次后 Z 轴
// 浅蓝圆盘填充渐进消失、缩小恢复"）：
//   参考路径 Geometry.getPolyfaces(tolerance) 把【世界】单位的 chordTol 直接交给
//   _getPolyfaces 对【局部】几何做 stroke 决策（缩放留在 placement 矩阵，stroke 后
//   才变换到世界）。屏幕恒定尺寸的装饰（ACS triad：局部半径恒 0.2，世界尺寸∝
//   worldPerPixel）在深缩放端 ratio=chordTol/r_local→0，参考公式（StrokeOptions.ts
//   :184-200 applyChordTol，与 DanQing 移植 1:1）给出的 stroke 数爆增（实测 17→768），
//   世界弦长塌到 MeshBuilder 顶点焊接容差（chordTol）以下 → 全部 fan 三角形被
//   退化守卫（MeshBuilder.ts:154-157，忠实移植）丢弃 → 填充消失（描边 polyline
//   无面积守卫而存活 = 用户截图的"只剩圆环"形态）。按同一公式推演参考若画此盘
//   同样爆点；DTA 空白连接不画 ACS triad（CDP 实测：acs viewflag 开、世界原点居中、
//   无 triad 像素），参考无可见行为可对齐。
//   修复=容差随坐标空间换算：chordTol ÷ placement 最大轴缩放。stroke 决策落在与
//   容差同一尺度（屏幕恒定装饰的有效半径恒 ~10.4·tol，弦长恒 ~4×tol），任何缩放
//   深度都不触发焊接退化。placement 为 identity/rigid（普通图元）时无变化。
double Geometry::localChordTolerance(double worldTolerance) const
{
    if (worldTolerance <= 0.0 || m_transform.IsIdentity())
        return worldTolerance;
    auto const& m = m_transform.matrix;
    double const s = std::max({m.ColumnX().Magnitude(), m.ColumnY().Magnitude(), m.ColumnZ().Magnitude()});
    return s > 0.0 ? worldTolerance / s : worldTolerance;
}

// Ported from: Geometry.getPolyfaces (62-72).
std::optional<PolyfacePrimitiveList> Geometry::getPolyfaces(double tolerance) const {
    auto facetOptions = StrokeOptions::CreateForFacets();
    facetOptions.chordTol = localChordTolerance(tolerance);
    if (m_displayParams.isTextured())
        facetOptions.needParams = true;
    if (!m_displayParams.ignoreLighting()) // ###TODO don't generate normals for 2d views.
        facetOptions.needNormals = true;
    return _getPolyfaces(facetOptions);
}

// Ported from: Geometry.getStrokes (74-78).
std::optional<StrokesPrimitiveList> Geometry::getStrokes(double tolerance) const {
    auto strokeOptions = StrokeOptions::CreateForCurves();
    strokeOptions.chordTol = localChordTolerance(tolerance);
    return _getStrokes(strokeOptions);
}

// --- PrimitivePathGeometry ---

// Ported from: PrimitivePathGeometry.collectCurveStrokes (122-129). Reference uses getPackedStrokes +
// getPoint3dArray; DanQing strokes via CloneStroked and collects each child LineString3d's points, then
// transforms them in place.
void PrimitivePathGeometry::collectCurveStrokes(StrokesPrimitivePointLists& strksPts,
    const CurveChain& loopOrPath, const StrokeOptions& facetOptions, const Transform& trans) {
    auto stroked = loopOrPath.CloneStroked(facetOptions);
    if (!stroked) return;
    for (const auto& child : stroked->Curves()) {
        if (!child || child->GetCurveType() != CurveType::LineString) continue;
        const auto& ls = static_cast<const LineString3d&>(*child);
        std::vector<Point3d> pts = ls.Points();
        trans.MultiplyPoint3dArrayInPlace(pts);
        strksPts.emplace_back(std::move(pts));
    }
}

// Ported from: PrimitivePathGeometry.getStrokesForLoopOrPath (103-120).
std::optional<StrokesPrimitiveList> PrimitivePathGeometry::getStrokesForLoopOrPath(
    const CurveChain& loopOrPath, const StrokeOptions& facetOptions, const DisplayParams& params,
    bool isDisjoint, const Transform& transform) {
    StrokesPrimitiveList list;
    if (!loopOrPath.isAnyRegionType() || params.wantRegionOutline()) {
        StrokesPrimitivePointLists ptsLists;
        collectCurveStrokes(ptsLists, loopOrPath, facetOptions, transform);
        if (!ptsLists.empty()) {
            const bool isPlanar = loopOrPath.isAnyRegionType();
            auto prim = StrokesPrimitive::create(params, isDisjoint, isPlanar);
            prim.strokes = std::move(ptsLists);
            list.push_back(std::move(prim));
        }
    }
    return list;
}

std::optional<StrokesPrimitiveList> PrimitivePathGeometry::_getStrokes(const StrokeOptions& facetOptions) const {
    return getStrokesForLoopOrPath(*m_path, facetOptions, m_displayParams, m_isDisjoint, m_transform);
}

// Ported from: PrimitivePointStringGeometry._getStrokes (145-155).
std::optional<StrokesPrimitiveList> PrimitivePointStringGeometry::_getStrokes(const StrokeOptions&) const {
    StrokesPrimitiveList list;
    StrokesPrimitivePointLists ptsLists;
    ptsLists.emplace_back(StrokesPrimitivePointList(m_pts));
    auto prim = StrokesPrimitive::create(m_displayParams, /*isDisjoint=*/true, /*isPlanar=*/false);
    prim.strokes = std::move(ptsLists);
    prim.transform(m_transform);
    list.push_back(std::move(prim));
    return list;
}

// Ported from: PrimitiveLineStringGeometry._getStrokes (171-181).
std::optional<StrokesPrimitiveList> PrimitiveLineStringGeometry::_getStrokes(const StrokeOptions&) const {
    StrokesPrimitiveList list;
    StrokesPrimitivePointLists ptsLists;
    ptsLists.emplace_back(StrokesPrimitivePointList(m_pts));
    auto prim = StrokesPrimitive::create(m_displayParams, /*isDisjoint=*/false, /*isPlanar=*/false);
    prim.strokes = std::move(ptsLists);
    prim.transform(m_transform);
    list.push_back(std::move(prim));
    return list;
}

// Ported from: PrimitiveLoopGeometry._getPolyfaces (195-212). SweepContour path is Phase-N TODO;
// fallback strokes the loop + AddPolygon (single-loop planar region; holes/parity uncovered — matches
// reference caveat GeometryPrimitives.ts:209).
std::optional<PolyfacePrimitiveList> PrimitiveLoopGeometry::_getPolyfaces(const StrokeOptions& facetOptions) const {
    if (!m_loop->isAnyRegionType()) return std::nullopt;

    auto builder = PolyfaceBuilder::create(facetOptions);
    size_t strokePts = 0;
    auto stroked = m_loop->CloneStroked(facetOptions);
    if (stroked) {
        std::vector<Point3d> pts;
        for (const auto& child : stroked->Curves()) {
            if (!child || child->GetCurveType() != CurveType::LineString) continue;
            const auto& ls = static_cast<const LineString3d&>(*child);
            pts.insert(pts.end(), ls.Points().begin(), ls.Points().end());
        }
        m_transform.MultiplyPoint3dArrayInPlace(pts);
        if (pts.size() >= 3) builder->AddPolygon(pts);
        for (const auto& child : stroked->Curves()) {
            if (child && child->GetCurveType() == CurveType::LineString)
                strokePts += static_cast<const LineString3d&>(*child).Points().size();
        }
    }
    const auto pf = builder->ClaimPolyface();
    // TEMP-DIAG（ACS 圆盘深放大消失排查）：loop→polyface 的离散化与三角化计数
    //（env DANQING_GL_TRACE=1）。strokePts==0 ⇒ CloneStroked 失败；pfVerts==0 ⇒
    // AddPolygon/ClaimPolyface 丢弃。
    if (std::getenv("DANQING_GL_TRACE")) {
        size_t verts = 0, tris = 0;
        if (pf) {
            auto const& pd = pf->Data();
            verts = pd.pointIndex.size() / 3;
            tris = pd.pointIndex.size() / 3;
        }
        std::fprintf(stderr, "[LODDIAG] loop-polyface chordTol=%.3g strokePts=%zu pfNull=%d pfTris=%zu\n",
                     facetOptions.chordTol, strokePts, pf ? 0 : 1, tris);
        (void)verts;
    }
    const bool wantEdges = (m_displayParams.regionEdgeType() == RegionEdgeType::Default);
    return PolyfacePrimitiveList{ PolyfacePrimitive::create(m_displayParams, pf, wantEdges, /*isPlanar=*/true) };
}

// Ported from: PrimitivePolyfaceGeometry ctor (223-226) — bake transform into the polyface.
PrimitivePolyfaceGeometry::PrimitivePolyfaceGeometry(const dqBase::RefPtr<IndexedPolyface>& polyface,
    const Transform& tf, const Range3d& range, const DisplayParams& params,
    std::optional<dqCommon::Feature> feature)
    : Geometry(tf, range, params, std::move(feature)),
      m_polyface(tf.IsIdentity() ? polyface
                                 : polyface->CloneTransformed(tf).template StaticCast<IndexedPolyface>()) {}

// Ported from: PrimitivePolyfaceGeometry._getPolyfaces (228-248). DanQing PolyfaceData has no UV
// param/paramIndex arrays (TODO Phase-N), so the !hasTexture param-clearing branch is a no-op.
std::optional<PolyfacePrimitiveList> PrimitivePolyfaceGeometry::_getPolyfaces(const StrokeOptions& facetOptions) const {
    (void)hasTexture(); // TODO Phase-N: clear UV params when !hasTexture (PolyfaceData param arrays pending).
    auto& data = m_polyface->Data();
    if (!facetOptions.needNormals) {
        data.normals.clear();
        data.normalIndex.clear();
    } else if (data.normals.empty()) {
        PolyfaceQuery::BuildAverageNormals(*m_polyface); // PR A: Phase-N stub (no-op until BuildAverageNormalsContext port).
    }
    return PolyfacePrimitiveList{ PolyfacePrimitive::create(m_displayParams, m_polyface) };
}

// Ported from: SolidPrimitiveGeometry ctor (256-260) — bake transform into the solid.
SolidPrimitiveGeometry::SolidPrimitiveGeometry(const dqBase::RefPtr<SolidPrimitive>& primitive,
    const Transform& tf, const Range3d& range, const DisplayParams& params,
    std::optional<dqCommon::Feature> feature)
    : Geometry(tf, range, params, std::move(feature)),
      m_primitive(tf.IsIdentity() ? primitive
                                  : primitive->CloneTransformed(tf).template StaticCast<SolidPrimitive>()) {}

// Ported from: SolidPrimitiveGeometry._getPolyfaces (264-268).
std::optional<PolyfacePrimitiveList> SolidPrimitiveGeometry::_getPolyfaces(const StrokeOptions& opts) const {
    auto builder = PolyfaceBuilder::create(opts);
    builder->AddGeometryQuery(*m_primitive);
    return PolyfacePrimitiveList{ PolyfacePrimitive::create(m_displayParams, builder->ClaimPolyface()) };
}

END_DQ_RENDER_NAMESPACE
