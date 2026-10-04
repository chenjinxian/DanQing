// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Cesium 装饰图元陈列馆实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/EmptyExample.ts
//              （CesiumDecorator :50-496）
#include "CesiumDecorator.h"

#include <dqApp/Application.h>
#include <dqApp/DecorateContext.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqCommon/ColorDef.h>
#include <dqRender/GraphicBuilder.h>
#include <dqRender/RenderGraphic.h>

#include <dqGeom/AngleSweep.h>
#include <dqGeom/Box.h>
#include <dqGeom/Cone.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Sphere.h>

#include <cmath>
#include <memory>
#include <vector>

namespace Gui {

using namespace dqRender;

// Ported from: CesiumDecorator.start (:266-269 + :40-48).
CesiumDecorator* CesiumDecorator::start(dqApp::IModelConnection* imodel)
{
    auto* decorator = new CesiumDecorator();
    decorator->m_iModel = imodel;
    dqApp::Application::Get().GetViewManager().AddDecorator(decorator);
    return decorator;
}

// Ported from: CesiumDecorator.stop (:493-495).
void CesiumDecorator::stop()
{
    dqApp::Application::Get().GetViewManager().DropDecorator(this);
}

// XY 共面三分点圆弧（EQUIVALENCE 见头文件——createCircularStartMiddleEnd）。
dqBase::RefPtr<dqGeom::Arc3d> CesiumDecorator::arcFromStartMiddleEnd(
    dqGeom::Point3d const& start, dqGeom::Point3d const& mid,
    dqGeom::Point3d const& end)
{
    constexpr double kPi = 3.14159265358979323846;
    // 外接圆心（XY 面）：|C-S|²=|C-M|² 与 |C-S|²=|C-E|² 的线性解。
    double const ax = mid.x - start.x, ay = mid.y - start.y;
    double const bx = end.x - start.x, by = end.y - start.y;
    double const d = 2.0 * (ax * by - ay * bx);
    if (std::abs(d) < 1.0e-12)
        return nullptr;  // 共线（参考 createCircularStartMiddleEnd 同返回 undefined）
    double const as = ax * ax + ay * ay;
    double const bs = bx * bx + by * by;
    double const cx = start.x + (by * as - ay * bs) / d;
    double const cy = start.y + (ax * bs - bx * as) / d;

    // vector0 = S−C；vector90 = 面内 CCW 90°（XY 面 → (−v0y, v0x, 0)）。
    dqGeom::Vector3d const v0 =
        dqGeom::Vector3d::From(start.x - cx, start.y - cy, 0.0);
    dqGeom::Vector3d const v90 = dqGeom::Vector3d::From(-v0.y, v0.x, 0.0);

    // sweep：S 起角 → E 终角，取过 M 的方向（CCW/CW 二择——中点行进角与
    // sweep 同号则保持，否则 ±2π 归一）。
    auto angleOf = [&](dqGeom::Point3d const& p) {
        return std::atan2(p.y - cy, p.x - cx);
    };
    double const a0 = angleOf(start);
    double const a1 = angleOf(end);
    double const aMid = angleOf(mid);
    double sweep = a1 - a0;
    bool const sameDir = (sweep >= 0.0) == (aMid - a0 >= 0.0);
    if (!sameDir)
        sweep += sweep >= 0.0 ? -2.0 * kPi : 2.0 * kPi;
    return dqGeom::Arc3d::FromVectors(
        dqGeom::Point3d::From(cx, cy, start.z), v0, v90,
        dqGeom::AngleSweep::FromStartSweepRadians(a0, sweep));
}

// Ported from: CesiumDecorator.decorate (:58-71).
void CesiumDecorator::Decorate(dqApp::DecorateContext& context)
{
    if (m_iModel == nullptr)
        return;
    auto* view = context.GetViewport().GetView();
    if (view == nullptr || !view->isSpatialView())
        return;

    createPointDecorations(context);
    createLineStringDecorations(context);
    createShapeDecorations(context);
    createArcDecorations(context);
    createPathDecorations(context);
    createLoopDecorations(context);
    createPolyfaceDecorations(context);
    createSolidPrimitiveDecorations(context);
}

// 布局基（参考各族的 projectExtents.center）。
static dqGeom::Point3d extentCenter(dqApp::IModelConnection* imodel)
{
    dqGeom::Range3d const& ext = imodel->GetProjectExtents();
    return dqGeom::Point3d::From(
        (ext.low.x + ext.high.x) / 2.0,
        (ext.low.y + ext.high.y) / 2.0,
        (ext.low.z + ext.high.z) / 2.0);
}

// 族的公共 builder 半边（参考各 forEach 内的三行——createGraphic →
// setSymbology → add* → addDecorationFromBuilder）。
// computeChordTolerance：PrimitiveBuilder.finish() 的 LOD 门（AcsTriad 同源
// 先例——无 closure 时 finish 恒 null）；取视口高向米/像素估计。
using BuilderPtr = std::unique_ptr<dqRender::GraphicBuilder>;
static BuilderPtr beginDecoration(dqApp::DecorateContext& context,
                                  GraphicType type,
                                  dqCommon::ColorDef const& color, int weight)
{
    dqApp::Viewport& vp = context.GetViewport();
    dqRender::GraphicBuilderOptions opts;
    opts.type = type;
    dqGeom::Point3d const refPoint(0.0, 0.0, 0.0);
    double const worldPerPixel =
        vp.GetViewingSpace().getPixelSizeAtPoint(&refPoint);
    opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
    auto builder = vp.createGraphicBuilder(opts);
    if (!builder)
        return nullptr;
    builder->setSymbology(color, color, weight);
    return builder;
}

static void endDecoration(dqApp::DecorateContext& context, GraphicType type,
                          BuilderPtr builder)
{
    if (!builder)
        return;
    if (auto* g = builder->finish()) {
        context.GetViewport().createGraphicOwner(g);
        context.AddDecoration(type, g);
    }
}

// Ported from: createPointDecorations (:73-98).
void CesiumDecorator::createPointDecorations(dqApp::DecorateContext& context)
{
    auto const center = extentCenter(m_iModel);

    dqGeom::Point3d const points[] = {
        dqGeom::Point3d::From(center.x - 50000, center.y, center.z + 10000),
        dqGeom::Point3d::From(center.x, center.y + 50000, center.z + 20000),
        dqGeom::Point3d::From(center.x + 50000, center.y - 50000, center.z + 30000),
    };
    for (auto const& point : points) {
        auto b = beginDecoration(context, GraphicType::WorldDecoration,
                                 dqCommon::ColorDef::from(0, 0, 255), 1);
        if (b) {
            b->addPointString(&point, 1);
            endDecoration(context, GraphicType::WorldDecoration, std::move(b));
        }
    }

    dqGeom::Point2d const overlayPoints[] = {
        dqGeom::Point2d::From(center.x - 90000, center.y + 90000),
    };
    auto b = beginDecoration(context, GraphicType::WorldOverlay,
                             dqCommon::ColorDef::from(255, 215, 0),  // gold to contrast with 3d points
                             2);
    if (b) {
        b->addPointString2d(overlayPoints, 1, center.z + 6000);
        endDecoration(context, GraphicType::WorldOverlay, std::move(b));
    }
}

// Ported from: createLineStringDecorations (:100-148).
void CesiumDecorator::createLineStringDecorations(
    dqApp::DecorateContext& context)
{
    auto const center = extentCenter(m_iModel);

    struct LineDef {
        std::vector<dqGeom::Point3d> points;
        GraphicType type;
        dqCommon::ColorDef color;
    };
    LineDef const lines[] = {
        {{
             dqGeom::Point3d::From(center.x - 120000, center.y - 120000, center.z + 5000),
             dqGeom::Point3d::From(center.x + 120000, center.y - 120000, center.z + 5000),
             dqGeom::Point3d::From(center.x + 120000, center.y + 120000, center.z + 5000),
             dqGeom::Point3d::From(center.x - 120000, center.y + 120000, center.z + 5000),
             dqGeom::Point3d::From(center.x - 120000, center.y - 120000, center.z + 5000),
         },
         GraphicType::WorldDecoration,
         dqCommon::ColorDef::from(255, 0, 0)},
        {{
             dqGeom::Point3d::From(center.x - 150000, center.y, center.z + 19000),
             dqGeom::Point3d::From(center.x, center.y + 150000, center.z + 19000),
             dqGeom::Point3d::From(center.x + 150000, center.y, center.z + 19000),
             dqGeom::Point3d::From(center.x, center.y - 150000, center.z + 19000),
             dqGeom::Point3d::From(center.x - 150000, center.y, center.z + 19000),
         },
         GraphicType::WorldOverlay,
         dqCommon::ColorDef::from(255, 165, 0)},
    };
    for (auto const& line : lines) {
        auto b = beginDecoration(context, line.type, line.color, 2);
        if (b) {
            b->addLineString(line.points.data(), line.points.size());
            endDecoration(context, line.type, std::move(b));
        }
    }

    dqGeom::Point2d const overlayLinePoints[] = {
        dqGeom::Point2d::From(center.x + 90000, center.y - 90000),
        dqGeom::Point2d::From(center.x + 150000, center.y - 90000),
        dqGeom::Point2d::From(center.x + 150000, center.y - 30000),
        dqGeom::Point2d::From(center.x + 90000, center.y - 30000),
        dqGeom::Point2d::From(center.x + 90000, center.y - 90000),
    };
    auto b2 = beginDecoration(context, GraphicType::WorldOverlay,
                              dqCommon::ColorDef::from(0, 200, 255), 3);
    if (b2) {
        b2->addLineString2d(overlayLinePoints, 5, center.z + 8000);
        endDecoration(context, GraphicType::WorldOverlay, std::move(b2));
    }
}

// Ported from: createShapeDecorations (:150-197).
void CesiumDecorator::createShapeDecorations(dqApp::DecorateContext& context)
{
    auto const center = extentCenter(m_iModel);

    struct ShapeDef {
        std::vector<dqGeom::Point3d> points;
        GraphicType type;
        dqCommon::ColorDef color;
    };
    ShapeDef const shapes[] = {
        {{
             dqGeom::Point3d::From(center.x - 80000, center.y - 80000, center.z + 15000),
             dqGeom::Point3d::From(center.x + 80000, center.y - 80000, center.z + 15000),
             dqGeom::Point3d::From(center.x, center.y + 80000, center.z + 15000),
             dqGeom::Point3d::From(center.x - 80000, center.y - 80000, center.z + 15000),
         },
         GraphicType::WorldDecoration,
         dqCommon::ColorDef::from(0, 255, 0)},
        {{
             dqGeom::Point3d::From(center.x - 60000, center.y + 40000, center.z + 25000),
             dqGeom::Point3d::From(center.x - 20000, center.y + 40000, center.z + 25000),
             dqGeom::Point3d::From(center.x - 20000, center.y + 80000, center.z + 25000),
             dqGeom::Point3d::From(center.x - 60000, center.y + 40000, center.z + 25000),
         },
         GraphicType::WorldDecoration,
         dqCommon::ColorDef::from(255, 0, 255)},
    };
    for (auto const& shape : shapes) {
        auto b = beginDecoration(context, shape.type, shape.color, 3);
        if (b) {
            b->addShape(shape.points.data(), shape.points.size());
            endDecoration(context, shape.type, std::move(b));
        }
    }

    dqGeom::Point2d const overlayShapePoints[] = {
        dqGeom::Point2d::From(center.x + 50000, center.y + 90000),
        dqGeom::Point2d::From(center.x + 120000, center.y + 90000),
        dqGeom::Point2d::From(center.x + 120000, center.y + 140000),
        dqGeom::Point2d::From(center.x + 50000, center.y + 140000),
        dqGeom::Point2d::From(center.x + 50000, center.y + 90000),
    };
    auto b2 = beginDecoration(context, GraphicType::WorldDecoration,
                              dqCommon::ColorDef::from(186, 85, 211), 3);
    if (b2) {
        b2->addShape2d(overlayShapePoints, 5, center.z + 9000);
        endDecoration(context, GraphicType::WorldDecoration, std::move(b2));
    }
}

// createScaledXYColumns 的等价组装（EQUIVALENCE 见头文件）：vector0=(rx,0,0)、
// vector90=(0,ry,0)。
static dqBase::RefPtr<dqGeom::Arc3d> xyColumnsArc(
    dqGeom::Point3d const& center, double rx, double ry,
    dqGeom::AngleSweep const& sweep)
{
    return dqGeom::Arc3d::FromVectors(
        center, dqGeom::Vector3d::From(rx, 0.0, 0.0),
        dqGeom::Vector3d::From(0.0, ry, 0.0), sweep);
}

// Ported from: createArcDecorations (:199-264).
void CesiumDecorator::createArcDecorations(dqApp::DecorateContext& context)
{
    auto const center = extentCenter(m_iModel);
    constexpr double kPi = 3.14159265358979323846;

    struct ArcDef {
        dqBase::RefPtr<dqGeom::Arc3d> arc;
        bool isEllipse;
        bool filled;
        GraphicType type;
        dqCommon::ColorDef color;
    };
    ArcDef const arcs[] = {
        {xyColumnsArc(
             dqGeom::Point3d::From(center.x - 100000, center.y - 100000,
                                   center.z + 35000),
             40000, 40000,
             dqGeom::AngleSweep::FromStartSweepRadians(0, kPi)),
         false, false, GraphicType::WorldDecoration,
         dqCommon::ColorDef::from(255, 255, 0)},
        {xyColumnsArc(
             dqGeom::Point3d::From(center.x + 100000, center.y + 100000,
                                   center.z + 40000),
             30000, 50000,
             dqGeom::AngleSweep::FromStartSweepRadians(0, kPi * 2)),
         true, true, GraphicType::WorldOverlay,
         dqCommon::ColorDef::from(0, 255, 255)},
        {xyColumnsArc(
             dqGeom::Point3d::From(center.x, center.y - 200000, center.z + 45000),
             60000, 30000,
             dqGeom::AngleSweep::FromStartSweepRadians(kPi / 4, kPi * 1.5)),
         false, false, GraphicType::WorldDecoration,
         dqCommon::ColorDef::from(255, 100, 100)},
    };
    for (auto const& arcDef : arcs) {
        if (!arcDef.arc.IsValid())
            continue;
        auto b = beginDecoration(context, arcDef.type, arcDef.color, 2);
        if (b) {
            b->addArc(*arcDef.arc, arcDef.isEllipse, arcDef.filled);
            endDecoration(context, arcDef.type, std::move(b));
        }
    }

    auto const overlayFullEllipse = xyColumnsArc(
        dqGeom::Point3d::From(center.x + 160000, center.y + 40000, center.z),
        25000, 40000,
        dqGeom::AngleSweep::FromStartSweepRadians(0, kPi * 2));
    if (overlayFullEllipse.IsValid()) {
        auto b = beginDecoration(context, GraphicType::WorldOverlay,
                                 dqCommon::ColorDef::from(64, 224, 208), 2);
        if (b) {
            b->addArc2d(*overlayFullEllipse, true, true, center.z + 12000);
            endDecoration(context, GraphicType::WorldOverlay, std::move(b));
        }
    }
}

// Ported from: createPathDecorations (:272-327).
void CesiumDecorator::createPathDecorations(dqApp::DecorateContext& context)
{
    auto const c = extentCenter(m_iModel);
    double const z = c.z + 50000;
    double const R = 20000;

    // First segment: horizontal line from left to right
    dqGeom::Point3d const P0 =
        dqGeom::Point3d::From(c.x - 80000 + m_xOffset, c.y - 80000, z);
    dqGeom::Point3d const P1 =
        dqGeom::Point3d::From(c.x - 20000 + m_xOffset, c.y - 80000, z);
    // Second segment: vertical line upward to corner point
    dqGeom::Point3d const P2 =
        dqGeom::Point3d::From(c.x - 20000 + m_xOffset, c.y - 20000, z);
    // Arc transition: 90-degree arc with radius 20000
    dqGeom::Point3d const arcMid = dqGeom::Point3d::From(
        P2.x + R / std::sqrt(2.0), P2.y + R / std::sqrt(2.0), z);
    dqGeom::Point3d const arcEnd = dqGeom::Point3d::From(P2.x + R, P2.y, z);
    auto const arc = arcFromStartMiddleEnd(P2, arcMid, arcEnd);
    // Third segment: continue upward from arc end point
    dqGeom::Point3d const P3 =
        dqGeom::Point3d::From(arcEnd.x, arcEnd.y + 40000, z);

    {
        std::vector<dqGeom::CurvePrimitivePtr> curves;
        curves.push_back(dqGeom::LineString3d::create({P0, P1}));
        curves.push_back(dqGeom::LineString3d::create({P1, P2}));
        if (arc.IsValid())
            curves.push_back(arc);
        curves.push_back(dqGeom::LineString3d::create({arcEnd, P3}));
        auto path1 = dqGeom::Path::Create(curves);
        if (path1.IsValid()) {
            auto b = beginDecoration(context, GraphicType::WorldDecoration,
                                     dqCommon::ColorDef::from(255, 100, 200), 3);
            if (b) {
                b->addPath(*path1);
                endDecoration(context, GraphicType::WorldDecoration, std::move(b));
            }
        }
    }

    // Second path: zigzag wave pattern
    {
        std::vector<dqGeom::Point3d> zigzagPts = {
            dqGeom::Point3d::From(c.x + 50000 + m_xOffset, c.y - 100000, z + 10000),
            dqGeom::Point3d::From(c.x + 70000 + m_xOffset, c.y - 80000, z + 10000),
            dqGeom::Point3d::From(c.x + 50000 + m_xOffset, c.y - 60000, z + 10000),
            dqGeom::Point3d::From(c.x + 70000 + m_xOffset, c.y - 40000, z + 10000),
            dqGeom::Point3d::From(c.x + 50000 + m_xOffset, c.y - 20000, z + 10000),
            dqGeom::Point3d::From(c.x + 70000 + m_xOffset, c.y, z + 10000),
        };
        auto zigZag = dqGeom::Path::Create(std::vector<dqGeom::CurvePrimitivePtr>{
            dqGeom::LineString3d::create(std::move(zigzagPts))});
        if (zigZag.IsValid()) {
            auto b = beginDecoration(context, GraphicType::WorldOverlay,
                                     dqCommon::ColorDef::from(100, 255, 100), 3);
            if (b) {
                b->addPath(*zigZag);
                endDecoration(context, GraphicType::WorldOverlay, std::move(b));
            }
        }
    }
}

// Ported from: createLoopDecorations (:329-349).
void CesiumDecorator::createLoopDecorations(dqApp::DecorateContext& context)
{
    auto const c = extentCenter(m_iModel);
    double const z = c.z + 70000;

    // Simple test: Just one triangle
    std::vector<dqGeom::Point3d> trianglePoints = {
        dqGeom::Point3d::From(c.x - 60000 + m_xOffset, c.y - 60000, z),
        dqGeom::Point3d::From(c.x + 60000 + m_xOffset, c.y - 60000, z),
        dqGeom::Point3d::From(c.x + m_xOffset, c.y + 60000, z),
        dqGeom::Point3d::From(c.x - 60000 + m_xOffset, c.y - 60000, z),
    };
    auto triangleLoop = dqGeom::Loop::CreatePolygon(trianglePoints);
    if (triangleLoop.IsValid()) {
        auto b = beginDecoration(context, GraphicType::WorldDecoration,
                                 dqCommon::ColorDef::from(255, 0, 255), 2);
        if (b) {
            b->addLoop(*triangleLoop);
            endDecoration(context, GraphicType::WorldDecoration, std::move(b));
        }
    }
}

// createPyramidPolyface（:387-412——侧躺金字塔：base 方 + apex +X + 4 三角面）。
static dqBase::RefPtr<dqGeom::IndexedPolyface> createPyramidPolyface(
    dqGeom::Point3d const& center, double baseSize)
{
    auto builder = dqGeom::PolyfaceBuilder::create();
    if (!builder)
        return nullptr;
    double const halfSize = baseSize / 2;
    double const height = baseSize * 0.8;  // Pyramid height

    // Rotated pyramid - lying on its side for better view
    dqGeom::Point3d const base1(center.x, center.y - halfSize, center.z - halfSize);
    dqGeom::Point3d const base2(center.x, center.y + halfSize, center.z - halfSize);
    dqGeom::Point3d const base3(center.x, center.y + halfSize, center.z + halfSize);
    dqGeom::Point3d const base4(center.x, center.y - halfSize, center.z + halfSize);
    dqGeom::Point3d const apex(center.x + height, center.y, center.z);

    dqGeom::Point3d const quad[4] = {base1, base2, base3, base4};
    builder->AddQuadFacet(quad);
    dqGeom::Point3d const f1[3] = {base1, apex, base2};
    builder->AddTriangleFacet(f1);
    dqGeom::Point3d const f2[3] = {base2, apex, base3};
    builder->AddTriangleFacet(f2);
    dqGeom::Point3d const f3[3] = {base3, apex, base4};
    builder->AddTriangleFacet(f3);
    dqGeom::Point3d const f4[3] = {base4, apex, base1};
    builder->AddTriangleFacet(f4);

    return builder->ClaimPolyface();
}

// createBoxPolyface（:414-441——六面外法线）。
static dqBase::RefPtr<dqGeom::IndexedPolyface> createBoxPolyface(
    dqGeom::Point3d const& center, double width, double depth, double height)
{
    auto builder = dqGeom::PolyfaceBuilder::create();
    if (!builder)
        return nullptr;
    double const halfW = width / 2, halfD = depth / 2, halfH = height / 2;

    dqGeom::Point3d const b1(center.x - halfW, center.y - halfD, center.z - halfH);
    dqGeom::Point3d const b2(center.x + halfW, center.y - halfD, center.z - halfH);
    dqGeom::Point3d const b3(center.x + halfW, center.y + halfD, center.z - halfH);
    dqGeom::Point3d const b4(center.x - halfW, center.y + halfD, center.z - halfH);
    dqGeom::Point3d const t1(center.x - halfW, center.y - halfD, center.z + halfH);
    dqGeom::Point3d const t2(center.x + halfW, center.y - halfD, center.z + halfH);
    dqGeom::Point3d const t3(center.x + halfW, center.y + halfD, center.z + halfH);
    dqGeom::Point3d const t4(center.x - halfW, center.y + halfD, center.z + halfH);

    dqGeom::Point3d const bottom[4] = {b4, b3, b2, b1};
    builder->AddQuadFacet(bottom);
    dqGeom::Point3d const top[4] = {t1, t2, t3, t4};
    builder->AddQuadFacet(top);
    dqGeom::Point3d const front[4] = {b1, b2, t2, t1};
    builder->AddQuadFacet(front);
    dqGeom::Point3d const right[4] = {b2, b3, t3, t2};
    builder->AddQuadFacet(right);
    dqGeom::Point3d const back[4] = {b3, b4, t4, t3};
    builder->AddQuadFacet(back);
    dqGeom::Point3d const left[4] = {b4, b1, t1, t4};
    builder->AddQuadFacet(left);

    return builder->ClaimPolyface();
}

// Ported from: createPolyfaceDecorations (:351-385).
void CesiumDecorator::createPolyfaceDecorations(
    dqApp::DecorateContext& context)
{
    auto const c = extentCenter(m_iModel);
    double const z = c.z + 80000;
    double const yOffset = -80000;  // Position below other decorations

    auto pyramidPolyface = createPyramidPolyface(
        dqGeom::Point3d::From(c.x + m_xOffset, c.y + yOffset, z),
        40000  // Base size
    );
    if (pyramidPolyface.IsValid()) {
        auto b = beginDecoration(context, GraphicType::WorldDecoration,
                                 dqCommon::ColorDef::from(255, 165, 0),  // Orange with transparent fill
                                 2);
        if (b) {
            b->addPolyface(*pyramidPolyface, true);
            endDecoration(context, GraphicType::WorldDecoration, std::move(b));
        }
    }

    auto boxPolyface = createBoxPolyface(
        dqGeom::Point3d::From(c.x + 100000 + m_xOffset, c.y + yOffset, z),
        30000,   // Width
        30000,   // Depth
        40000);  // Height
    if (boxPolyface.IsValid()) {
        auto b = beginDecoration(context, GraphicType::WorldOverlay,
                                 dqCommon::ColorDef::from(100, 255, 255),  // Cyan with transparent fill
                                 2);
        if (b) {
            b->addPolyface(*boxPolyface, true);
            endDecoration(context, GraphicType::WorldOverlay, std::move(b));
        }
    }
}

// Ported from: createSolidPrimitiveDecorations (:443-491).
void CesiumDecorator::createSolidPrimitiveDecorations(
    dqApp::DecorateContext& context)
{
    auto const c = extentCenter(m_iModel);
    double const z = c.z + 80000;  // Same height as other decorations
    double const yOffset = -80000;  // Position below other decorations
    double const boxSize = 20000;   // Smaller for better visibility

    // Box solid primitive（:450-464）。
    {
        dqGeom::Point3d const boxCenter(c.x + 200000, c.y + yOffset, z);
        dqGeom::Range3d const boxRange(
            dqGeom::Point3d::From(boxCenter.x - boxSize / 2,
                                  boxCenter.y - boxSize / 2,
                                  boxCenter.z - boxSize / 2),
            dqGeom::Point3d::From(boxCenter.x + boxSize / 2,
                                  boxCenter.y + boxSize / 2,
                                  boxCenter.z + boxSize / 2));
        auto boxSolid = dqGeom::Box::CreateRange(boxRange, true);
        if (boxSolid.IsValid()) {
            auto b = beginDecoration(
                context, GraphicType::WorldDecoration,
                dqCommon::ColorDef::from(255, 100, 100),  // Red with transparent fill
                2);
            if (b) {
                b->addSolidPrimitive(*boxSolid);
                endDecoration(context, GraphicType::WorldDecoration, std::move(b));
            }
        }
    }

    // Sphere solid primitive（:466-476——createCenterRadius 的全纬扫面）。
    {
        double const sphereRadius = 15000;
        dqGeom::Point3d const sphereCenter(c.x + 240000, c.y + yOffset, z);
        auto sphereSolid = dqGeom::Sphere::CreateCenterRadius(
            sphereCenter, sphereRadius, dqGeom::Sphere::FullLatitudeSweep(), true);
        if (sphereSolid.IsValid()) {
            auto b = beginDecoration(
                context, GraphicType::WorldOverlay,
                dqCommon::ColorDef::from(100, 100, 255),  // Blue with transparent fill
                2);
            if (b) {
                b->addSolidPrimitive(*sphereSolid);
                endDecoration(context, GraphicType::WorldOverlay, std::move(b));
            }
        }
    }

    // Cone solid primitive（:478-490——CreateBaseAndTarget[vectorX/90 =
    // 单位基] radiusA=coneRadius、radiusB=0 → 锥）。
    {
        double const coneHeight = 25000;
        double const coneRadius = 10000;
        dqGeom::Point3d const coneStart(c.x + 280000, c.y + yOffset, z);
        dqGeom::Point3d const coneEnd(c.x + 280000, c.y + yOffset, z + coneHeight);
        auto coneSolid = dqGeom::Cone::CreateBaseAndTarget(
            coneStart, coneEnd, dqGeom::Vector3d::From(1, 0, 0),
            dqGeom::Vector3d::From(0, 1, 0), coneRadius, 0.0, true);
        if (coneSolid.IsValid()) {
            auto b = beginDecoration(
                context, GraphicType::WorldDecoration,
                dqCommon::ColorDef::from(100, 255, 100),  // Green with transparent fill
                2);
            if (b) {
                b->addSolidPrimitive(*coneSolid);
                endDecoration(context, GraphicType::WorldDecoration, std::move(b));
            }
        }
    }
}

}  // namespace Gui
