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

#include "EditManipulator.h"
#include "Export.h"
#include "ToolAdmin.h"
#include "Viewport.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/ViewFlags.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipUtilsLoops.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Plane3dByOriginAndUnitNormal.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Ray3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <memory>
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
    /// Called when user right clicks on clip geometry or clip modify handle.
    /// Return true if event handled.
    /// Ported from: ViewClipEventHandler.onRightClick (:51)
    /// §3.4：HitDetail（DanQing 拾取链无该类型——PickDumpScene 先例）→
    /// sourceId 载荷承载 hit.sourceId。
    virtual bool onRightClick(uint32_t sourceId, BeButtonEvent const& ev)
    {
        (void)sourceId;
        (void)ev;
        return false;
    }
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

    // --- 装饰绘制面（:232-343 + drawClipPlanesLoops :356-382；P-F）---

    /// Ported from: addClipPlanesLoops (:232-238 —— outline=路径/填充=loop）
    static void addClipPlanesLoops(dqRender::GraphicBuilder& builder,
                                   std::vector<dqBase::RefPtr<dqGeom::Loop>> const& loops,
                                   bool outline);

    /// Ported from: drawClipShape (:294-309 —— WorldDecoration 轮廓 + 未选时
    /// WorldOverlay 隐藏线；flashed→hilite 色）
    static void drawClipShape(DecorateContext& context, dqGeom::ClipShape const& shape,
                              dqGeom::Range1d const& extents, dqCommon::ColorDef const& color,
                              double weight, std::optional<uint32_t> id = std::nullopt);

    /// Ported from: drawClipPlanesLoops (:356-382 —— WorldDecoration 边线 +
    /// WorldOverlay 隐藏线 + 可选填充面）
    static void drawClipPlanesLoops(DecorateContext& context,
                                    std::vector<dqBase::RefPtr<dqGeom::Loop>> const& loops,
                                    dqCommon::ColorDef const& color, double weight, bool dashed,
                                    std::optional<dqCommon::ColorDef> fill = std::nullopt,
                                    std::optional<uint32_t> id = std::nullopt);

protected:
    ViewClipEventHandler* m_clipEventHandler = nullptr;  // not owned（参考 ctor 形参）

private:
    /// Ported from: addClipShape (:239-246 —— lo/hi 两多边形 + 竖线连接）
    static void addClipShape(dqRender::GraphicBuilder& builder, dqGeom::ClipShape const& shape,
                             dqGeom::Range1d const& extents);
};

/// Tool to remove a clip volume for a view.
/// Ported from: itwinjs-core ViewClipClearTool (ClipViewTool.ts:445-480)
class DQ_APP_EXPORT ViewClipClearTool final : public ViewClipTool {
public:
    const char* getToolId() const override { return "ViewClip.Clear"; }

    /// Ported from: ctor 继承（参考子类未声明 ctor——继承基类
    /// constructor(clipEventHandler?) :78；C++ using 继承构造承载）。
    using ViewClipTool::ViewClipTool;

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

    /// Ported from: ctor 继承（参考子类未声明 ctor——继承基类
    /// constructor(clipEventHandler?) :78；C++ using 继承构造承载）。
    using ViewClipTool::ViewClipTool;

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

    /// Ported from: ctor 继承（参考子类未声明 ctor——继承基类
    /// constructor(clipEventHandler?) :78；C++ using 继承构造承载）。
    using ViewClipTool::ViewClipTool;

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

// ---------------------------------------------------------------------------
// ViewClipDecoration（ClipViewTool.ts:1285-1997）——剖切装饰 + 修改手柄
// M-P P-F。EQUIVALENCE：
//  - 拖拽修改工具（ViewClipShapeModifyTool/ViewClipPlanesModifyTool :1061-1282）
//    依赖 InputCollector 安装调度（DanQulaing stub）——modifyControls 返回 false
//    承载（TODO 随 InputCollector 落地补齐）。
//  - >5 点多边形压缩（PolylineOps.compressByChapterError 未移植）——不压缩
//    直接使用（凸/凹多边形全保真；压缩为优化非语义）。
//  - 右键菜单宿主（onActiveClipRightClick 菜单面）——Provider onRightClick
//    无监听时默认 negate 语义 1:1（:2048-2049）；菜单宿主接线归 P-G。
//  - 单面 clip 的 floatingOrigin 迁移（isPointVisibleXY 未移植）——手柄恒
//    位于 loop 质心/平面投影点（参考默认路径）。
//  - animateFrustumChange（orientView 尾）——SetupFromFrustum+synchWithView
//    终态 1:1，动画面缺席。
// ---------------------------------------------------------------------------

/// Handle data for clip volume control.
/// Ported from: itwinjs-core ViewClipControlArrow (ClipViewTool.ts:1285-1308)
struct ViewClipControlArrow {
    ViewClipControlArrow() = default;  // vector::resize 需默认构造（§3.4 技术适配）

    /// Ported from: ctor (:1287-1296)
    ViewClipControlArrow(dqGeom::Point3d const& originIn, dqGeom::Vector3d const& directionIn,
                         double sizeInchesIn,
                         std::optional<dqCommon::ColorDef> fillIn = std::nullopt,
                         std::optional<dqCommon::ColorDef> outlineIn = std::nullopt,
                         std::optional<std::string> nameIn = std::nullopt)
        : origin(originIn)
        , direction(directionIn)
        , sizeInches(sizeInchesIn)
        , fill(fillIn)
        , outline(outlineIn)
        , name(nameIn.has_value() ? *nameIn : "") {}

    dqGeom::Point3d origin;                          // :1288
    dqGeom::Vector3d direction;                      // :1289
    double sizeInches = 0.0;                         // :1290
    std::optional<dqCommon::ColorDef> fill;          // :1291
    std::optional<dqCommon::ColorDef> outline;       // :1292
    std::string name;                                // :1293（zLow/zHigh 命名手柄）
    std::optional<dqGeom::Point3d> floatingOrigin;   // :1294（单面手柄可见性迁移）
};

/// Tool to show a manipulator for clip volumes.
/// Ported from: itwinjs-core ViewClipDecoration (ClipViewTool.ts:1310-1997)
class DQ_APP_EXPORT ViewClipDecoration final : public HandleProvider {
public:
    /// Ported from: ctor (:1335-1344 —— getClipData + clipId + decorator 注册 +
    /// selectOnCreate 选集替换）
    ViewClipDecoration(Viewport& clipView, ViewClipEventHandler* clipEventHandler = nullptr);

    dqGeom::ClipShape const* clipShape() const { return m_clipShape; }
    dqGeom::ConvexClipPlaneSet const* clipPlaneSet() const { return m_clipPlanes; }
    dqGeom::ClipVector::Ptr const& clip() const { return m_clip; }
    uint32_t clipId() const { return m_clipId; }
    /// Ported from: getControlIndex (:1350)
    int getControlIndex(uint32_t id) const;

    /// Ported from: stop (:1352-1365)
    void stop();

    /// Ported from: createControls (:1535-1563 —— 选集门[clipId 选中 + 无其它
    /// 元素] + clearOnDeselect 清理）+ createClipShapeControls (:1441-1465 每边
    /// 中点法向箭头 + zLow/zHigh）+ createClipPlanesControls (:1477-1533 loop
    /// 质心优先 + 非贡献面红箭头）
    bool createControls() override;

    /// Ported from: clearControls (:1565-1568 —— 选集控制 id 移除）
    void clearControls() override;

    /// Ported from: modifyControls (:1571-1585 —— modify 工具安装；拖拽面
    /// EQUIVALENCE——InputCollector 调度缺席，登记 false）
    bool modifyControls(uint32_t sourceId, BeButtonEvent const& ev) override;

    /// Ported from: doClipPlaneNegate (:1587-1605 —— 逐面 cloneNegated 重建）
    bool doClipPlaneNegate(int index);

    /// Ported from: doClipPlaneClear (:1607-1635 —— 单面→整体 clear；多面→重建
    /// 去该面）
    bool doClipPlaneClear(int index);

    /// Ported from: doClipPlaneOrientView (:1727-1755 —— 锚定射线刚体矩阵 +
    /// setupFromFrustum + synchWithView；animateFrustumChange EQUIVALENCE）
    bool doClipPlaneOrientView(int index);

    /// Ported from: isClipShapeAlignedWithWorldUp (:1767-1798 —— 世界上面平行
    /// 判别 + extents 世界化填充）
    bool isClipShapeAlignedWithWorldUp(dqGeom::Range1d* extents = nullptr);

    /// Ported from: doClipShapeSetZExtents (:1800-1828 —— 世界 zLow/zHigh →
    /// transformToClip 逆变换 → ClipShape.createFrom + initSecondaryProps）
    bool doClipShapeSetZExtents(dqGeom::Range1d const& extents);

    /// Ported from: onRightClick (:1830-1833 —— 交 handler）
    bool onRightClick(uint32_t sourceId, BeButtonEvent const& ev);

    /// Ported from: onSelectionChanged (:182-185 面) —— 选集事件深度包裹
    /// （延迟 delete 的释放点——见 ViewClipDecoration.cpp 文件头注）
    void onSelectionChanged() override;

    /// Ported from: onManipulatorEvent (:1841-1846 —— Accept→onModifyClip）
    void onManipulatorEvent(ManipulatorEventType eventType) override;

    /// Ported from: testDecorationHit (:1848)
    bool TestDecorationHit(uint32_t featureId) const override;

    /// Ported from: getDecorationToolTip (:1850-1853 —— "View Clip"/
    /// "Modify View Clip"——CoreTools.json tools.ViewClip.Message 解析值内联，
    /// DanQing 无 localization 面）
    QString GetDecorationToolTip(uint32_t featureId) const override;

    /// Ported from: updateDecorationListener override (:1855 —— 装饰器注册
    /// 以 clipId 存在性为门，非 add 形参）
    void updateDecorationListener(bool add) override;

    /// Ported from: decorate (:1854-1964 —— 轮廓[白线 + 青填充 + 非贡献红虚线]
    /// + 手柄箭头[getArrowShape + getArrowTransform + 选中态填充]）
    void Decorate(DecorateContext& context) override;

    // --- 单例面（:1966-1996）---

    /// Ported from: get (:1966-1970)
    static ViewClipDecoration* get(Viewport& vp);
    /// Ported from: create (:1972-1979 —— hasClip 门）
    static std::optional<uint32_t> create(Viewport& vp,
                                          ViewClipEventHandler* clipEventHandler = nullptr);
    /// Ported from: clear (:1981-1986)
    static void clear();
    /// Ported from: toggle (:1988-1994)
    static std::optional<uint32_t> toggle(Viewport& vp,
                                         ViewClipEventHandler* clipEventHandler = nullptr);

    /// onViewClose 事件面登记：DanQing ViewManager 无 onViewClose（参考
    /// :1341 经 IModelApp.viewManager.onViewClose 注册）——保留公开可调面，
    /// 宿主关闭视口时接线。
    void onViewClose(Viewport& vp);

    /// 修改工具数据面访问（EQUIVALENCE 下的测试缝）。
    std::vector<ViewClipControlArrow> const& controls() const { return m_controls; }
    std::vector<uint32_t> const& controlIds() const { return m_controlIds; }

private:
    /// Ported from: getClipData (:1368-1425 —— shape/planes 解析 + >12 面只读
    /// 预览 loops + loops.length > planes.length 拒绝）
    bool getClipData();

    /// Ported from: ensureNumControls (:1427-1436)
    void ensureNumControls(size_t numReqControls);

    /// Ported from: createClipShapeControls (:1438-1465)
    bool createClipShapeControls();

    /// Ported from: createClipPlanesControls (:1477-1533)
    bool createClipPlanesControls();

    /// Ported from: getLoopCentroidAreaNormal (:1467-1475 —— PolygonOps.
    /// centroidAreaNormal 的 loop/LineString 形态面）
    static std::optional<dqGeom::Ray3d> getLoopCentroidAreaNormal(dqGeom::Loop const* geom);

    /// Ported from: getWorldUpPlane (:1757-1766)
    /// EQUIVALENCE：AccuDrawHintBuilder.getContextRotation(Top) 未移植——世界 Z
    /// 列承载；isContextRotationRequired/getAuxCoordOrigin（ACS 面）缺席——
    /// 世界原点承载。
    std::optional<dqGeom::Plane3dByOriginAndUnitNormal> getWorldUpPlane() const;

    /// Ported from: isAlignedToWorldUpPlane (:1762-1765 —— 双向平行判别）
    bool isAlignedToWorldUpPlane(dqGeom::Plane3dByOriginAndUnitNormal const& plane,
                                 dqGeom::Transform const* transformFromClip) const;

    dqGeom::ClipVector::Ptr m_clip;                       // :1316 _clip
    uint32_t m_clipId = 0;                                // :1317 _clipId（transient id 面）
    std::optional<dqGeom::Range1d> m_clipShapeExtents;    // :1319
    dqGeom::ClipShape const* m_clipShape = nullptr;       // :1318（借自 clip——不拥有）
    dqGeom::ConvexClipPlaneSet const* m_clipPlanes = nullptr;  // :1320（借自 clip）
    std::vector<dqBase::RefPtr<dqGeom::Loop>> m_clipPlanesLoops;      // :1321
    std::vector<dqBase::RefPtr<dqGeom::Loop>> m_clipPlanesLoopsNoncontributing;  // :1322
    std::vector<uint32_t> m_controlIds;                   // :1323
    std::vector<ViewClipControlArrow> m_controls;         // :1324
    bool m_suspendDecorator = false;                      // :1325
    ViewClipEventHandler* m_clipEventHandler = nullptr;   // not owned

    static ViewClipDecoration* s_decorator;               // :1311 _decorator 单例
    static uint32_t s_nextTransientId;                    // transientIds.getNext() 面
    /// clear() 重入守卫（DanQing 选集直挂适配——EditManipulator.h 文件头
    /// EQUIVALENCE——的同步重入面：clearControls 的 selectionSet.remove 触发
    /// Synch → createControls → clearOnDeselect → clear()；参考经 ToolAdmin
    /// manipulatorToolEvent 无此同步重入）。
    static bool s_clearing;
};

/// Event types for ViewClipDecorationProvider.onActiveClipChanged.
/// Ported from: itwinjs-core ClipEventType (ClipViewTool.ts:2003)
enum class ClipEventType : uint8_t { New, NewPlane, Modify, Clear };

/// An implementation of ViewClipEventHandler that responds to new clips by
/// presenting clip modification handles.
/// Ported from: itwinjs-core ViewClipDecorationProvider (ClipViewTool.ts:2008-2073)
class DQ_APP_EXPORT ViewClipDecorationProvider final : public ViewClipEventHandler {
public:
    bool selectDecorationOnCreate = true;   // :2011
    bool clearDecorationOnDeselect = true;  // :2012

    /// Ported from: onActiveClipChanged BeEvent (:2014 —— 载荷 (viewport,
    /// eventType, provider)；§3.4：HitDetail → sourceId 见 ViewClipEventHandler）
    dqBase::DqEvent<Viewport&, ClipEventType, ViewClipDecorationProvider*> onActiveClipChanged;
    /// Ported from: onActiveClipRightClick BeEvent (:2024 —— 载荷 (hit, ev,
    /// provider)）
    dqBase::DqEvent<uint32_t, BeButtonEvent const&, ViewClipDecorationProvider*> onActiveClipRightClick;

    /// Ported from: selectOnCreate (:2028)
    bool selectOnCreate() const override { return selectDecorationOnCreate; }
    /// Ported from: clearOnDeselect (:2029)
    bool clearOnDeselect() const override { return clearDecorationOnDeselect; }

    /// Ported from: onNewClip (:2031-2034）
    void onNewClip(Viewport& viewport) override;
    /// Ported from: onNewClipPlane (:2036-2039）
    void onNewClipPlane(Viewport& viewport) override;
    /// Ported from: onModifyClip (:2041-2043）
    void onModifyClip(Viewport& viewport) override;
    /// Ported from: onClearClip (:2045-2048）
    void onClearClip(Viewport& viewport) override;
    /// Ported from: onRightClick (:2044-2048 —— 无监听 → 默认 negate :2046-2047）
    bool onRightClick(uint32_t sourceId, BeButtonEvent const& ev) override;

    /// Ported from: show/hide/toggle/isActive (:2058-2061）
    void showDecoration(Viewport& vp);
    void hideDecoration();
    std::optional<uint32_t> toggleDecoration(Viewport& vp);
    bool isDecorationActive(Viewport& vp) const;

    /// Ported from: static create/clear (:2063-2073 —— 单例）
    static ViewClipDecorationProvider& create();
    static void clearProvider();

private:
    static ViewClipDecorationProvider* s_provider;  // :2009
};

}  // namespace dqApp
