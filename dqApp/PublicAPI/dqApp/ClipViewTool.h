// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewClip 工具族（五种 clip 定义 + Clear）
// Ported from: itwinjs-core core/frontend/src/tools/ClipViewTool.ts
//              ViewClipEventHandler (:37-51) + ViewClipTool (:77-443) +
//              ViewClipClearTool (:445-480) + ViewClipByPlaneTool (:483-558) +
//              ViewClipByShapeTool (:564-800) + ViewClipByRangeTool (:805-921) +
//              ViewClipByElementTool (:926-1058)
//
// M-P P-E。§3.4/EQUIVALENCE（逐条；验证法 = ClipViewToolTest + E2E 像素锁）：
//  - targetView：DanQing PrimitiveTool 无 ToolAdmin 安装期目标视口槽（MeasureTool
//    先例——事件流以 ev.viewport 为目标视口）；工厂静态方法显式收 Viewport&。
//  - getPlaneInwardNormal 的 AccuDrawHintBuilder.getContextRotation 未移植——
//    六世界朝向以世界轴列取负承载（Top→-Z…与参考 Top 视图矩阵合同一致）；
//    View/Face 取视图旋转 Z 列负（参考 Face 语义=屏幕朝向）。EQUIVALENCE：
//    ACS 上下文锁定的旋转偏差面缺席。
//  - ToolSettings（orientation DialogItem）/supplyToolSettingsProperties/
//    applyToolSettingPropertyChange：无 ToolSettings 面板基建——orientation 以
//    公开 setter 承载（面板接线随宿主 P-G 落地）。
//  - ByShape 的 AccuDraw 投影（projectPointToPlaneInView/projectPointToLineInView
//    与 2 点自动矩形）：平面投影以法向正交投影承载（矩阵列 2 为面法向）；
//    2 点自动矩形 = 参考闭合路径的同构面。dynamics 橡皮筋经 decorate（装饰
//    绘制面与 P-F 的 drawClip 族同基建——随 P-F 接线）。
//  - ByElement 的 iModel.elements.getPlacements：M-N(2) 既有 placements 数据面
//    （DumpIModelConnection::findPlacement——instances60-placements-v1）。
//    computeDisplayTransform 无消费者（M-O(2) I11 登记）——缺席恒 nullopt。
//  - locateManager.doLocate：拾取链以拾取命中 id 直取承载（SelectionTool 先例）。
//  - ToolAssistance 提示族随各工具 showPrompt 落地（M-O(2) 3i 面已建）。
//  - drawClip/drawClipShape/drawClipPlanesLoops（装饰绘制）归 P-F
//    ViewClipDecoration 消费面（本件只落数据面与判定面）。
#pragma once

#include "Export.h"
#include "ToolAdmin.h"
#include "Viewport.h"

#include <dqCommon/ViewFlags.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace dqApp {

class Viewport;

/// Orientation for the clip plane/shape context rotation.
/// Ported from: itwinjs-core ContextRotationId (AccuDraw.ts:88-97 子集——
/// ViewClip 工具的 orientation 设置枚举面)
enum class ContextRotationId : int {
    Top = 0,
    Front = 1,
    Left = 2,
    Bottom = 3,
    Back = 4,
    Right = 5,
    View = 6,
    Face = 7,
};

/// Events for ViewClipTool.
/// Ported from: itwinjs-core ViewClipEventHandler (ClipViewTool.ts:37-51)
class DQ_APP_EXPORT ViewClipEventHandler {
public:
    virtual ~ViewClipEventHandler() = default;

    /// If true, the tool will select the clip decoration upon creation.
    /// Ported from: ViewClipEventHandler.selectOnCreate (:39-40)
    virtual bool selectOnCreate() const { return false; }
    /// If true, the tool will clear the clip decoration upon deselect.
    /// Ported from: ViewClipEventHandler.clearOnDeselect (:41-42)
    virtual bool clearOnDeselect() const { return false; }

    /// Called when a new clip is defined or an existing clip is modified.
    /// Ported from: ViewClipEventHandler.onNewClip (:43-45)
    virtual void onNewClip(Viewport& viewport) { (void)viewport; }
    /// Called when a new clip plane is defined.
    /// Ported from: ViewClipEventHandler.onNewClipPlane (:46-48)
    virtual void onNewClipPlane(Viewport& viewport) { (void)viewport; }
    /// Called when an existing clip is modified.
    /// Ported from: ViewClipEventHandler.onModifyClip (:49-50)
    virtual void onModifyClip(Viewport& viewport) { (void)viewport; }
    /// Called when an existing clip is cleared.
    /// Ported from: ViewClipEventHandler.onClearClip (:51)
    virtual void onClearClip(Viewport& viewport) { (void)viewport; }
    /// Called when a right click is performed on the clip decoration.
    /// Ported from: ViewClipEventHandler.onRightClick (:49 of provider 面)
    virtual void onRightClick(Viewport& viewport) { (void)viewport; }
};

/// Tool to define a clip volume for a view（基类——五种定义的共用面）.
/// Ported from: itwinjs-core ViewClipTool (ClipViewTool.ts:77-443)
class DQ_APP_EXPORT ViewClipTool : public PrimitiveTool {
public:
    /// Ported from: ViewClipTool ctor (:79)
    explicit ViewClipTool(ViewClipEventHandler* clipEventHandler = nullptr)
        : m_clipEventHandler(clipEventHandler) {}

    // --- 基类生命周期（:105-130）---

    /// Ported from: onPostInstall (:107-110)
    void onPostInstall() override;
    /// Ported from: onUnsuspend (:112)
    void onUnsuspend() override;
    /// Ported from: onRestartTool (:113)
    void onRestartTool();
    /// Ported from: showPrompt (:114 — 基类空)
    virtual void showPrompt() {}
    /// Ported from: setupAndPromptForNextAction (:115)
    virtual void setupAndPromptForNextAction() { showPrompt(); }
    /// Ported from: onResetButtonUp (:116-119 —— onReinitialize + No)
    EventHandled onResetButtonUp(BeButtonEvent const& ev) override;

    /// Ported from: requireWriteableTarget (:101 — false：剖切不写元素)
    bool requireWriteableTarget() const override { return false; }
    /// Ported from: isCompatibleViewport (:102 —— 3d 操纵门)
    bool isCompatibleViewport(Viewport* vp) const;

    // --- 工厂静态面（:131-232）——数据面核心，测试直锁 ---

    /// Ported from: getPlaneInwardNormal (:131-136)（EQUIVALENCE 见文件头——
    /// AccuDraw 上下文旋转缺席，世界轴/视图 Z 承载）
    static std::optional<dqGeom::Vector3d> getPlaneInwardNormal(ContextRotationId orientation,
                                                                Viewport const& viewport);

    /// Ported from: enableClipVolume (:138-144)
    static bool enableClipVolume(Viewport& viewport);

    /// Ported from: setViewClip (:146-150 —— view.setViewClip + setupFromView）
    static bool setViewClip(Viewport& viewport, dqGeom::ClipVector::Ptr const& clip = {});

    /// Ported from: doClipToConvexClipPlaneSet (:152-157)
    static bool doClipToConvexClipPlaneSet(Viewport& viewport,
                                           dqGeom::ConvexClipPlaneSet const& planes);

    /// Ported from: doClipToPlane (:159-179 —— 追加现存唯一 planeSet 的多面剖切）
    static bool doClipToPlane(Viewport& viewport, dqGeom::Point3d const& origin,
                              dqGeom::Vector3d const& normal, bool clearExistingPlanes);

    /// Ported from: doClipToShape (:181-185)
    static bool doClipToShape(Viewport& viewport, std::vector<dqGeom::Point3d> const& xyPoints,
                              dqGeom::Transform const* transform = nullptr,
                              std::optional<double> zLow = std::nullopt,
                              std::optional<double> zHigh = std::nullopt);

    /// Ported from: doClipToRange (:187-195 —— 薄 Z 时只裁 XY）
    static bool doClipToRange(Viewport& viewport, dqGeom::Range3d const& range,
                              dqGeom::Transform const* transform = nullptr);

    /// Ported from: doClipClear (:196-200)
    static bool doClipClear(Viewport& viewport);

    /// Ported from: getClipRayTransformed (:203-215)
    static dqGeom::Ray3d getClipRayTransformed(dqGeom::Point3d const& origin,
                                               dqGeom::Vector3d const& direction,
                                               dqGeom::Transform const* transform = nullptr);

    /// Ported from: getOffsetValueTransformed (:217-227)
    static double getOffsetValueTransformed(double offset, dqGeom::Transform const* transform = nullptr);

    /// Ported from: getClipShapePoints (:312-318)
    static std::vector<dqGeom::Point3d> getClipShapePoints(dqGeom::ClipShape const& shape, double z);

    /// Ported from: getClipShapeExtents (:320-343)
    static dqGeom::Range1d getClipShapeExtents(dqGeom::ClipShape const& shape,
                                               dqGeom::Range3d const& viewRange);

    /// Ported from: isSingleClipShape (:345-357)
    static dqGeom::ClipShape const* isSingleClipShape(dqGeom::ClipVector const& clip);

    /// Ported from: isSingleConvexClipPlaneSet (:385-394)
    static dqGeom::ConvexClipPlaneSet const* isSingleConvexClipPlaneSet(dqGeom::ClipVector const& clip);

    /// Ported from: isSingleClipPlane (:396-402)
    static dqGeom::ClipPlane const* isSingleClipPlane(dqGeom::ClipVector const& clip);

    /// Ported from: areClipsEqual (:404-440)
    static bool areClipsEqual(dqGeom::ClipVector const& clipA, dqGeom::ClipVector const& clipB);

    /// Ported from: hasClip (:441-443)
    static bool hasClip(Viewport const& viewport);

protected:
    ViewClipEventHandler* m_clipEventHandler = nullptr;  // not owned（参考 ctor 形参）
};

/// Tool to remove a clip volume for a view.
/// Ported from: itwinjs-core ViewClipClearTool (ClipViewTool.ts:445-480)
class DQ_APP_EXPORT ViewClipClearTool final : public ViewClipTool {
public:
    const char* getToolId() const override { return "ViewClip.Clear"; }

    /// Ported from: isCompatibleViewport (:449 —— hasClip 门)
    bool isCompatibleViewport(Viewport* vp) const;

    /// Ported from: doClipClear (:452-460)
    bool doClipClear(Viewport& viewport);

    /// Ported from: onPostInstall (:462-466 —— 有 clip 则安装即清）
    void onPostInstall() override;

    /// Ported from: onDataButtonDown (:468-473)
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;
};

/// Tool to define a clip volume for a view by specifying a plane.
/// Ported from: itwinjs-core ViewClipByPlaneTool (ClipViewTool.ts:483-558)
class DQ_APP_EXPORT ViewClipByPlaneTool : public ViewClipTool {
public:
    /// Ported from: toolId (:484)
    const char* getToolId() const override { return "ViewClip.ByPlane"; }

    /// Ported from: ctor (:489 —— _clearExistingPlanes 多面剖切位）
    explicit ViewClipByPlaneTool(ViewClipEventHandler* clipEventHandler = nullptr,
                                 bool clearExistingPlanes = false)
        : ViewClipTool(clipEventHandler), m_clearExistingPlanes(clearExistingPlanes) {}

    /// Ported from: orientation getter/setter (:491-494)
    ContextRotationId orientation() const { return m_orientation; }
    void setOrientation(ContextRotationId option) { m_orientation = option; }

    /// Ported from: onDataButtonDown (:545-558 —— 单点接受：
    /// 法向 → enableClipVolume → doClipToPlane → onNewClipPlane → 重初始化退出）
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;

protected:
    ContextRotationId m_orientation = ContextRotationId::Face;  // :487 默认 Face
    bool m_clearExistingPlanes = false;
};

/// Tool to define a clip volume for a view by specifying a shape.
/// Ported from: itwinjs-core ViewClipByShapeTool (ClipViewTool.ts:564-800)
class DQ_APP_EXPORT ViewClipByShapeTool : public ViewClipTool {
public:
    const char* getToolId() const override { return "ViewClip.ByShape"; }

    /// Ported from: orientation (:571-574)
    ContextRotationId orientation() const { return m_orientation; }
    void setOrientation(ContextRotationId option) { m_orientation = option; }

    /// Ported from: onDataButtonDown (:759-800 —— 逐点收集；≥2 点无 Ctrl 落点
    /// 即闭合：局部化 → doClipToShape → onNewClip；Ctrl 加点 :759）
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;

    /// Ported from: onUndoPreviousStep (:792-800)
    bool onUndoPreviousStep() override;

    /// Ported from: getClipPoints (:674-709 —— 投影到首点平面；2 点且无 Ctrl
    /// 自动补成矩形 :692-703)（EQUIVALENCE：AccuDraw 投影 → 法向正交投影）
    std::vector<dqGeom::Point3d> getClipPoints(BeButtonEvent const& ev) const;

    /// 已收集的点（测试面）。
    std::vector<dqGeom::Point3d> const& points() const { return m_points; }

protected:
    ContextRotationId m_orientation = ContextRotationId::Top;  // :566 默认 Top
    std::vector<dqGeom::Point3d> m_points;                     // :570 _points
    std::optional<dqGeom::Matrix3d> m_matrix;                  // :571
    std::optional<double> m_zLow;                              // :572
    std::optional<double> m_zHigh;                             // :573
};

/// Tool to define a clip volume for a view by specifying range corners.
/// Ported from: itwinjs-core ViewClipByRangeTool (ClipViewTool.ts:805-921)
class DQ_APP_EXPORT ViewClipByRangeTool : public ViewClipTool {
public:
    const char* getToolId() const override { return "ViewClip.ByRange"; }

    /// Ported from: getClipRange (:840-852 —— Top 上下文旋转基 + 逆变换
    /// 两角点；EQUIVALENCE：AccuDraw ACS 锁 → 世界 Top 恒等基）
    bool getClipRange(dqGeom::Range3d& range, dqGeom::Transform& transform,
                      BeButtonEvent const& ev) const;

    /// Ported from: onDataButtonDown (:880-914 —— 两角点：第二点 →
    /// enableClipVolume → doClipToRange → onNewClip；Ctrl+Z undo :792 同族）
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;

    /// Ported from: onUndoPreviousStep (:916-921)
    bool onUndoPreviousStep() override;

    /// 首角点（测试面；:812 _corner）。
    std::optional<dqGeom::Point3d> const& corner() const { return m_corner; }

protected:
    std::optional<dqGeom::Point3d> m_corner;  // :812
};

/// Tool to define a clip volume for a view using the element aligned box or
/// axis aligned box.
/// Ported from: itwinjs-core ViewClipByElementTool (ClipViewTool.ts:926-1058)
class DQ_APP_EXPORT ViewClipByElementTool : public ViewClipTool {
public:
    const char* getToolId() const override { return "ViewClip.ByElement"; }

    /// Ported from: ctor (:932 —— _alwaysUseRange）
    explicit ViewClipByElementTool(ViewClipEventHandler* clipEventHandler = nullptr,
                                   bool alwaysUseRange = false)
        : ViewClipTool(clipEventHandler), m_alwaysUseRange(alwaysUseRange) {}

    /// Ported from: doClipToElements (:978-1036 —— 单元素：placement.transform
    /// 元素对齐盒 :1030-1036；多元素合并 range :992-994；XY 退化 → XZ/YZ 四点
    /// shape 回退 :1004-1027；1.001 padding :1000）
    /// §3.4：Id64Arg → std::vector<uint64_t>（findPlacement 数据面）。
    static bool doClipToElements(Viewport& viewport, std::vector<uint64_t> const& ids,
                                 bool alwaysUseRange = false);

    /// Ported from: onDataButtonDown (:1050-1058 —— doLocate 命中 → doClipToElements）
    /// EQUIVALENCE：locateManager 拾取链 → 拾取命中 id 直取（PickDumpScene 先例）。
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;

protected:
    bool m_alwaysUseRange = false;
};

}  // namespace dqApp
