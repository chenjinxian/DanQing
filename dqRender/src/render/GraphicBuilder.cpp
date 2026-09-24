// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/render/GraphicAssembler.ts
// DanQing dqRender — GraphicBuilder non-virtual curve/arc/range/frustum method bodies
//
// 保真依据：逐方法移植 GraphicAssembler.ts（addArc/addArc2d/addCurvePrimitive/addRangeBox/
// addFrustum/addFrustumSides/addRangeBoxFromCorners/addRangeBoxSidesFromCorners）。这些是非虚方法，
// 经 this->addPath/addLoop/addLineString/addShape 虚函数分发到具体 PrimitiveBuilder。
#include "dqRender/GraphicBuilder.h"

#include <dqCommon/Npc.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/CurvePrimitive.h>
#include <dqGeom/LineSegment3d.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <variant>

BEGIN_DQ_RENDER_NAMESPACE

// Npc corner index helper (Npc enum values are the array indices; 1:1 Frustum.ts Npc).
static inline size_t npc(dqCommon::Npc n) noexcept { return static_cast<size_t>(n); }

// Ported from: GraphicAssembler.addArc (219-236).
void GraphicBuilder::addArc(const dqGeom::Arc3d& ellipse, bool isEllipse, bool filled) {
    // Clone the arc into a 1-child chain (1:1 Loop.create(ellipse) / Path.create(ellipse)).
    auto arcChild = ellipse.clone().StaticCast<dqGeom::CurvePrimitive>();

    if (isEllipse || filled) {
        auto loop = dqGeom::Loop::Create();
        loop->TryAddChild(arcChild);
        // Filled partial arc: bridge the sweep gap with a line segment to the start.
        if (filled && !isEllipse && !ellipse.Sweep().IsFullCircle()) {
            dqGeom::Point3d start = ellipse.StartPoint();
            dqGeom::Point3d end = ellipse.EndPoint();
            loop->TryAddChild(dqGeom::LineSegment3d::create(start, end).StaticCast<dqGeom::CurvePrimitive>());
        }
        addLoop(*loop);
    } else {
        auto path = dqGeom::Path::Create();
        path->TryAddChild(arcChild);
        addPath(*path);
    }
}

// Ported from: GraphicAssembler.addArc2d (245-253) — offset arc to z=zDepth, then addArc.
void GraphicBuilder::addArc2d(const dqGeom::Arc3d& ellipse, bool isEllipse, bool filled, double zDepth) {
    if (0.0 == zDepth) {
        addArc(ellipse, isEllipse, filled);
        return;
    }
    // Reference sets ell.center.z = zDepth; replicate by translating the cloned arc along z.
    auto ell = ellipse.clone().StaticCast<dqGeom::Arc3d>();
    const double dz = zDepth - ellipse.CenterRef().z;
    ell->TryTransformInPlace(dqGeom::Transform::CreateTranslation(0.0, 0.0, dz));
    addArc(*ell, isEllipse, filled);
}

// Ported from: GraphicAssembler.addCurvePrimitive (265-284) — dispatch by curve type.
void GraphicBuilder::addCurvePrimitive(const dqGeom::CurvePrimitive& curve) {
    using CT = dqGeom::CurveType;
    switch (curve.GetCurveType()) {
        case CT::LineString: {
            auto const& ls = static_cast<dqGeom::LineString3d const&>(curve);
            addLineString(ls.Points().data(), ls.Points().size());
            break;
        }
        case CT::LineSegment: {
            dqGeom::Point3d pts[2] = { curve.StartPoint(), curve.EndPoint() };
            addLineString(pts, 2);
            break;
        }
        case CT::Arc: {
            addArc(static_cast<dqGeom::Arc3d const&>(curve), false, false);
            break;
        }
        default: {
            auto path = dqGeom::Path::Create();
            path->TryAddChild(curve.clone().StaticCast<dqGeom::CurvePrimitive>());
            addPath(*path);
            break;
        }
    }
}

// Ported from: GraphicAssembler.addRangeBox (347-356). Solid branch needs SolidPrimitive (Phase 3).
void GraphicBuilder::addRangeBox(const dqGeom::Range3d& range, bool solid) {
    if (!solid) {
        addFrustum(dqCommon::Frustum::fromRange(range));
        return;
    }
    // TODO Phase-3 (SolidPrimitive): Box::createRange(range, true) → addSolidPrimitive(box).
    (void)range;
}

// Ported from: GraphicAssembler.addFrustum (359-361).
void GraphicBuilder::addFrustum(const dqCommon::Frustum& frustum) {
    addRangeBoxFromCorners(frustum.points);
}

// Ported from: GraphicAssembler.addFrustumSides (364-366).
void GraphicBuilder::addFrustumSides(const dqCommon::Frustum& frustum) {
    addRangeBoxSidesFromCorners(frustum.points);
}

// Ported from: GraphicAssembler.addRangeBoxFromCorners (369-386).
void GraphicBuilder::addRangeBoxFromCorners(const dqGeom::Point3d* p) {
    using dqCommon::Npc;
    {
        dqGeom::Point3d pts[10] = {
            p[npc(Npc::LeftBottomFront)], p[npc(Npc::LeftTopFront)],
            p[npc(Npc::RightTopFront)], p[npc(Npc::RightBottomFront)],
            p[npc(Npc::RightBottomRear)], p[npc(Npc::RightTopRear)],
            p[npc(Npc::LeftTopRear)], p[npc(Npc::LeftBottomRear)],
            p[npc(Npc::LeftBottomFront)], p[npc(Npc::RightBottomFront)],
        };
        addLineString(pts, 10);
    }
    {
        dqGeom::Point3d pts[2] = { p[npc(Npc::LeftTopFront)], p[npc(Npc::LeftTopRear)] };
        addLineString(pts, 2);
    }
    {
        dqGeom::Point3d pts[2] = { p[npc(Npc::RightTopFront)], p[npc(Npc::RightTopRear)] };
        addLineString(pts, 2);
    }
    {
        dqGeom::Point3d pts[2] = { p[npc(Npc::LeftBottomRear)], p[npc(Npc::RightBottomRear)] };
        addLineString(pts, 2);
    }
}

// Ported from: GraphicAssembler.addRangeBoxSidesFromCorners (389-426) — 6 faces as closed polygons.
void GraphicBuilder::addRangeBoxSidesFromCorners(const dqGeom::Point3d* p) {
    using dqCommon::Npc;
    auto shape = [&](dqCommon::Npc a, dqCommon::Npc b, dqCommon::Npc c, dqCommon::Npc d) {
        dqGeom::Point3d pts[5] = { p[npc(a)], p[npc(b)], p[npc(c)], p[npc(d)], p[npc(a)] };
        addShape(pts, 5);
    };
    shape(Npc::LeftBottomFront, Npc::LeftTopFront, Npc::RightTopFront, Npc::RightBottomFront);
    shape(Npc::RightTopRear, Npc::LeftTopRear, Npc::LeftBottomRear, Npc::RightBottomRear);
    shape(Npc::RightTopRear, Npc::LeftTopRear, Npc::LeftTopFront, Npc::RightTopFront);
    shape(Npc::RightTopRear, Npc::RightBottomRear, Npc::RightBottomFront, Npc::RightTopFront);
    shape(Npc::LeftBottomRear, Npc::RightBottomRear, Npc::RightBottomFront, Npc::LeftBottomFront);
    shape(Npc::LeftBottomRear, Npc::LeftTopRear, Npc::LeftTopFront, Npc::LeftBottomFront);
}

// Ported from: GraphicAssembler.addPrimitive (GraphicAssembler.ts:302-341).
// Dispatch by variant type to the matching addXXX. Each `true === primitive.xxx` undefined-coercion
// in the reference is already expressed by the C++ struct's `bool = false` default members.
void GraphicBuilder::addPrimitive(const GraphicPrimitive& primitive) {
    if (const auto* p = std::get_if<GraphicLineString>(&primitive)) {
        addLineString(p->points.data(), p->points.size());
        return;
    }
    if (const auto* p = std::get_if<GraphicLineString2d>(&primitive)) {
        addLineString2d(p->points.data(), p->points.size(), p->zDepth);
        return;
    }
    if (const auto* p = std::get_if<GraphicPointString>(&primitive)) {
        addPointString(p->points.data(), p->points.size());
        return;
    }
    if (const auto* p = std::get_if<GraphicPointString2d>(&primitive)) {
        addPointString2d(p->points.data(), p->points.size(), p->zDepth);
        return;
    }
    if (const auto* p = std::get_if<GraphicShape>(&primitive)) {
        addShape(p->points.data(), p->points.size());
        return;
    }
    if (const auto* p = std::get_if<GraphicShape2d>(&primitive)) {
        addShape2d(p->points.data(), p->points.size(), p->zDepth);
        return;
    }
    if (const auto* p = std::get_if<GraphicArc>(&primitive)) {
        addArc(*p->arc, p->isEllipse, p->filled);
        return;
    }
    if (const auto* p = std::get_if<GraphicArc2d>(&primitive)) {
        addArc2d(*p->arc, p->isEllipse, p->filled, p->zDepth);
        return;
    }
    if (const auto* p = std::get_if<GraphicPath>(&primitive)) {
        addPath(*p->path);
        return;
    }
    if (const auto* p = std::get_if<GraphicLoop>(&primitive)) {
        addLoop(*p->loop);
        return;
    }
    if (const auto* p = std::get_if<GraphicPolyface>(&primitive)) {
        addPolyface(*p->polyface, p->filled);
        return;
    }
    if (const auto* p = std::get_if<GraphicSolidPrimitive>(&primitive)) {
        addSolidPrimitive(*p->solidPrimitive);
        return;
    }
}

END_DQ_RENDER_NAMESPACE
