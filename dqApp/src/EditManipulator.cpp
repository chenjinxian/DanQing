// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — EditManipulator 实现
// Ported from: itwinjs-core core/frontend/src/tools/EditManipulator.ts
#include <dqApp/EditManipulator.h>

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/SelectionSet.h>
#include <dqApp/ViewManager.h>

#include <cmath>

namespace dqApp {

// ---------------------------------------------------------------------------
// HandleUtils (:278-328)
// ---------------------------------------------------------------------------

// Ported from: HandleUtils.adjustForBackgroundColor (:285-290).
// EQUIVALENCE：参考 adjustedForContrast（DanQing ColorDef 未移植该方法）——
// 亮度判别承载：背景暗（亮度 < 0.5）提亮输入色 / 背景亮压暗。发散 = 提亮/压暗
// 的具体曲线（参考 internal 对比度算法）；验证法 = 暗背景白色保持/亮背景压暗
// 的方向断言（ClipDecorationTest）。
dqCommon::ColorDef HandleUtils::adjustForBackgroundColor(dqCommon::ColorDef const& color,
                                                        Viewport const& vp)
{
    ViewState const* view = vp.GetView();
    if (view != nullptr && view->AsViewState3d() != nullptr
        && view->GetDisplayStyle().getEnvironment().displaySky)
        return color;

    // 亮度（Rec. 601 加权）——tbgr 布局：r=低 8 位、g=中、b=高。
    auto lumaOf = [](dqCommon::ColorDef const& c) {
        uint32_t const tbgr = c.getTbgr();
        double const r = static_cast<double>(tbgr & 0xff);
        double const g = static_cast<double>((tbgr >> 8) & 0xff);
        double const b = static_cast<double>((tbgr >> 16) & 0xff);
        return (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
    };
    // DisplayStyle 包装层 getBackgroundColor 返 tbgr（uint32）——从设置层取
    // ColorDef 值类型。
    dqCommon::ColorDef const bg =
        view->GetDisplayStyle().getSettings().getBackgroundColor();
    double const bgLuma = lumaOf(bg);
    double const colorLuma = lumaOf(color);

    // 对比度不足（亮度差 < 0.5）时向背景反向调整。
    // from(red, green, blue, transparency)——transparency = 255 − alpha。
    if (std::abs(bgLuma - colorLuma) < 0.5) {
        int const trans = 255 - color.getAlpha();
        return bgLuma < 0.5 ? dqCommon::ColorDef::from(255, 255, 255, trans)
                            : dqCommon::ColorDef::from(64, 64, 64, trans);
    }
    return color;
}

// Ported from: HandleUtils.getArrowTransform (:300-313).
// EQUIVALENCE：boresite = 视图旋转 Z 列（AccuDraw getBoresite 缺席——参考
// boresite.direction 即视线方向）。
std::optional<dqGeom::Transform> HandleUtils::getArrowTransform(
    Viewport const& vp, dqGeom::Point3d const& base, dqGeom::Vector3d const& direction,
    double sizeInches)
{
    ViewState3d const* view3d = vp.GetView()->AsViewState3d();
    if (view3d == nullptr)
        return std::nullopt;
    dqGeom::Vector3d boresiteDir = view3d->getRotation().ColumnZ();
    if (std::abs(direction.DotProduct(boresiteDir)) >= 0.99)
        return std::nullopt;  // direction almost perpendicular to viewing direction

    double const pixelSize = vp.PixelsFromInches(sizeInches);
    double const scale = vp.GetViewingSpace().getPixelSizeAtPoint(&base) * pixelSize;
    // Matrix3d.createRigidFromColumns(direction, boresite.direction, AxisOrder.XZY)
    dqGeom::Vector3d colZ = dqGeom::Vector3d::FromCrossProduct(direction, boresiteDir);
    if (colZ.Normalize() <= 1e-14)
        return std::nullopt;
    dqGeom::Vector3d const colY = dqGeom::Vector3d::FromCrossProduct(colZ, direction);
    dqGeom::Matrix3d matrix = dqGeom::Matrix3d::CreateRowValues(
        direction.x * scale, colY.x * scale, colZ.x * scale,
        direction.y * scale, colY.y * scale, colZ.y * scale,
        direction.z * scale, colY.z * scale, colZ.z * scale);
    return dqGeom::Transform(base, matrix);
}

// Ported from: HandleUtils.getArrowShape (:316-327).
// §3.4：flangeStart/flangeWidth 参考默认 = tipStart/baseWidth（参数默认值
// 引用同函数其他参数——C++ 不允许，显式展开）。
std::vector<dqGeom::Point3d> HandleUtils::getArrowShape(
    double baseStart, double baseWidth, double tipStart, double tipEnd, double tipWidth,
    double flangeStartIn, double flangeWidthIn)
{
    double const flangeStart = (flangeStartIn != 0.0 ? flangeStartIn : tipStart);
    double const flangeWidth = (flangeWidthIn != 0.0 ? flangeWidthIn : baseWidth);
    std::vector<dqGeom::Point3d> shapePts;
    shapePts.push_back(dqGeom::Point3d::From(tipEnd, 0.0, 0.0));
    shapePts.push_back(dqGeom::Point3d::From(flangeStart, tipWidth, 0.0));
    shapePts.push_back(dqGeom::Point3d::From(tipStart, flangeWidth, 0.0));
    shapePts.push_back(dqGeom::Point3d::From(baseStart, baseWidth, 0.0));
    shapePts.push_back(dqGeom::Point3d::From(baseStart, -baseWidth, 0.0));
    shapePts.push_back(dqGeom::Point3d::From(tipStart, -flangeWidth, 0.0));
    shapePts.push_back(dqGeom::Point3d::From(flangeStart, -tipWidth, 0.0));
    shapePts.push_back(shapePts[0]);
    return shapePts;
}

// ---------------------------------------------------------------------------
// HandleProvider (:127-276)
// ---------------------------------------------------------------------------

// Ported from: HandleProvider ctor (:136-138 —— SelectionSet.OnChanged 直挂；
// EQUIVALENCE 见文件头：无 ManipulatorToolEvent，Synch 语义随监听建立)。
HandleProvider::HandleProvider(Viewport& clipView) : m_clipView(clipView) {
    m_selectionScope.add(m_clipView.GetView()->GetIModel()->GetSelectionSet().OnChanged.AddListener(
        [this](void*) { this->onSelectionChanged(); }));
}

HandleProvider::~HandleProvider()
{
    // DqEventScope 析构批量退订（stop 的监听卸载半边）。
}

// Ported from: HandleProvider.stop (:141-151).
void HandleProvider::stop()
{
    clearControls();
}

// Ported from: HandleProvider.onSelectionChanged (:182-185).
void HandleProvider::onSelectionChanged()
{
    onManipulatorEvent(ManipulatorEventType::Synch);
}

// Ported from: HandleProvider.updateDecorationListener (:188-200).
void HandleProvider::updateDecorationListener(bool add)
{
    if (add) {
        Application::Get().GetViewManager().AddDecorator(this);
        Application::Get().GetViewManager().invalidateDecorationsAllViews();
    } else {
        Application::Get().GetViewManager().DropDecorator(this);
        Application::Get().GetViewManager().invalidateDecorationsAllViews();
    }
}

// Ported from: HandleProvider.clearControls (:211-213).
void HandleProvider::clearControls()
{
    m_isActive = false;
    updateDecorationListener(false);
}

// Ported from: HandleProvider.updateControls (:222-228).
void HandleProvider::updateControls()
{
    bool const created = createControls();
    if (m_isActive && !created)
        clearControls();
    else {
        m_isActive = created;
        updateDecorationListener(created);
    }
}

// Ported from: HandleProvider.onManipulatorEvent (:231-234).
void HandleProvider::onManipulatorEvent(ManipulatorEventType /*eventType*/)
{
    updateControls();
}

// Ported from: HandleProvider.onDecorationButtonEvent (:246-275 —— 右键分支
// 交 onRightClick/ctrl 门/touch 门的简化承载：Data 按钮 → modifyControls)。
bool HandleProvider::onDecorationButtonEvent(uint32_t sourceId, BeButtonEvent const& ev)
{
    if (!m_isActive)
        return false;
    if (ev.button != BeButton::Data)
        return false;
    if ((ev.keyModifiers & BeModifierKeys::Control) != BeModifierKeys::None)
        return false;  // Support ctrl+click to select multiple controls...
    return modifyControls(sourceId, ev);
}

// ---------------------------------------------------------------------------
// HandleTool (:47-121)
// ---------------------------------------------------------------------------

// Ported from: HandleTool.init (:61-72 —— receivedDownEvent + accuSnap/
// initLocate 面；DanQing 无这两面——no-op 承载)。
void HandleTool::init()
{
}

// Ported from: HandleTool.cancel (:82 —— 默认取消).
bool HandleTool::cancel(BeButtonEvent const& /*ev*/)
{
    return true;
}

// Ported from: HandleTool.onComplete (:92-97).
EventHandled HandleTool::onComplete(BeButtonEvent const& /*ev*/, ManipulatorEventType event)
{
    exitTool();
    m_manipulator->onManipulatorEvent(event);
    return EventHandled::Yes;
}

// Ported from: HandleTool.onDataButtonDown (:99-104).
EventHandled HandleTool::onDataButtonDown(BeButtonEvent const& ev)
{
    if (!accept(ev))
        return EventHandled::No;
    return onComplete(ev, ManipulatorEventType::Accept);
}

// Ported from: HandleTool.onResetButtonUp (:106-111).
EventHandled HandleTool::onResetButtonUp(BeButtonEvent const& ev)
{
    if (!cancel(ev))
        return EventHandled::No;
    return onComplete(ev, ManipulatorEventType::Cancel);
}

// Ported from: HandleTool.onPostInstall (:117-120).
void HandleTool::onPostInstall()
{
    InteractiveTool::onPostInstall();
    init();
}

}  // namespace dqApp
