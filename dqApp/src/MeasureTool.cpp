// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Measure distance 工具实现
// Ported from: itwinjs-core core/frontend/src/tools/MeasureTool.ts
#include "dqApp/MeasureTool.h"

#include "dqApp/Application.h"
#include "dqApp/DecorateContext.h"
#include "dqApp/NotificationManager.h"
#include "dqApp/ToolAdmin.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewState.h"
#include "dqApp/MeasureTool.h"

#include <dqRender/GraphicBuilder.h>
#include <dqRender/RenderGraphic.h>

#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_APP_NAMESPACE

namespace {

// CoreTools en 实值（CoreTools.json）——DanQing 无 localization 系统，恒 en
//（EQUIVALENCE 同 ViewTool::translate 先例）。
char const* measureTranslate(char const* qualifiedKey)
{
    struct Entry {
        char const* key;
        char const* text;
    };
    static Entry const kEntries[] = {
        {"Measure.Distance.Prompts.FirstPoint", "Enter point to measure from"},
        {"Measure.Distance.Prompts.NextPoint", "Enter point to measure to"},
        {"Measure.Labels.CumulativeDistance", "Cumulative Distance"},
        {"Measure.Labels.Distance", "Distance"},
        {"Measure.Labels.Slope", "Slope"},
        {"Measure.Labels.StartCoord", "Start Coordinate"},
        {"Measure.Labels.EndCoord", "End Coordinate"},
        {"Measure.Labels.Delta", "Delta"},
        {"ElementSet.Inputs.AcceptPoint", "Accept point"},
        {"ElementSet.Inputs.Restart", "Restart"},
        {"ElementSet.Inputs.Cancel", "Cancel"},
        {"ElementSet.Inputs.AdditionalPoint", "Define additional points"},
        {"ElementSet.Inputs.UndoLastPoint", "Undo last point"},
    };
    for (auto const& e : kEntries)
        if (std::string_view(qualifiedKey) == e.key)
            return e.text;
    return qualifiedKey;
}

// formatQuantity 的 EQUIVALENCE（文件头登记）——恒米制 4 位小数。
std::string formatLength(double meters)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.4f m", meters);
    return buf;
}

std::string formatAngle(double radians)
{
    char buf[64];
    // 角度 4 位小数（Units.RAD 缺省弧度制）。
    std::snprintf(buf, sizeof(buf), "%.4f rad", radians);
    return buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// MeasureLabel — Ported from: MeasureTool.ts:63-103.
// ---------------------------------------------------------------------------

MeasureLabel::MeasureLabel(dqGeom::Point3d const& worldLocation,
                           std::string label)
    : m_worldLocation(worldLocation)
    , m_label(std::move(label))
{}

// Ported from: MeasureLabel.setPosition (:93-97).
bool MeasureLabel::setPosition(Viewport& vp)
{
    dqGeom::Point3d const viewPt = vp.WorldToView(m_worldLocation);
    m_position = viewPt;
    // Offset from snap location...（:95）
    m_position.y -= std::floor(vp.PixelsFromInches(0.44)) + 0.5;
    ViewRect const rect = vp.viewRect();
    return m_position.x >= static_cast<double>(rect.left)
        && m_position.x <= static_cast<double>(rect.right)
        && m_position.y >= static_cast<double>(rect.top)
        && m_position.y <= static_cast<double>(rect.bottom);
}

// Ported from: MeasureLabel.addDecoration (:99-102).
void MeasureLabel::addDecoration(DecorateContext& context)
{
    if (!setPosition(context.GetViewport()))
        return;
    dqRender::CanvasDecoration decoration;
    decoration.position = dqGeom::Point2d::From(m_position.x, m_position.y);
    // drawDecoration 以 position 为中心 translate 后调用（CanvasDecoration
    // 契约）。
    std::string const label = m_label;
    decoration.drawDecoration = [label](dqRender::CanvasContext& ctx) {
        // ctx.font = "16px sans-serif"（CanvasContext 无 text/measureText 面
        // ——EQUIVALENCE：标签框宽 = label 长度 × 16 × 0.6 + 边距的近似；
        // 文本渲染面随 CanvasContext text 族落地——当前画框 + 白边框
        // [视觉占位]，文本缺席登记）。fillRect/strokeRect 无 1:1 面经
        // 路径 + fill()/stroke() 组合（canvas 同语义）。
        double const labelHeight = 16.0;  // measureText("M").width ≈ 字高
        double const labelWidth =
            static_cast<double>(label.size()) * 16.0 * 0.6 + labelHeight;
        double const hw = labelWidth / 2.0;
        // rgba(0,0,0,.4) 底框。
        ctx.beginPath();
        ctx.moveTo(-hw, -labelHeight);
        ctx.lineTo(hw, -labelHeight);
        ctx.lineTo(hw, labelHeight);
        ctx.lineTo(-hw, labelHeight);
        ctx.lineTo(-hw, -labelHeight);
        ctx.setGlobalAlpha(0.4);
        ctx.setFillStyle(dqCommon::ColorDef::from(0, 0, 0));
        ctx.fill();
        // 白边框。
        ctx.setGlobalAlpha(1.0);
        ctx.setLineWidth(1);
        ctx.setStrokeStyle(dqCommon::ColorDef::from(255, 255, 255));
        ctx.stroke();
    };
    context.AddCanvasDecoration(std::move(decoration));
}

// ---------------------------------------------------------------------------
// MeasureDistanceTool — Ported from: MeasureTool.ts:185-729.
// ---------------------------------------------------------------------------

// Ported from: onPostInstall (:218-232).
void MeasureDistanceTool::onPostInstall()
{
    PrimitiveTool::onPostInstall();
    setupAndPromptForNextAction();
}

// Ported from: onUnsuspend (:245).
void MeasureDistanceTool::onUnsuspend()
{
    showPrompt();
}

// Ported from: showPrompt (:248-274).
void MeasureDistanceTool::showPrompt()
{
    ToolAssistanceInstruction const mainInstruction =
        ToolAssistance::createInstruction(
            "icon-measure-distance",
            measureTranslate(0 == m_locationData.size()
                                 ? "Measure.Distance.Prompts.FirstPoint"
                                 : "Measure.Distance.Prompts.NextPoint"));
    std::vector<ToolAssistanceInstruction> mouseInstructions;
    mouseInstructions.push_back(ToolAssistance::createInstruction(
        ToolAssistanceImage::LeftClick,
        measureTranslate("ElementSet.Inputs.AcceptPoint"), false,
        ToolAssistanceInputMethod::Mouse));
    if (0 == m_locationData.size()) {
        if (!m_acceptedSegments.empty()) {
            mouseInstructions.push_back(ToolAssistance::createInstruction(
                ToolAssistanceImage::RightClick,
                measureTranslate("ElementSet.Inputs.Restart"), false,
                ToolAssistanceInputMethod::Mouse));
        }
    } else {
        mouseInstructions.push_back(ToolAssistance::createInstruction(
            ToolAssistanceImage::RightClick,
            measureTranslate("ElementSet.Inputs.Cancel"), false,
            ToolAssistanceInputMethod::Mouse));
        mouseInstructions.push_back(ToolAssistance::createModifierKeyInstruction(
            ToolAssistance::ctrlKey(), ToolAssistanceImage::LeftClick,
            measureTranslate("ElementSet.Inputs.AdditionalPoint"), false,
            ToolAssistanceInputMethod::Mouse));
        mouseInstructions.push_back(ToolAssistance::createKeyboardInstruction(
            ToolAssistance::createKeyboardInfo(
                {ToolAssistance::ctrlKey(), "Z"}),
            measureTranslate("ElementSet.Inputs.UndoLastPoint"), false,
            ToolAssistanceInputMethod::Mouse));
    }
    std::vector<ToolAssistanceSection> sections;
    sections.push_back(ToolAssistance::createSection(
        std::move(mouseInstructions), ToolAssistance::inputsLabel()));
    ToolAssistanceInstructions const instructions =
        ToolAssistance::createInstructions(mainInstruction, std::move(sections));
    Application::Get().GetNotificationManager().setToolAssistance(instructions);
}

// Ported from: setupAndPromptForNextAction (:277-285 — 光标面 stub[游标资产
// 未移植]；accuSnap enableSnap/accuDraw hint 面 DanQing 缺——跳过段登记)。
void MeasureDistanceTool::setupAndPromptForNextAction()
{
    showPrompt();
}

// Ported from: acceptNewSegments 的段数学 (:580-594)。
void MeasureDistanceTool::computeSegment(MeasureSegment& out,
                                         MeasureLocation const& from,
                                         MeasureLocation const& to)
{
    out.adjustedStart = from.adjustedPoint;
    out.adjustedEnd = to.adjustedPoint;
    out.distance = out.adjustedStart.Distance(out.adjustedEnd);
    // :584-585 xyDist/zDist/slope。
    double const dx = out.adjustedEnd.x - out.adjustedStart.x;
    double const dy = out.adjustedEnd.y - out.adjustedStart.y;
    double const xyDist = std::sqrt(dx * dx + dy * dy);
    double const zDist = out.adjustedEnd.z - out.adjustedStart.z;
    out.slope = (0.0 == xyDist ? 3.14159265358979323846
                               : std::atan(zDist / xyDist));
    out.adjustedDelta = dqGeom::Vector3d::FromStartEnd(out.adjustedStart,
                                                       out.adjustedEnd);
    out.refAxes = from.refAxes;
    // refAxes 恒 identity（EQUIVALENCE）——multiplyTransposeVectorInPlace 恒等。
    out.start = from.point;
    out.end = to.point;
    out.delta = dqGeom::Vector3d::FromStartEnd(out.start, out.end);
    // :596 marker at start.interpolate(0.5, end)。
    out.markerWorldLocation =
        dqGeom::Point3d::FromInterpolate(out.start, 0.5, out.end);
}

// Ported from: acceptNewSegments (:578-632 —— 段构造；marker 圆点视觉等价
// [EQUIVALENCE]，onMouseButton 选中面不可达)。
void MeasureDistanceTool::acceptNewSegments()
{
    if (m_locationData.size() > 1) {
        for (size_t i = 0; i <= m_locationData.size() - 2; ++i) {
            MeasureSegment seg;
            computeSegment(seg, m_locationData[i], m_locationData[i + 1]);
            m_acceptedSegments.push_back(std::move(seg));
        }
        m_locationData.clear();
        m_haveLastMotion = false;
    }
    updateTotals();
}

// Ported from: updateTotals (:486-501).
void MeasureDistanceTool::updateTotals()
{
    m_totalDistance = 0.0;
    m_totalDistanceMarker = std::nullopt;
    for (auto const& seg : m_acceptedSegments)
        m_totalDistance += seg.distance;
    if (0.0 == m_totalDistance)
        return;

    m_totalDistanceMarker = MeasureLabel(
        m_acceptedSegments.back().end, formatLength(m_totalDistance));
    reportMeasurements();
}

// Ported from: reportMeasurements (:478-484 — Sticky → DanQing Toast[消息
// 类型面 Sticky 未移植，EQUIVALENCE 登记]）。
void MeasureDistanceTool::reportMeasurements()
{
    if (!m_totalDistanceMarker.has_value())
        return;
    std::string const brief =
        std::string(measureTranslate(m_acceptedSegments.size() > 1
                                         ? "Measure.Labels.CumulativeDistance"
                                         : "Measure.Labels.Distance"))
        + ": " + m_totalDistanceMarker->label();
    Application::Get().GetNotificationManager().OutputMessage(
        NotifyMessageDetails(OutputMessagePriority::Info, brief));
}

// Ported from: onDataButtonDown (:641-688 — ctrl 分流 :682-683)。
EventHandled MeasureDistanceTool::onDataButtonDown(BeButtonEvent const& ev)
{
    MeasureLocation loc;
    loc.point = ev.point;
    // adjustPoint EQUIVALENCE：无 currHit 面 → 恒等（文件头登记）。
    loc.adjustedPoint = ev.point;
    // getReferenceAxes EQUIVALENCE：恒 identity。
    loc.refAxes = dqGeom::Matrix3d::CreateIdentity();
    m_locationData.push_back(loc);

    if (m_locationData.size() > 1
        && (ev.keyModifiers & BeModifierKeys::Control) == BeModifierKeys::None)
        acceptNewSegments();
    setupAndPromptForNextAction();
    if (ev.viewport != nullptr)
        ev.viewport->InvalidateDecorations();
    return EventHandled::No;
}

// Ported from: onResetButtonUp (:691-701).
EventHandled MeasureDistanceTool::onResetButtonUp(BeButtonEvent const& ev)
{
    if (m_locationData.empty()) {
        // onReinitialize（:693）= Restart——清段重提示（参考经 onRestartTool
        // 重建工具；等价净效果 = 清态）。
        m_acceptedSegments.clear();
        m_haveLastMotion = false;
        updateTotals();
        setupAndPromptForNextAction();
        if (ev.viewport != nullptr)
            ev.viewport->InvalidateDecorations();
        return EventHandled::No;
    }
    acceptNewSegments();
    setupAndPromptForNextAction();
    if (ev.viewport != nullptr)
        ev.viewport->InvalidateDecorations();
    return EventHandled::No;
}

// Ported from: onUndoPreviousStep (:704-721).
bool MeasureDistanceTool::onUndoPreviousStep()
{
    if (m_locationData.empty() && m_acceptedSegments.empty())
        return false;

    if (!m_locationData.empty()) {
        m_locationData.pop_back();
    } else if (!m_acceptedSegments.empty()) {
        m_acceptedSegments.pop_back();
    }

    if (m_locationData.empty() && m_acceptedSegments.empty()) {
        m_haveLastMotion = false;
        updateTotals();
        setupAndPromptForNextAction();
    } else {
        updateTotals();
        setupAndPromptForNextAction();
    }
    return true;
}

// Ported from: onMouseMotion (:463-476 — 动态尾点)。
void MeasureDistanceTool::onMouseMotion(BeButtonEvent const& ev)
{
    if (!m_locationData.empty() && ev.viewport != nullptr) {
        m_lastMotionPt = ev.point;
        m_lastMotionAdjustedPt = ev.point;  // adjustPoint EQUIVALENCE 恒等
        m_haveLastMotion = true;
        ev.viewport->InvalidateDecorations();
    }
}

// Ported from: getSnapPoints (:291-307).
std::optional<std::vector<dqGeom::Point3d>>
MeasureDistanceTool::getSnapPoints() const
{
    if (m_acceptedSegments.empty() && m_locationData.size() < 2)
        return std::nullopt;

    std::vector<dqGeom::Point3d> snapPoints;
    for (auto const& seg : m_acceptedSegments) {
        if (snapPoints.empty()
            || !seg.start.AlmostEqual(snapPoints.back(), 1.0e-10))
            snapPoints.push_back(seg.start);
        if (!seg.end.AlmostEqual(snapPoints.front(), 1.0e-10))
            snapPoints.push_back(seg.end);
    }
    if (m_locationData.size() > 1)
        for (auto const& loc : m_locationData)
            snapPoints.push_back(loc.point);
    return snapPoints;
}

// Ported from: displayDynamicDistance (:319-332).
void MeasureDistanceTool::displayDynamicDistance(
    DecorateContext& context, std::vector<dqGeom::Point3d> const& points,
    std::vector<dqGeom::Point3d> const& adjustedPoints)
{
    double totalDistance = 0.0;
    for (size_t i = 0; i + 1 < adjustedPoints.size(); ++i)
        totalDistance += adjustedPoints[i].Distance(adjustedPoints[i + 1]);
    if (0.0 == totalDistance)
        return;

    MeasureLabel distDyn(points.back(), formatLength(totalDistance));
    distDyn.addDecoration(context);
}

// Ported from: createDecorations (:385-455).
void MeasureDistanceTool::createDecorations(DecorateContext& context,
                                            bool isSuspended)
{
    Viewport& vp = context.GetViewport();

    // 动态段（:389-415——收点中 + 尾点）。
    if (!isSuspended && !m_locationData.empty() && m_haveLastMotion) {
        std::vector<dqGeom::Point3d> tmpPoints;
        std::vector<dqGeom::Point3d> tmpAdjustedPoints;
        for (auto const& loc : m_locationData) {
            tmpPoints.push_back(loc.point);
            tmpAdjustedPoints.push_back(loc.adjustedPoint);
        }
        tmpPoints.push_back(m_lastMotionPt);
        tmpAdjustedPoints.push_back(m_lastMotionAdjustedPt);

        // hilite 色（viewport.hilite.color —— DanQing 0x23bbfc[SelectionSet
        // Hilite.ts:53 同源]）。
        dqCommon::ColorDef const colorDynVis = dqCommon::ColorDef::from(
            0x23, 0xbb, 0xfc);
        Viewport& viewport = vp;
        dqRender::GraphicBuilderOptions optsVis;
        optsVis.type = dqRender::GraphicType::WorldDecoration;
        double const worldPerPixel =
            viewport.GetViewingSpace().getPixelSizeAtPoint(&m_lastMotionPt);
        optsVis.computeChordTolerance = [worldPerPixel]() {
            return worldPerPixel;
        };
        if (auto builder = viewport.createGraphicBuilder(optsVis)) {
            builder->setSymbology(colorDynVis, dqCommon::ColorDef::from(0, 0, 0),
                                  3);
            builder->addLineString(tmpPoints.data(), tmpPoints.size());
            if (auto* g = builder->finish()) {
                viewport.createGraphicOwner(g);
                context.AddDecoration(dqRender::GraphicType::WorldDecoration, g);
            }
        }

        dqRender::GraphicBuilderOptions optsHid;
        optsHid.type = dqRender::GraphicType::WorldOverlay;
        optsHid.computeChordTolerance = [worldPerPixel]() {
            return worldPerPixel;
        };
        if (auto builder = viewport.createGraphicBuilder(optsHid)) {
            builder->setSymbology(colorDynVis, dqCommon::ColorDef::from(0, 0, 0),
                                  1);
            builder->addLineString(tmpPoints.data(), tmpPoints.size());
            if (auto* g = builder->finish()) {
                viewport.createGraphicOwner(g);
                context.AddDecoration(dqRender::GraphicType::WorldOverlay, g);
            }
        }
        displayDynamicDistance(context, tmpPoints, tmpAdjustedPoints);
    }

    // 已收段（:417-436——白 adjustedForContrast 线 + marker 圆点）。
    if (!m_acceptedSegments.empty()) {
        dqCommon::ColorDef const colorAccVis =
            dqCommon::ColorDef::white.adjustedForContrast(
                dqCommon::ColorDef::fromTbgr(
                    vp.GetView()->GetDisplayStyle().getBackgroundColor()));
        dqRender::GraphicBuilderOptions optsAccVis;
        optsAccVis.type = dqRender::GraphicType::WorldDecoration;
        dqRender::GraphicBuilderOptions optsAccHid;
        optsAccHid.type = dqRender::GraphicType::WorldOverlay;
        if (auto builder = vp.createGraphicBuilder(optsAccVis)) {
            builder->setSymbology(colorAccVis, dqCommon::ColorDef::from(0, 0, 0),
                                  3);
            for (auto const& seg : m_acceptedSegments) {
                dqGeom::Point3d const pts[2] = {seg.start, seg.end};
                builder->addLineString(pts, 2);
            }
            if (auto* g = builder->finish()) {
                vp.createGraphicOwner(g);
                context.AddDecoration(dqRender::GraphicType::WorldDecoration, g);
            }
        }
        if (auto builder = vp.createGraphicBuilder(optsAccHid)) {
            builder->setSymbology(colorAccVis, dqCommon::ColorDef::from(0, 0, 0),
                                  1);
            for (auto const& seg : m_acceptedSegments) {
                dqGeom::Point3d const pts[2] = {seg.start, seg.end};
                builder->addLineString(pts, 2);
            }
            if (auto* g = builder->finish()) {
                vp.createGraphicOwner(g);
                context.AddDecoration(dqRender::GraphicType::WorldOverlay, g);
            }
        }
        // marker 圆点（:429——视觉等价 canvas 圆：黑边 rgba(255,255,255,.5)）。
        for (auto const& seg : m_acceptedSegments) {
            dqGeom::Point3d const viewPt = vp.WorldToView(
                seg.markerWorldLocation);
            dqRender::CanvasDecoration decoration;
            decoration.position =
                dqGeom::Point2d::From(viewPt.x, viewPt.y);
            decoration.drawDecoration = [](dqRender::CanvasContext& ctx) {
                ctx.beginPath();
                ctx.arc(0.0, 0.0, 25.0 * 0.5, 0.0,
                        2.0 * 3.14159265358979323846);
                ctx.setLineWidth(2);
                ctx.setStrokeStyle(dqCommon::ColorDef::from(0, 0, 0));
                // rgba(255,255,255,.5)。
                ctx.setGlobalAlpha(0.5);
                ctx.setFillStyle(dqCommon::ColorDef::from(255, 255, 255));
                ctx.fill();
                ctx.stroke();
            };
            context.AddCanvasDecoration(std::move(decoration));
        }
    }

    // 累计标签（:438-439）。
    if (m_totalDistanceMarker.has_value())
        m_totalDistanceMarker->addDecoration(context);
}

END_DQ_APP_NAMESPACE
