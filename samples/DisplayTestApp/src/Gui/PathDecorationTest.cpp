// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Path decoration test/demo 实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/PathDecorationTest.ts
//              (PathCanvasDecoration :10-18 + PathDecorationTest :22-72 +
//              PathDecorationTestTool :74-82 + _getPath :84-109)。
// EQUIVALENCE 登记（E4/E4b/E7）见头文件。
#include "PathDecorationTest.h"

#include <dqApp/Application.h>
#include <dqApp/DecorateContext.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewingSpace.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqGeom/Arc3d.h>
#include <dqGeom/AngleSweep.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Vector3d.h>

#include <dqRender/GraphicBuilder.h>

#include <vector>

namespace Gui {

namespace {

// E4：transientIds.getNext 的本地静态计数器承载（ViewClipDecoration.cpp:150
// 先例同型——跨族相撞发散见头注）。
uint32_t s_nextTransientId = 0xFF000000;

}  // namespace

PathDecorationTest* PathDecorationTest::s_decorator = nullptr;

// Ported from: PathCanvasDecoration (:10-18)——drawDecoration :11-17。
// E7 文本占位（头注）：fillText 居中白字 → 文本 bbox 白色填充框。
dqRender::CanvasDecoration PathDecorationTest::s_canvasDecoration = [] {
    dqRender::CanvasDecoration decoration;
    decoration.position = std::nullopt;  // 参考未设 position（绝对 (256,256) 直绘）
    decoration.drawDecoration = [](dqRender::CanvasContext& ctx) {
        // :12-16——textAlign=center / textBaseline=middle / font=14px /
        // fillStyle=white / fillText(text, 256, 256)。占位框 = 文本 bbox
        // （MeasureLabel 先例的宽度近似式：len × fontPx × 0.6）。
        constexpr double kFontPx = 14.0;   // :14 "14px sans-serif"
        constexpr double kTextLen =
            sizeof("Path Decoration Test Tool Active") - 1;  // :16 文本长 32
        double const halfW = kTextLen * kFontPx * 0.6 * 0.5;  // 134.4
        double const halfH = kFontPx * 0.5;                   // 7
        constexpr double cx = 256.0;  // :16 fillText x
        constexpr double cy = 256.0;  // :16 fillText y
        ctx.beginPath();
        ctx.moveTo(cx - halfW, cy - halfH);
        ctx.lineTo(cx + halfW, cy - halfH);
        ctx.lineTo(cx + halfW, cy + halfH);
        ctx.lineTo(cx - halfW, cy + halfH);
        ctx.lineTo(cx - halfW, cy - halfH);
        ctx.setFillStyle(dqCommon::ColorDef::white);  // :15 fillStyle "white"
        ctx.fill();
    };
    return decoration;
}();

// Ported from: constructor (:29-31)。
PathDecorationTest::PathDecorationTest(dqGeom::Range3d const& extents)
    : m_path(getPath(extents))  // :30
{}

// :38-39 惰性赋值行（Decorate 与 AssignPickIdForTest 共用）。
void PathDecorationTest::ensurePickId()
{
    if (!m_pickId.has_value())
        m_pickId = ++s_nextTransientId;  // E4（头注）
}

// Ported from: decorate (:37-44)。
void PathDecorationTest::Decorate(dqApp::DecorateContext& context)
{
    ensurePickId();  // :38-39
    // :40-42——createGraphicBuilder(GraphicType.WorldDecoration, undefined,
    // _pickId) + addPath + addDecorationFromBuilder。pickId 第三参 =
    // GraphicBuilderOptions.pickable{id}——DanQing 无该载体（E4b 头注）。
    dqApp::Viewport& vp = context.GetViewport();
    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::WorldDecoration;
    // computeChordTolerance 闭包（PrimitiveBuilder.finish 的 LOD 门——
    // CesiumDecorator/ViewClipDecoration 先例同源；参考视口工厂内建，
    // ViewContext.ts:73-75 → ViewportGraphicBuilderOptions）。
    double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(nullptr);
    opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
    auto builder = vp.createGraphicBuilder(opts);
    if (builder) {
        builder->addPath(*m_path);  // :41（默认 symbology——参考无 setSymbology）
        // :42 addDecorationFromBuilder = finish + addDecoration（ViewClipDecoration
        // 先例形态）。
        context.AddDecoration(dqRender::GraphicType::WorldDecoration, builder->finish());
    }
    context.AddCanvasDecoration(s_canvasDecoration);  // :43
}

// Ported from: testDecorationHit (:47)。
bool PathDecorationTest::TestDecorationHit(uint32_t featureId) const
{
    // `id === this._pickId`——_pickId undefined（未 decorate）时恒假 =
    // optional 缺席守卫。
    return m_pickId.has_value() && featureId == *m_pickId;
}

// Ported from: start (:53-56)。
void PathDecorationTest::Start(dqGeom::Range3d const& extents)
{
    s_decorator = new PathDecorationTest(extents);  // :54
    dqApp::Application::Get().GetViewManager().AddDecorator(s_decorator);  // :55
}

// Ported from: stop (:59-63)。
void PathDecorationTest::Stop()
{
    if (s_decorator != nullptr) {  // :60
        dqApp::Application::Get().GetViewManager().DropDecorator(s_decorator);  // :61
        delete s_decorator;  // C++ 显式所有权（参考 GC——:62 置空随 GC 回收）
    }
    s_decorator = nullptr;  // :62
}

// Ported from: toggle (:66-71)。
void PathDecorationTest::Toggle(dqGeom::Range3d const& extents)
{
    if (s_decorator == nullptr)  // :67 `undefined === decorator`
        Start(extents);          // :68
    else
        Stop();                  // :70
}

// Ported from: PathDecorationTestTool.run (:76-81)。
bool PathDecorationTestTool::run()
{
    // :77-79——selectedView 缺席则不动（恒 true 返回）。DanQing 等价面 =
    // GetActiveViewport（GltfDecorationTool.cpp:208 先例）。
    auto* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    if (vp != nullptr)  // :78 `undefined !== vp`
        PathDecorationTest::Toggle(vp->GetView()->GetIModel()->GetProjectExtents());  // :79
    return true;  // :80
}

// Ported from: _getPath (:84-109)。
dqGeom::PathPtr getPath(dqGeom::Range3d const& extents)
{
    // :85-87——null range + extendPoint(low/high)（= extents 拷贝语义）。
    dqGeom::Range3d range = dqGeom::Range3d::CreateNull();
    range.ExtendPoint(extents.low);
    range.ExtendPoint(extents.high);

    dqGeom::Range1d const range1d = dqGeom::Range1d::CreateXX(0.0, 0.2);  // :89
    std::vector<dqGeom::CurvePrimitivePtr> curves;                        // :90
    constexpr int numIterations = 1000;                                   // :92
    curves.reserve(2 * static_cast<size_t>(numIterations));
    for (int i = 0; i < numIterations; ++i) {                             // :93
        double const fract =
            range1d.FractionToPoint((i + 1.0) / numIterations);           // :94
        double const halfFract = fract * 0.5;                             // :95

        dqGeom::Point3d const a = range.FractionToPoint(0.0, 0.0, 0.0);       // :97
        dqGeom::Point3d const b = range.FractionToPoint(halfFract, 0.0, 0.0); // :98
        dqGeom::Point3d const c = range.FractionToPoint(halfFract, halfFract, 0.0);  // :99
        dqGeom::Point3d const d = range.FractionToPoint(halfFract, halfFract, halfFract); // :100
        dqGeom::Point3d const e = range.FractionToPoint(fract * 0.75, fract, fract); // :101
        dqGeom::Point3d const f = range.FractionToPoint(fract, fract, fract);  // :102

        // :104——Point3d[4] 子项经 Path.create 转 LineString3d（Path.ts:48-63
        // 的 Array.isArray 分支）——DanQing 侧显式 LineString3d::create 同语义。
        curves.push_back(dqGeom::LineString3d::create({a, b, c, d}));
        // :105——Arc3d.create(center, vector0, vector90, sweep)（Arc3d.ts:426-445
        // = createColumns(v0, v90, unitCrossProductWithDefault(v0,v9,0,0,0))）。
        // DanQing FromVectors 同形（FromCrossProduct+Normalize）；退化零叉积时
        // 参考落 (0,0,0) 而 Normalize 落 (1,0,0)——本构造域 fract≥0.0002 恒
        // 非退化（v0=(0.25,0.5,0.5)·f·diag 与 v90=(0.5,0.5,0.5)·f·diag 不平行），
        // 零发散。
        curves.push_back(dqGeom::Arc3d::FromVectors(
            d, dqGeom::Vector3d::FromStartEnd(d, e), dqGeom::Vector3d::FromStartEnd(d, f),
            dqGeom::AngleSweep::FromStartEndDegrees(0.0, 90.0)));
    }
    return dqGeom::Path::Create(curves);  // :108
}

}  // namespace Gui
