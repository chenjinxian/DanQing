// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — EditManipulator（on-screen control handles 基建）
// Ported from: itwinjs-core core/frontend/src/tools/EditManipulator.ts
//              EventType (:31-39) + HandleTool (:47-121) + HandleProvider (:127-276) +
//              HandleUtils (:278-328)
//
// M-P P-F。§3.4/EQUIVALENCE（验证法 = ClipDecorationTest + E2E 轮廓像素锁）：
//  - HandleTool 的 InputCollector 安装/挂起调度（DanQing InputCollector 为
//    "minimal stub"——ToolAdmin.h :673 登记），本件以 Tool 直装承载 accept/cancel
//    双出口语义；拖拽修改工具（ViewClipShapeModifyTool/ViewClipPlanesModifyTool
//    :1061-1282）随 InputCollector 调度落地补齐（P-F TODO 登记）。
//  - ManipulatorToolEvent（ToolAdmin.manipulatorToolEvent :137）：DanQing ToolAdmin
//    无该事件面——HandleProvider 以 SelectionSet.OnChanged 直挂承载 Synch 语义
//    （参考 Start/Stop 事件对选集监听的安装/卸载 :159-176 —— DanQing 无安装期
//    生命周期差异，监听随 provider 生命周期）。
//  - getArrowTransform 的 AccuDrawHintBuilder.getBoresite：DanQing AccuDraw 未
//    移植——boresite = 视图旋转 Z 列（观察方向；参考 boresite 即视线）。
#pragma once

#include "Decorator.h"
#include "Export.h"
#include "ToolAdmin.h"
#include "Viewport.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/Vector3d.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace dqApp {

/// Specifies the event for EditManipulator::HandleProvider::onManipulatorEvent.
/// Ported from: itwinjs-core EditManipulator.EventType (EditManipulator.ts:31-39)
enum class ManipulatorEventType : uint8_t {
    /// Control handles should be created, updated, or cleared based on the active selection.
    Synch,
    /// Control handle modification was cancelled by user.
    Cancel,
    /// Control handle modification was accepted by user.
    Accept,
};

/// Utility methods for creating control handles and other decorations.
/// Ported from: itwinjs-core EditManipulator.HandleUtils (EditManipulator.ts:278-328)
struct DQ_APP_EXPORT HandleUtils {
    /// Adjust input color for contrast against view background.
    /// Ported from: HandleUtils.adjustForBackgroundColor (:285-290)
    /// §3.4：adjustedForContrast（DanQing ColorDef 未移植）→ 亮度判别翻转承载
    /// （暗背景提亮 / 亮背景压暗——对比度调整的等价面；EQUIVALENCE 登记）。
    static dqCommon::ColorDef adjustForBackgroundColor(dqCommon::ColorDef const& color,
                                                      Viewport const& vp);

    /// Compute a transform that will try to orient a 2d shape (like an arrow)
    /// to face the camera.
    /// Ported from: HandleUtils.getArrowTransform (:300-313)
    /// EQUIVALENCE：boresite = 视图旋转 Z 列（AccuDraw getBoresite 缺席——观察
    /// 方向等价重表达）；pixelsFromInches + getPixelSizeAtPoint 同参考链。
    static std::optional<dqGeom::Transform> getArrowTransform(Viewport const& vp,
                                                             dqGeom::Point3d const& base,
                                                             dqGeom::Vector3d const& direction,
                                                             double sizeInches);

    /// Return array of shape points representing a unit arrow in xy plane
    /// pointing in positive x direction.
    /// Ported from: HandleUtils.getArrowShape (:316-327)
    static std::vector<dqGeom::Point3d> getArrowShape(
        double baseStart = 0.0, double baseWidth = 0.15, double tipStart = 0.55,
        double tipEnd = 1.0, double tipWidth = 0.3, double flangeStart = 0.0,
        double flangeWidth = 0.0);
};

/// A handle provider maintains a set of controls used to modify element(s) or
/// pickable decorations.
/// Ported from: itwinjs-core EditManipulator.HandleProvider (EditManipulator.ts:127-276)
class DQ_APP_EXPORT HandleProvider : public IDecorator {
public:
    /// Ported from: HandleProvider ctor (:136-138 —— manipulatorToolEvent 监听)
    /// §3.4：DanQing 无 ManipulatorToolEvent——SelectionSet.OnChanged 直挂
    /// （Synch 语义承载，见文件头）。
    explicit HandleProvider(Viewport& clipView);
    virtual ~HandleProvider();

    /// Call to clear this handle provider.
    /// Ported from: HandleProvider.stop (:141-151)
    void stop();

    /// Event raised by SelectionSet when the active selection changes.
    /// Ported from: HandleProvider.onSelectionChanged (:182-185)
    /// （virtual——ViewClipDecoration 覆写以持选集事件栈安全面）
    virtual void onSelectionChanged();

    /// Register for decorate event to start displaying control handles.
    /// Ported from: HandleProvider.updateDecorationListener (:188-200 ——
    /// 子类覆写门面：ViewClipDecoration :1855 以 clipId 存在性承载）
    virtual void updateDecorationListener(bool add);

    /// Sub-classes should override to display the pickable graphics for their controls.
    /// Ported from: HandleProvider.decorate (:203)
    void Decorate(DecorateContext& context) override { (void)context; }

    /// The provider is responsible for checking if modification by controls is valid.
    /// Ported from: HandleProvider.createControls (:208)
    virtual bool createControls() = 0;

    /// Call to stop displaying the control handles.
    /// Ported from: HandleProvider.clearControls (:211-213)
    virtual void clearControls();

    /// A provider can install an input collector to support interactive modification.
    /// Ported from: HandleProvider.modifyControls (:219)
    virtual bool modifyControls(uint32_t sourceId, BeButtonEvent const& ev) = 0;

    /// Create, update, or clear based on the current selection.
    /// Ported from: HandleProvider.updateControls (:222-228)
    void updateControls();

    /// Update controls to reflect active selection or post-modification state.
    /// Ported from: HandleProvider.onManipulatorEvent (:231-234)
    virtual void onManipulatorEvent(ManipulatorEventType eventType);

    /// Event raised to allow a pickable decoration to respond to being located.
    /// Ported from: HandleProvider.onDecorationButtonEvent (:246-275 —— 右键/
    /// 双击/ctrl+click/touch 分派面；modifyControls 为数据半边)
    virtual bool onDecorationButtonEvent(uint32_t sourceId, BeButtonEvent const& ev);

    bool TestDecorationHit(uint32_t featureId) const override = 0;
    QString GetDecorationToolTip(uint32_t /*featureId*/) const override { return {}; }

protected:
    Viewport& m_clipView;              // 参考 _clipView（ScreenViewport 形参）
    bool m_isActive = false;           // :128 _isActive
    dqBase::DqEventScope m_selectionScope;  // 选集监听（stop 卸载）
};

/// Interactive control handle modification base（InputCollector 面——见文件头
/// EQUIVALENCE）。
/// Ported from: itwinjs-core EditManipulator.HandleTool (EditManipulator.ts:47-121)
class DQ_APP_EXPORT HandleTool : public InteractiveTool {
public:
    HandleTool(HandleProvider* manipulator)
        : m_manipulator(manipulator) {}

    const char* getToolId() const override { return "Select.Manipulator"; }

    /// Establish the initial tool state for handle modification.
    /// Ported from: HandleTool.init (:61-72 —— receivedDownEvent + accuSnap/
    /// initLocate 面；DanQing 无 AccuSnap 面——no-op 承载)
    void init();

    /// Called from reset button up event to allow modification to be cancelled.
    /// Ported from: HandleTool.cancel (:82)
    virtual bool cancel(BeButtonEvent const& ev);

    /// Called from data button down event to check if enough input has been
    /// gathered to complete the modification.
    /// Ported from: HandleTool.accept (:87)
    virtual bool accept(BeButtonEvent const& ev) = 0;

    /// Called following cancel or accept to update the handle provider.
    /// Ported from: HandleTool.onComplete (:92-97)
    EventHandled onComplete(BeButtonEvent const& ev, ManipulatorEventType event);

    /// Ported from: HandleTool.onDataButtonDown (:99-104)
    EventHandled onDataButtonDown(BeButtonEvent const& ev) override;

    /// Ported from: HandleTool.onResetButtonUp (:106-111)
    EventHandled onResetButtonUp(BeButtonEvent const& ev) override;

    /// Ported from: HandleTool.onPostInstall (:117-120)
    void onPostInstall() override;

protected:
    HandleProvider* m_manipulator;  // not owned（参考 public readonly manipulator）
};

}  // namespace dqApp
