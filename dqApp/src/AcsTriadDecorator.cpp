// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ACS triad decorator implementation
// Ported from: itwinjs-core core/frontend/src/AuxCoordSys.ts:38-306
//                (ACSDisplayOptions / ACSDisplaySizes / limitRange / isOriginInView /
//                 getAdjustedColor / addAxisLabel / addAxis / createGraphicBuilder /
//                 display) + AccuDraw.ts:2290-2304 (decorate gate + ACS origin point).
#include "dqApp/AcsTriadDecorator.h"

#include "dqApp/DecorateContext.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewState.h"

#include <dqRender/GraphicBuilder.h>
#include <dqRender/RenderGraphic.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/Npc.h>
#include <dqCommon/ViewFlags.h>

#include <dqGeom/AngleSweep.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

namespace dqApp {
namespace {

// Ported from: itwinjs-core AuxCoordSys.ts:38-45 (ACSDisplayOptions)。
enum class ACSDisplayOptions : uint32_t {
    None = 0,
    Active = 1 << 0,
    Deemphasized = 1 << 1,
    Hilite = 1 << 2,
    CheckVisible = 1 << 3,
    Dynamics = 1 << 4,
};
constexpr ACSDisplayOptions operator|(ACSDisplayOptions a, ACSDisplayOptions b)
{
    return static_cast<ACSDisplayOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr bool hasOption(ACSDisplayOptions v, ACSDisplayOptions bit)
{
    return (static_cast<uint32_t>(v) & static_cast<uint32_t>(bit))
        != static_cast<uint32_t>(ACSDisplayOptions::None);
}

// Ported from: itwinjs-core AuxCoordSys.ts:47-59 (ACSDisplaySizes).
constexpr double kTriadSizeInches = 0.6;   // TriadSizeInches
constexpr double kArrowBaseStart  = 0.3;   // ArrowBaseStart
constexpr double kArrowBaseWidth  = 0.2;   // ArrowBaseWidth
constexpr double kArrowTipEnd     = 1.25;  // ArrowTipEnd
constexpr double kArrowTipStart   = 0.85;  // ArrowTipStart
constexpr double kArrowTipFlange  = 0.75;  // ArrowTipFlange
constexpr double kArrowTipWidth   = 0.4;   // ArrowTipWidth
constexpr double kZAxisLength     = 0.65;  // ZAxisLength
constexpr double kLabelStart      = 0.4;   // LabelStart
constexpr double kLabelEnd        = 0.8;   // LabelEnd
constexpr double kLabelWidth      = 0.15;  // LabelWidth

// Ported from: itwinjs-core AuxCoordSys.ts:130 — clamp to [min, max].
double limitRange(double min, double max, double val)
{
    return std::max(min, std::min(max, val));
}

// Ported from: itwinjs-core AuxCoordSys.ts:136-165 (isOriginInView)。
// adjustOrigin=true 时把视外的 drawOrigin 钳回视口内（边距 TriadSizeInches 像素），
// 返回"原点本来是否在视内"（钳制仍执行）。
bool isOriginInView(dqGeom::Point3d& drawOrigin, Viewport const& vp, bool adjustOrigin)
{
    dqGeom::Point3d testPtView = vp.WorldToView(drawOrigin);              // :137
    dqCommon::Frustum const frustum = vp.getFrustum(false);               // :138 CoordSystem.View
    dqGeom::Point3d screenRange;                                          // :139-142
    screenRange.x = frustum.points[static_cast<int>(dqCommon::Npc_000)]
        .Distance(frustum.points[static_cast<int>(dqCommon::Npc_100)]);
    screenRange.y = frustum.points[static_cast<int>(dqCommon::Npc_000)]
        .Distance(frustum.points[static_cast<int>(dqCommon::Npc_010)]);
    screenRange.z = frustum.points[static_cast<int>(dqCommon::Npc_000)]
        .Distance(frustum.points[static_cast<int>(dqCommon::Npc_001)]);

    // Check if current acs origin is outside view... (:145)
    bool const inView = !((testPtView.x < 0 || testPtView.x > screenRange.x)
                       || (testPtView.y < 0 || testPtView.y > screenRange.y));

    if (!adjustOrigin)                                                    // :147-148
        return inView;

    if (!inView) {                                                        // :150-154
        double const offset = vp.PixelsFromInches(kTriadSizeInches);      // :151
        testPtView.x = limitRange(offset, screenRange.x - offset, testPtView.x);  // :152
        testPtView.y = limitRange(offset, screenRange.y - offset, testPtView.y);  // :153
    }

    // Limit point to NPC box to prevent triad from being clipped from display... (:156-162)
    dqGeom::Point3d originPtNpc = vp.ViewToNpc(testPtView);               // :157
    originPtNpc.x = limitRange(0.0, 1.0, originPtNpc.x);                  // :158
    originPtNpc.y = limitRange(0.0, 1.0, originPtNpc.y);                  // :159
    originPtNpc.z = limitRange(0.0, 1.0, originPtNpc.z);                  // :160
    testPtView = vp.NpcToView(originPtNpc);                               // :161
    drawOrigin = vp.ViewToWorld(testPtView);                              // :162

    return inView;                                                        // :164
}

// Ported from: itwinjs-core AuxCoordSys.ts:167-185 (getAdjustedColor)。
dqCommon::ColorDef getAdjustedColor(dqCommon::ColorDef inColor, bool isFill,
                                    Viewport const& vp, ACSDisplayOptions options,
                                    dqCommon::ColorDef bgColor)
{
    dqCommon::ColorDef color = dqCommon::ColorDef::create();
    if (hasOption(options, ACSDisplayOptions::Hilite)) {
        // :169-170 — viewport.hilite.color。AccuDraw 门控路径（CheckVisible|Active）
        // 恒不含 Hilite，此分支当前不可达；Hilite.Settings 状态随 AccuDraw 移植落地。
        color = inColor.equals(dqCommon::ColorDef::white)
            ? vp.getContrastToBackgroundColor() : inColor;  // 占位（不可达，见上）
    } else if (hasOption(options, ACSDisplayOptions::Active)) {
        color = inColor.equals(dqCommon::ColorDef::white)                 // :172
            ? vp.getContrastToBackgroundColor() : inColor;
    } else {
        color = dqCommon::ColorDef::from(150, 150, 150, 0);               // :174
    }

    color = color.adjustedForContrast(bgColor);                           // :177

    if (isFill)                                                           // :179-180
        color = color.withTransparency(
            hasOption(options, ACSDisplayOptions::Deemphasized | ACSDisplayOptions::Dynamics) ? 225 : 200);
    else                                                                  // :181-182
        color = color.withTransparency(
            hasOption(options, ACSDisplayOptions::Deemphasized) ? 150 : 75);

    return color;
}

// Ported from: itwinjs-core AuxCoordSys.ts:187-212 (addAxisLabel)。The X/Y glyphs
// are line segments (no text/font). axis 2 (Z) has no label glyph in the reference.
// labelColor is the adjusted white→contrast color (:188-189, NOT the axis color —
// otherwise the X glyph is red-on-red and invisible)。
void addAxisLabel(dqRender::GraphicBuilder& builder, int axis, ACSDisplayOptions options,
                  Viewport const& vp, dqCommon::ColorDef bgColor)
{
    dqCommon::ColorDef const labelColor =
        getAdjustedColor(dqCommon::ColorDef::white, false, vp, options, bgColor);  // :188-189
    builder.setSymbology(labelColor, labelColor, 2);                        // :190 weight 2

    if (axis == 0) {
        // "X": two diagonals (:193-195 + :203-205)。
        dqGeom::Point3d linePts1[2] = { {kLabelStart, -kLabelWidth, 0.0}, {kLabelEnd, kLabelWidth, 0.0} };
        builder.addLineString(linePts1, 2);
        // NOTE: Don't use same point array, addPointString/addLineString don't deep copy... (:202)
        dqGeom::Point3d linePts2[2] = { {kLabelStart,  kLabelWidth, 0.0}, {kLabelEnd, -kLabelWidth, 0.0} };
        builder.addLineString(linePts2, 2);
    } else {
        // "Y": vertical stub + 3-point fork (:197-199 + :207-210)。
        double const forkBase = (kLabelStart + kLabelEnd) * 0.5;          // :198/208 (0.6)
        dqGeom::Point3d linePts1[2] = { {0.0, kLabelStart, 0.0}, {0.0, forkBase, 0.0} };
        builder.addLineString(linePts1, 2);
        dqGeom::Point3d linePts2[3] = { { kLabelWidth, kLabelEnd, 0.0},
                                        {0.0,          forkBase,  0.0},
                                        {-kLabelWidth, kLabelEnd, 0.0} };
        builder.addLineString(linePts2, 3);
    }
}

// Ported from: itwinjs-core AuxCoordSys.ts:214-268 (addAxis)。
// placementMatrix: 参考经 builder.placement.matrix 读（:235-236）——DanQing GraphicBuilder
// 不暴露 placement，由 createGraphicBuilder 调用点按值传入（同一矩阵）。
void addAxis(dqRender::GraphicBuilder& builder, int axis, ACSDisplayOptions options,
             Viewport const& vp, dqCommon::ColorDef bgColor,
             dqGeom::Matrix3d const& placementMatrix)
{
    // :215 — axis → color。
    dqCommon::ColorDef const base = (axis == 0) ? dqCommon::ColorDef::red
                                  : (axis == 1) ? dqCommon::ColorDef::green
                                                : dqCommon::ColorDef::blue;
    dqCommon::ColorDef const lineColor = getAdjustedColor(base, false, vp, options, bgColor);  // :216
    dqCommon::ColorDef const fillColor = getAdjustedColor(base, true,  vp, options, bgColor);  // :217

    if (axis == 2) {                                                      // :219
        // Z 轴：tip point → stem → 原点圆盘（参考顺序；pass 内按 renderOrder 排序，
        // BlankingRegion(2) < Linear(5) → 盘先画、点在上）。
        builder.setSymbology(lineColor, lineColor, 6);                    // :220
        dqGeom::Point3d tip = {0.0, 0.0, kZAxisLength};
        builder.addPointString(&tip, 1);                                  // :221
        // NOTE: ACS origin point will be drawn separately as a pickable world decoration...

        // stem：weight 1，LinePixels.Solid（:225 Dynamics→Code2，本路径无 Dynamics）。
        builder.setSymbology(lineColor, lineColor, 1);                    // :225
        dqGeom::Point3d linePts2[2] = { {0.0, 0.0, 0.0}, {0.0, 0.0, kZAxisLength} };  // :223-224
        builder.addLineString(linePts2, 2);                               // :226

        // Z out-of-screen filled disc at the origin (:228-246)。
        double const scale = kArrowTipWidth / 2.0;                        // :228
        dqGeom::Point3d const center{0.0, 0.0, 0.0};                      // :229
        dqGeom::Matrix3d const viewRMatrix = vp.getRotation();            // :230

        dqGeom::Vector3d xVec = viewRMatrix.RowX();                       // :232
        dqGeom::Vector3d yVec = viewRMatrix.RowY();                       // :233

        // builder.placement.matrix.multiplyTransposeVectorInPlace(xVec/yVec) (:235-236)
        xVec = placementMatrix.MultiplyTransposeVector(xVec);
        yVec = placementMatrix.MultiplyTransposeVector(yVec);

        xVec.Normalize();                                                 // :238
        yVec.Normalize();                                                 // :239

        // :241 — Arc3d.createScaledXYColumns(center, columns(xVec, yVec, 0), scale, scale, 全圆)
        dqGeom::Vector3d const v0{xVec.x * scale, xVec.y * scale, xVec.z * scale};
        dqGeom::Vector3d const v90{yVec.x * scale, yVec.y * scale, yVec.z * scale};
        auto ellipse = dqGeom::Arc3d::FromVectors(center, v0, v90, dqGeom::AngleSweep::FullCircle());
        if (ellipse) {
            builder.addArc(*ellipse, false, false);                       // :242
            builder.setBlankingFill(fillColor);                           // :244
            builder.addArc(*ellipse, true, true);                         // :245
        }
        return;                                                           // :246
    }

    // X/Y 箭头剪影（7 顶点 + 闭合，:249-259）。
    dqGeom::Point3d shapePts[8] = {
        {kArrowTipEnd,    0.0,             0.0},  // :250
        {kArrowTipFlange,  kArrowTipWidth, 0.0},  // :251
        {kArrowTipStart,   kArrowBaseWidth, 0.0}, // :252
        {kArrowBaseStart,  kArrowBaseWidth, 0.0}, // :253
        {kArrowBaseStart, -kArrowBaseWidth, 0.0}, // :254
        {kArrowTipStart,  -kArrowBaseWidth, 0.0}, // :255
        {kArrowTipFlange, -kArrowTipWidth, 0.0},  // :256
        {kArrowTipEnd,    0.0,             0.0},  // :257 (= shapePts[0])
    };
    if (axis == 1) {                                                      // :258-259 — swap x<->y
        for (auto& p : shapePts)
            p = dqGeom::Point3d{p.y, p.x, p.z};
    }

    builder.setSymbology(lineColor, lineColor, 1);                        // :261 (Solid)
    builder.addLineString(shapePts, 8);                                   // :262

    addAxisLabel(builder, axis, options, vp, bgColor);                    // :264

    builder.setBlankingFill(fillColor);                                   // :266
    builder.addShape(shapePts, 8);                                        // :267
}

}  // namespace

AcsTriadDecorator::~AcsTriadDecorator()
{
    // NOTE: do NOT call disposeGraphic here. The graphic was created by a
    // per-viewport RenderSystem that may already be destroyed at app shutdown
    // (viewports are torn down via ShutdownAll before the ViewManager dies).
    // disposeGraphic would touch freed GL resources and crash. The per-frame
    // dispose in Decorate handles within-session cleanup; at process exit the
    // OS reclaims the last graphic. (~RenderGraphicOwner is a no-op — no GL
    // touch — so `delete` here only frees the owner struct.)
    delete m_graphicOwner;
    m_graphicOwner = nullptr;
    delete m_pickOwner;
    m_pickOwner = nullptr;
}

void AcsTriadDecorator::Decorate(DecorateContext& context)
{
    auto& vp = context.GetViewport();
    auto* view = vp.GetView();
    if (!view || !view->getViewFlags().acsTriad())
        return;  // ← AccuDraw.decorate gate (AccuDraw.ts:2291)

    // ← AccuDraw.ts:2292 — display(context, CheckVisible | Active)。
    ACSDisplayOptions options = ACSDisplayOptions::CheckVisible | ACSDisplayOptions::Active;

    // Background color for adjustedForContrast (:177 读 viewport.view.backgroundColor)。
    dqCommon::ColorDef const bgColor = dqCommon::ColorDef::fromTbgr(
        view->GetDisplayStyle().getBackgroundColor());

    // Per-decoration rebuild — 1:1 with itwinjs AuxCoordSystemState.display
    // (AuxCoordSys.ts:301-305: createGraphicBuilder + addDecorationFromBuilder
    // every decorate, no caching). Safe because (a) MeshGraphic now releases GL
    // on disposal, and (b) Viewport::CollectDecorations clears m_decorations
    // (non-owning GraphicList, Decorations.h:66-87) BEFORE invoking decorators,
    // so the previous graphics are unreferenced when we dispose them. ViewTool
    // invalidates decorations on zoom/rotate/pan (ViewTool.cpp:290/631/648 + the
    // InvalidateRenderPlan→InvalidateScene→InvalidateDecorations cascade), so
    // Decorate re-runs with the new camera and the placement re-bakes → triad
    // keeps ~0.6" screen size, disc stays circular.
    // Cross-viewport guard (TD-12, 2026-09-23): the cached graphics belong
    // to the RenderSystem that created them. If the current viewport's system
    // differs (previous test's viewport shut down its driver), disposeGraphic
    // would touch a dead driver → SEH. Drop pointers without disposing
    // instead (dead driver's GL resources reclaimed with its context).
    dqRender::RenderSystem* currentSystem = vp.renderSystem();
    bool const sameSystem = (m_creatingSystem == currentSystem);
    if (m_graphicOwner) {
        if (sameSystem)
            m_graphicOwner->disposeGraphic();  // MeshGraphic dtors free live GL
        delete m_graphicOwner;                 // ~RenderGraphicOwner is a no-op
        m_graphicOwner = nullptr;
        m_graphic = nullptr;
    }
    if (m_pickOwner) {
        if (sameSystem)
            m_pickOwner->disposeGraphic();
        delete m_pickOwner;
        m_pickOwner = nullptr;
        m_pickGraphic = nullptr;
    }
    m_creatingSystem = currentSystem;

    // ------------------------------------------------------------------
    // createGraphicBuilder (AuxCoordSys.ts:271-299)
    // ------------------------------------------------------------------
    bool const checkOutOfView = hasOption(options, ACSDisplayOptions::CheckVisible);  // :272
    dqGeom::Point3d drawOrigin{0.0, 0.0, 0.0};  // :273 this.getOrigin()（世界原点 ACS）

    if (checkOutOfView && !isOriginInView(drawOrigin, vp, true))          // :275
        options = options | ACSDisplayOptions::Deemphasized;              // :276

    double pixelSize = vp.PixelsFromInches(kTriadSizeInches);             // :278

    if (hasOption(options, ACSDisplayOptions::Deemphasized))              // :280-281
        pixelSize *= 0.8;
    else if (hasOption(options, ACSDisplayOptions::Active))               // :282-283
        pixelSize *= 0.9;

    double const exaggerate = view->getAspectRatioSkew();                 // :285
    double const scale = vp.GetViewingSpace().getPixelSizeAtPoint(&drawOrigin) * pixelSize;  // :286
    // TEMP-DIAG：深缩放 triad 消失排查（zoom 10→40 步之间 FBO 失去 triad）。
    // 门控 DANQING_GL_TRACE（轻量打印）——OIT_DUMP 的帧尾 readback 有 observer
    // effect（滚轮动画被拖至冻结，2026-09-14 实测），不适合动态过程取证。
    if (std::getenv("DANQING_GL_TRACE")) {
        auto const* v3 = view->AsViewState3d();
        std::fprintf(stderr, "[ACSDIAG] px@origin=%.6g pixelSize=%.1f scale=%.6g ext=(%.6g,%.6g) originView=(%.1f,%.1f)\n",
                     vp.GetViewingSpace().getPixelSizeAtPoint(&drawOrigin), pixelSize, scale,
                     v3 ? v3->GetExtents().x : -1.0, v3 ? v3->GetExtents().y : -1.0,
                     vp.GetViewingSpace().WorldToView(drawOrigin).x,
                     vp.GetViewingSpace().WorldToView(drawOrigin).y);
    }
    dqGeom::Matrix3d rMatrix = dqGeom::Matrix3d::CreateIdentity();        // :287 this.getRotation()（世界原点 ACS → I）

    rMatrix.TransposeInPlace();                                           // :289
    // :290 — rMatrix.scaleColumns(scale, scale / exaggerate, scale, rMatrix)
    for (int r = 0; r < 3; ++r) {
        rMatrix.coffs[r * 3 + 0] *= scale;
        rMatrix.coffs[r * 3 + 1] *= scale / exaggerate;
        rMatrix.coffs[r * 3 + 2] *= scale;
    }
    dqGeom::Transform const placement = dqGeom::Transform::CreateOriginAndMatrix(drawOrigin, rMatrix);  // :291

    dqRender::GraphicBuilderOptions opts;
    opts.type = dqRender::GraphicType::WorldOverlay;                      // :293
    opts.placement = placement;
    // GraphicBuilder.finish() returns null unless a computeChordTolerance closure
    // is supplied (PrimitiveBuilder uses it as the LOD/tessellation gate). The
    // viewport-based factory returns the pixel size at the draw point — do the same.
    double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(&drawOrigin);
    opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
    auto builder = vp.createGraphicBuilder(opts);
    if (!builder)
        return;

    addAxis(*builder, 0, options, vp, bgColor, rMatrix);  // :295 X red
    addAxis(*builder, 1, options, vp, bgColor, rMatrix);  // :296 Y green
    addAxis(*builder, 2, options, vp, bgColor, rMatrix);  // :297 Z blue

    // ------------------------------------------------------------------
    // display (AuxCoordSys.ts:301-305) → addDecorationFromBuilder
    // ------------------------------------------------------------------
    m_graphic = builder->finish();
    if (m_graphic) {
        // Wrap in a GraphicOwner so ~AcsTriadDecorator can release it at shutdown.
        // The graphic itself is disposed + rebuilt every Decorate (above); the owner
        // is just the handle the dtor deletes (without touching GL — see dtor).
        m_graphicOwner = vp.createGraphicOwner(m_graphic);
        context.AddDecoration(dqRender::GraphicType::WorldOverlay, m_graphic);
    }

    // ------------------------------------------------------------------
    // ACS origin point（AccuDraw.ts:2293-2299）——pickable world decoration。
    // 参考用 transientIds 取 pick id 建 WorldDecoration builder；DanQing 的
    // GraphicBuilderOptions 尚无 pickable/transientIds 接线（TODO 随 AccuDraw/
    // ElementLocate 移植）——此处产出**视觉等价**的蓝点（同 builder/同色/同 weight），
    // 不注册 pick id（TestDecorationHit 恒 false 至接线完成）。
    // ------------------------------------------------------------------
    {
        dqRender::GraphicBuilderOptions pickOpts;
        pickOpts.type = dqRender::GraphicType::WorldDecoration;           // :2295
        // placement undefined → identity（:2295 第二参）
        pickOpts.computeChordTolerance = opts.computeChordTolerance;
        auto pickBuilder = vp.createGraphicBuilder(pickOpts);
        if (pickBuilder) {
            // :2296 — ColorDef.blue.adjustedForContrast(view.backgroundColor, 50)。
            dqCommon::ColorDef const color =
                dqCommon::ColorDef::blue.adjustedForContrast(bgColor, 50);
            pickBuilder->setSymbology(color, color, 6);                   // :2297
            dqGeom::Point3d const acsOrigin{0.0, 0.0, 0.0};               // :2298 acs.getOrigin()
            pickBuilder->addPointString(&acsOrigin, 1);
            m_pickGraphic = pickBuilder->finish();                        // :2299 addDecorationFromBuilder
            if (m_pickGraphic) {
                m_pickOwner = vp.createGraphicOwner(m_pickGraphic);
                context.AddDecoration(dqRender::GraphicType::WorldDecoration, m_pickGraphic);
            }
        }
    }

    // ← AccuDraw.ts:2303 — flags.redrawCompass = false（compass 未移植，无可清状态）。
}

}  // namespace dqApp
