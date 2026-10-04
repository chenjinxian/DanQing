// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Measure distance 工具
// Ported from: itwinjs-core core/frontend/src/tools/MeasureTool.ts
//              （MeasureDistanceTool :185-729 + MeasureLabel :63-103 +
//               Location/Segment :147-149 + adjustPoint :152-183[简化]）
//
// EQUIVALENCE（§11.10，逐条带验证法）：
//   - 数量格式化（quantityFormatter/formatsProvider/FormatterSpec，
//     :220-221/:329/:498）：DanQing 无量纲格式化系统——恒米制 4 位小数
//     （"12.3456 m"，参考 DefaultToolsUnits.LENGTH 缺省 Units.M 的 de facto
//     形态）；验证法 = MeasureToolTest 距离标签串断言。
//   - adjustPoint（:152-183——accuSnap.currHit + computeDisplayTransform 的
//     模型显示变换逆映射）：DanQing 无 computeDisplayTransform/currHit 面
//     ——adjustedPoint 恒等于 point（参考在无命中分支的同一返回 :155-156）；
//     验证法 = 距离/斜率数学锁（恒等调整下 distance==|end-start|）。
//   - 参考轴面（getReferenceAxes :633-638——isContextRotationRequired/
//     getAuxCoordRotation + snap primitive 法线/切线轴序 :648-678）：
//     DanQing 无 ACS 旋转消费面/snap 几何细节面——refAxes 恒 identity
//     （参考 undefined vp 分支的同一返回 :635-637）；验证法同上。
//   - MeasureMarker（:106-144——Marker 子类：canvas 圆点 + pick +
//     onMouseButton 选中态 + tooltip；选中触发 displayDelta :430-431）：
//     DanQing CanvasDecoration 无 pick/鼠标反馈面（W4 登记 mouse-transparent）
//     ——marker 画视觉等价圆点（黑边 rgba(255,255,255,.5)），选中态/displayDelta/
//     marker tooltip 不可达；验证法 = 像素锁（线段+标签上屏，选中面缺席）。
//   - transientIds（:446——snap 几何的 pick id）：DanQing 无瞬态 id 池——
//     _snapGeomId 恒 0（TestDecorationHit 恒 false）。
//   - getDecorationGeometry（:310-315——IModelJson PointString3d 瓦 snap
//     几何流）：DanQing 无 IModelJson——不实现（snap 到测量点的几何面缺席）。
//   - 悬浮 tooltip（getMarkerToolTip :503-566 HTMLElement）→ 段数据在
//     reportMeasurements 的消息面输出（distance/slope 保真；Start/End/Delta
//     坐标串随消息面扩）。
#pragma once

#include "Export.h"
#include "Decorator.h"
#include "ToolAdmin.h"
#include "ToolAssistance.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>
#include <dqRender/CanvasDecoration.h>

#include <string>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

class DecorateContext;
class Viewport;

// MeasureLabel — 距离标签 canvas 装饰。
// Ported from: MeasureTool.ts MeasureLabel (:63-103).
class DQ_APP_EXPORT MeasureLabel {
public:
    MeasureLabel(dqGeom::Point3d const& worldLocation, std::string label);

    // Ported from: MeasureLabel.setPosition (:93-97 — worldToView +
    // pixelsFromInches(0.44) 上偏 + 视域包含门)。
    bool setPosition(Viewport& vp);
    // Ported from: MeasureLabel.addDecoration (:99-102).
    void addDecoration(DecorateContext& context);

    // :66 label（消息面/reportMeasurements 消费）。
    std::string const& label() const noexcept { return m_label; }
    dqGeom::Point3d const& worldLocation() const noexcept
    {
        return m_worldLocation;
    }

private:
    // Ported from: MeasureLabel.drawDecoration (:73-91 — 16px sans-serif 半
    // 透明黑底白边白字居中标签)。
    void drawDecoration(dqRender::CanvasContext& ctx) const;

    dqGeom::Point3d m_worldLocation;
    dqGeom::Point3d m_position;
    std::string m_label;
};

// Location / Segment — 收点/已收段数据。
// Ported from: MeasureTool.ts :147-149.
struct MeasureLocation {
    dqGeom::Point3d point;
    dqGeom::Point3d adjustedPoint;
    dqGeom::Matrix3d refAxes;
};

struct MeasureSegment {
    double distance = 0.0;
    double slope = 0.0;
    dqGeom::Point3d start;
    dqGeom::Point3d end;
    dqGeom::Vector3d delta;
    dqGeom::Point3d adjustedStart;
    dqGeom::Point3d adjustedEnd;
    dqGeom::Vector3d adjustedDelta;
    dqGeom::Matrix3d refAxes;
    dqGeom::Point3d markerWorldLocation;  // :596 start.interpolate(0.5, end)
};

// MeasureDistanceTool — 点列测距（收点 → 段接受 → 累计标签）。
// Ported from: MeasureTool.ts MeasureDistanceTool (:185-729).
class DQ_APP_EXPORT MeasureDistanceTool : public PrimitiveTool {
public:
    // Ported from: MeasureDistanceTool.toolId (:186).
    const char* getToolId() const noexcept override { return "Measure.Distance"; }
    // Ported from: keyin（CoreTools.json tools.Measure.Distance.keyin）。
    std::string englishKeyin() const override { return "measure distance"; }

    // Ported from: requireWriteableTarget (:216).
    bool requireWriteableTarget() const override { return false; }

    // Ported from: onPostInstall (:218-232 — formatter 面 EQUIVALENCE 恒米制
    // → 无异步装载；setupAndPromptForNextAction)。
    void onPostInstall() override;
    // Ported from: onUnsuspend (:245).
    void onUnsuspend() override;

    // Ported from: onDataButtonDown (:641-688 — 收点 + ctrl 分流 + 接受)。
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;
    // Ported from: onResetButtonUp (:691-701 — 空位置→Restart；否则接受)。
    EventHandled onResetButtonUp(BeButtonEvent const& ev) override;
    // Ported from: onUndoPreviousStep (:704-721 — location/segment 逆序弹)。
    bool onUndoPreviousStep() override;
    // Ported from: onMouseMotion (:463-476 — 动态尾点跟随)。
    void onMouseMotion(BeButtonEvent const& ev) override;

    // --- 断言/宿主面 ---
    std::vector<MeasureLocation> const& locationData() const noexcept
    {
        return m_locationData;
    }
    std::vector<MeasureSegment> const& acceptedSegments() const noexcept
    {
        return m_acceptedSegments;
    }
    double totalDistance() const noexcept { return m_totalDistance; }
    // 累计标签（updateTotals 产物——无段时 nullopt）。
    std::optional<MeasureLabel> const& totalDistanceMarker() const noexcept
    {
        return m_totalDistanceMarker;
    }

    // 段距离/斜率计算（acceptNewSegments :580-594 的数学面——测试直驱）。
    static void computeSegment(MeasureSegment& out,
                               MeasureLocation const& from,
                               MeasureLocation const& to);

    // 测试面（protected getSnapPoints 的公开观察口）。
    std::optional<std::vector<dqGeom::Point3d>> getSnapPointsTest() const
    {
        return getSnapPoints();
    }

protected:
    // Ported from: showPrompt (:248-274 — FirstPoint/NextPoint + Restart/
    // Cancel/AdditionalPoint/Ctrl+Z 指令段；touch 段无输入面略)。
    void showPrompt();
    // Ported from: setupAndPromptForNextAction (:277-285 — 光标面 stub)。
    void setupAndPromptForNextAction();
    // Ported from: acceptNewSegments (:578-632 — 段构造 + updateTotals)。
    void acceptNewSegments();
    // Ported from: updateTotals (:486-501 — 累计 + 标签 + report)。
    void updateTotals();
    // Ported from: reportMeasurements (:478-484 — Sticky 消息面)。
    void reportMeasurements();
    // Ported from: createDecorations (:385-455 — 动态线/隐藏线 + 已收段线 +
    // 累计标签 + snap 点列；marker/选中面 EQUIVALENCE 见文件头)。
    void createDecorations(DecorateContext& context, bool isSuspended);
    // Ported from: displayDynamicDistance (:319-332 — 动态段距离标签)。
    void displayDynamicDistance(DecorateContext& context,
                                std::vector<dqGeom::Point3d> const& points,
                                std::vector<dqGeom::Point3d> const& adjustedPoints);
    // Ported from: getSnapPoints (:291-307)。
    std::optional<std::vector<dqGeom::Point3d>> getSnapPoints() const;

    // :189-199 载体。
    std::vector<MeasureLocation> m_locationData;
    std::vector<MeasureSegment> m_acceptedSegments;
    double m_totalDistance = 0.0;
    std::optional<MeasureLabel> m_totalDistanceMarker;
    bool m_haveLastMotion = false;
    dqGeom::Point3d m_lastMotionPt;
    dqGeom::Point3d m_lastMotionAdjustedPt;
};

END_DQ_APP_NAMESPACE
