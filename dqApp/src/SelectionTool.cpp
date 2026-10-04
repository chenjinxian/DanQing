// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionTool implementation
// Ported from: itwinjs-core core/frontend/src/tools/SelectTool.ts
//
// M-O(3) P3 增量：selectByPoints 框选族（:287-439）+ ctrl Invert（:414）+
// 框选装饰（:287-327）。EQUIVALENCE 登记：
//   - hit-cycling（:480-502——LocateManager.currHit + doLocate(hitIndex) 的
//     同光标多深度轮转）：DanQing pick 缓冲为深度决胜单值（无命中列表），
//     轮转不可达——延后随 pick 多命中面落地。
//   - 体积选择（ElementSetTool.getVolumeSelectionCandidates :726-744 的
//     ToolSettings.enableVolumeSelection=true 分支）：缺省 false——恒走
//     area 面（参考缺省同态）。
//   - 跨线虚线（ctx.setLineDash([5,5]) :313）：CanvasContext 无 dash 面
//     ——实线绘制，dash 面随 CanvasContext 扩展落地。
#include "SelectionTool.h"
#include "dqApp/Application.h"
#include "dqApp/DecorateContext.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewManager.h"
#include "dqApp/IModelConnection.h"

#include <dqRender/CanvasDecoration.h>

#include <cmath>
#include <vector>

namespace dqApp {

// ---------------------------------------------------------------------------
// onPostInstall — initialize the selection tool state (was OnStart).
// Ported from: itwinjs-core SelectionTool.onPostInstall (SelectTool.ts:574-577)
//              -> initSelectTool (SelectTool.ts:231-242)
// ---------------------------------------------------------------------------
void SelectionTool::onPostInstall()
{
    // Ported from: itwinjs-core SelectionTool.initSelectTool()
    //              this._isSelectByPoints = false;
    //              this._points.length = 0;
    //              this.initLocateElements(enableLocate, false, ...);
    //              IModelApp.locateManager.options.allowDecorations = true;
    //              this.showPrompt(mode, method);
    // enableLocate = SelectionMethod.Pick === method — DanQing 当前唯一模式即 Pick
    // （拖拽框选 _isSelectByPoints/_points 与 method/mode 选择随框选路径 TODO）。
    // SelectTool.ts:239: initLocateElements(enableLocate, false,
    //   enableLocate ? "default" : crossHairCursor, CoordinateLockOverrides.All)
    // ——选择工具悬停时：默认箭头光标 + locate 光圈跟随（坐标锁 All 的
    // coordLockOvr 参数随工具状态子系统 TODO）。
    initLocateElements(/*enableLocate=*/true, /*enableSnap=*/false, "default");
}

// ---------------------------------------------------------------------------
// processMiss — clear selection when clicking on empty space.
// Ported from: itwinjs-core SelectionTool.processMiss() (SelectTool.ts:244-249)
// ---------------------------------------------------------------------------
bool SelectionTool::ProcessMiss(BeButtonEvent const& ev)
{
    (void)ev;
    // Ported from: itwinjs-core SelectionTool.processMiss()
    //              if (!this.iModel.selectionSet.isActive) return false;
    //              this.iModel.selectionSet.emptyAll();
    //              return true;
    auto* viewport = GetViewport();
    if (!viewport)
        return false;

    auto* iModel = viewport->GetIModel();
    if (!iModel)
        return false;

    auto& selSet = iModel->GetSelectionSet();
    if (selSet.isEmpty())
        return false;

    selSet.EmptyAll();
    return true;
}

// ---------------------------------------------------------------------------
// onDataButtonUp — handle data-button release (pick/select/hilite).
// Ported from: itwinjs-core SelectionTool.onDataButtonUp()
//              (SelectTool.ts:441-469)
//
// Task 13 reconciliation (Down -> Up):
//   The reference SelectTool overrides `onDataButtonUp` (SelectTool.ts:441),
//   NOT `onDataButtonDown`. itwinjs performs the locate/pick on the data-
//   button **release** transition. Pre-Task-13 this method was named
//   `onDataButtonDown` (a holdover from the legacy `OnMouseButtonDown`
//   naming); Task 13 renames it to match the reference so Task 7's
//   dispatch (sendButtonEvent) routes the pick on the up event — matching
//   the reference's button transition exactly.
//
// Body source mapping (SelectTool.ts:441-469):
//   L442-443: viewport undefined guard          -> viewport null check
//   L445-446: selectByPointsEnd stub            -> always false (no drag infra)
//   L448-454: selectionMethod != Pick branch     -> N/A (Pick is Step 3 default)
//   L456:     locateManager.doLocate             -> PickAtPoint (Step 3 substitute)
//   L457-463: selectDecoration + processHit      -> processHit path (replace)
//   L465-466: wantSelectionClearOnMiss + processMiss -> emptyAll
//   L468:     return EventHandled.Yes
// ---------------------------------------------------------------------------
EventHandled SelectionTool::onDataButtonUp(BeButtonEvent const& event)
{
    // Ported from: SelectTool.ts:442-443
    //              if (undefined === ev.viewport) return EventHandled.No;
    if (event.viewport == nullptr)
        return EventHandled::No;

    auto* viewport = GetViewport();
    if (!viewport)
        return EventHandled::No;

    auto* iModel = viewport->GetIModel();
    if (!iModel)
        return EventHandled::No;

    // Ported from: SelectTool.ts:445-446 selectByPointsEnd (SelectTool.ts:371-391)
    //               -> drag-selection infrastructure; Step 3 stub (always false).
    // if (selectByPointsEnd(event)) return EventHandled::Yes;

    // Ported from: SelectTool.ts:448-454 selectionMethod != Pick branch.
    //               Step 3 default is SelectionMethod.Pick, so this branch is
    //               unreachable; the Pick path below runs unconditionally.
    // if (SelectionMethod.Pick != this.selectionMethod) { ... }

    // Ported from: SelectTool.ts:456
    //              const hit = await IModelApp.locateManager.doLocate(
    //                  new LocateResponse(), true, ev.point, ev.viewport,
    //                  ev.inputSource);
    // Step 3 substitution: PickAtPoint (pixel-based pick) stands in for
    //                       locateManager.doLocate (geometry-based locate).
    //                       BeButtonEvent carries screen coordinates in
    //                       `viewPoint` (Tool.ts:190); PickAtPoint takes
    //                       screen pixels. TODO: real doLocate via
    //                       LocateManager port (faithful locate pipeline).
    uint32_t const featureId = viewport->PickAtPoint(
        static_cast<int32_t>(event.viewPoint.x),
        static_cast<int32_t>(event.viewPoint.y));

    auto& selSet = iModel->GetSelectionSet();
    auto& hiliteSet = iModel->GetHiliteSet();

    // Ported from: SelectTool.ts:457-463 (selectDecoration + processHit).
    //               selectDecoration is TODO (no HitDetail / decoration pick
    //               port yet); the processHit path runs when featureId != 0.
    // Ported from: SelectTool.ts:408-426 processHit
    //               -> :251-274 updateSelection
    //               -> SelectionProcessing.ReplaceSelectionWithElement
    //               -> this.iModel.selectionSet.replace(elementId).
    if (featureId > 0) {
        QSet<uint32_t> ids;
        ids.insert(featureId);
        // :414 ev.isControlKey ? InvertElementInSelection : Replace
        //（M-O(3) P3 归位——此前恒 Replace）。
        if ((event.keyModifiers & BeModifierKeys::Control)
            != BeModifierKeys::None) {
            if (selSet.Contains(featureId))
                selSet.Remove(ids);
            else
                selSet.add(ids);
        } else {
            selSet.Replace(ids);
        }
    } else {
        // Ported from: SelectTool.ts:465-466
        //              if (!ev.isControlKey && this.wantSelectionClearOnMiss(ev)
        //                  && this.processMiss(ev)) this.syncSelectionMode();
        // wantSelectionClearOnMiss -> true in SelectionMode.Replace (SelectTool.ts:85)
        // Step 3 default is Replace, so the clear-on-miss path applies.
        // syncSelectionMode is a UI TODO stub.
        if (!ProcessMiss(event))
            return EventHandled::No;
    }

    // Ported from: itwinjs-core ViewManager / Target.setHiliteSet
    //               (SelectionSet change listener mirrors the hilite set).
    hiliteSet.SyncWith(selSet);

    // Ported from: itwinjs-core Target.setHilitedFeature + Viewport hilite sync
    //               (the RenderTarget owns the feature-override LUT; the
    //               Viewport drives it via SetHilitedFeature).
    if (selSet.isEmpty()) {
        viewport->SetHilitedFeature(0);
    } else {
        viewport->SetHilitedFeature(featureId);
    }

    // Ported from: itwinjs-core ViewManager.onSelectionSetChanged()
    //               (notifies decorators + viewports that the selection set
    //               changed so they can refresh feature overrides).
    Application::Get().GetViewManager().OnSelectionSetChanged();

    // Ported from: SelectTool.ts:468 return EventHandled.Yes;
    return EventHandled::Yes;
}

// ---------------------------------------------------------------------------
// onResetButtonUp — handle reset-button release (was onResetButtonDown).
// Ported from: itwinjs-core SelectionTool.onResetButtonUp()
//              (SelectTool.ts:471-509)
//
// Task 13 reconciliation:
//   (1) Down -> Up rename (matches the reference's button transition; see
//       onDataButtonUp header comment).
//   (2) Body rewrite: the pre-Task-13 body EMPTIED the selection set on
//       reset. That was invented behavior — the reference's onResetButtonUp
//       does NOT empty the selection set; it cycles through overlapping hits
//       at the cursor (locateManager.currHit / accuSnap.currHit / doLocate)
//       and otherwise calls accuSnap.resetButton. The invented clear has
//       been removed; the faithful Step 3 stub mirrors the reference shape
//       (selectByPoints cleanup branch + TODO hit-cycling + TODO resetButton)
//       and returns EventHandled.Yes (matching the reference's terminal
//       return on every branch).
//
// Body source mapping (SelectTool.ts:471-509):
//   L472-477: _isSelectByPoints cleanup -> unreachable in Step 3 (no drag)
//   L480-502: hit cycling via currHit + doLocate -> TODO LocateManager port
//   L504-505: selectDecoration fallback     -> TODO HitDetail port
//   L507:     accuSnap.resetButton          -> TODO AccuSnap port
//   L508:     return EventHandled.Yes       -> faithful terminal return
// ---------------------------------------------------------------------------
EventHandled SelectionTool::onResetButtonUp(BeButtonEvent const& event)
{
    // Ported from: SelectTool.ts:472-477 selectByPoints cleanup（M-O(3) P3
    //               落地——框选中的 Reset 释放走清面分支）。
    if (m_isSelectByPoints) {
        if (event.viewport != nullptr)
            event.viewport->InvalidateDecorations();
        m_isSelectByPoints = false;
        m_points.clear();
        return EventHandled::Yes;
    }

    // Ported from: SelectTool.ts:480-502 overlapping-hit cycling.
    // TODO（EQUIVALENCE 延后——见文件头登记）：hit-cycling 需 pick 多命中面。

    // Ported from: SelectTool.ts:508 return EventHandled.Yes.
    //               The reference's onResetButtonUp returns Yes on every
    //               terminal branch. The stub preserves that contract.
    (void)event;
    return EventHandled::Yes;
}

// ---------------------------------------------------------------------------
// selectByPoints 族 — Ported from: SelectTool.ts:278-439（M-O(3) P3）。
// ---------------------------------------------------------------------------

// Ported from: SelectionTool.useOverlapSelection (:278-285 — 右→左拖 =
// overlap；Shift 反转).
bool SelectionTool::useOverlapSelection(BeButtonEvent const& ev) const
{
    if (ev.viewport == nullptr || m_points.empty())
        return false;
    dqGeom::Point3d const pt1 = ev.viewport->WorldToView(m_points[0]);
    dqGeom::Point3d const pt2 = ev.viewport->WorldToView(ev.point);
    bool const overlapMode = (pt1.x > pt2.x);
    // Shift inverts inside/overlap selection...
    bool const shift = (ev.keyModifiers & BeModifierKeys::Shift)
        != BeModifierKeys::None;
    return shift ? !overlapMode : overlapMode;
}

// Ported from: SelectionTool.selectByPointsStart (:359-369).
bool SelectionTool::selectByPointsStart(BeButtonEvent const& ev)
{
    if (BeButton::Data != ev.button && BeButton::Reset != ev.button)
        return false;
    m_points.clear();
    m_points.push_back(ev.point);
    m_isSelectByPoints = true;
    return true;
}

// Ported from: SelectionTool.selectByPointsProcess (:329-357 — 候选 + ctrl
// 分流 + 空面 miss 清)。
bool SelectionTool::selectByPointsProcess(dqGeom::Point3d const& origin,
                                          dqGeom::Point3d const& corner,
                                          BeButtonEvent const& ev,
                                          SelectionMethod method, bool overlap)
{
    Viewport* vp = ev.viewport;
    if (vp == nullptr)
        return false;

    // getAreaSelectionCandidates（:637-721）的等价物：PickAtRect 矩形遍历
    // 去重 + Box 非 overlap 的"完全在内"收缩带判定（:676-701 的 outline 带
    // ——border band = 边缘 2 设备像素内的像素；inside = contents - 有边缘
    // 像素的 id）。EQUIVALENCE：参考一次 readPixels 后逐像素分区；DanQing
    // 经 PickAtRect 全域 + 四边缘带五读（band 换算 2/dpr CSS 像素）。
    std::vector<uint32_t> contents;
    vp->PickAtRect(static_cast<int32_t>(origin.x), static_cast<int32_t>(origin.y),
                   static_cast<int32_t>(corner.x), static_cast<int32_t>(corner.y),
                   contents);
    if (SelectionMethod::Box == method && !overlap
        && contents.size() > 1) {
        // 边缘带（2 device px）：四条带读出 outline 候选。
        double const dpr = vp->devicePixelRatioF() > 0.0
            ? vp->devicePixelRatioF() : 1.0;
        int32_t const band = std::max(1, static_cast<int32_t>(std::ceil(2.0 / dpr)));
        int32_t const x0 = static_cast<int32_t>(std::min(origin.x, corner.x));
        int32_t const y0 = static_cast<int32_t>(std::min(origin.y, corner.y));
        int32_t const x1 = static_cast<int32_t>(std::max(origin.x, corner.x));
        int32_t const y1 = static_cast<int32_t>(std::max(origin.y, corner.y));
        std::vector<uint32_t> outline;
        vp->PickAtRect(x0, y0, x1, y0 + band, outline);            // top
        vp->PickAtRect(x0, y1 - band, x1, y1, outline);            // bottom
        vp->PickAtRect(x0, y0, x0 + band, y1, outline);            // left
        vp->PickAtRect(x1 - band, y0, x1, y1, outline);            // right
        // inside = contents - outline（:693-700）。
        std::vector<uint32_t> outlineSet;
        {
            // 去重。
            std::set<uint32_t> seen;
            for (uint32_t id : outline)
                if (id != 0)
                    seen.insert(id);
            outlineSet.assign(seen.begin(), seen.end());
        }
        std::vector<uint32_t> inside;
        for (uint32_t id : contents) {
            bool inOutline = false;
            for (uint32_t oid : outlineSet)
                if (oid == id) { inOutline = true; break; }
            if (!inOutline)
                inside.push_back(id);
        }
        contents = std::move(inside);
    }

    auto* iModel = vp->GetIModel();
    if (iModel == nullptr)
        return false;

    if (contents.empty()) {
        // :337-342 空面：非 ctrl + miss 清。
        bool const ctrl = (ev.keyModifiers & BeModifierKeys::Control)
            != BeModifierKeys::None;
        if (!ctrl && ProcessMiss(ev)) {
            iModel->GetHiliteSet().SyncWith(iModel->GetSelectionSet());
            vp->SetHilitedFeature(0);
            Application::Get().GetViewManager().OnSelectionSetChanged();
            return true;
        }
        return false;
    }

    // :345-356 SelectionMode.Replace 分流（Add/Remove 模式面未移植——缺省
    // Replace）：ctrl → Invert；否则 Replace。
    QSet<uint32_t> ids;
    for (uint32_t id : contents)
        ids.insert(id);
    bool const ctrl = (ev.keyModifiers & BeModifierKeys::Control)
        != BeModifierKeys::None;
    if (ctrl) {
        // Invert（:260-261 语义——在选移除/不在选加入）。
        QSet<uint32_t> toRemove;
        QSet<uint32_t> toAdd;
        for (uint32_t id : contents) {
            if (iModel->GetSelectionSet().Contains(id))
                toRemove.insert(id);
            else
                toAdd.insert(id);
        }
        if (!toRemove.isEmpty())
            iModel->GetSelectionSet().Remove(toRemove);
        if (!toAdd.isEmpty())
            iModel->GetSelectionSet().add(toAdd);
    } else {
        iModel->GetSelectionSet().Replace(ids);
    }
    iModel->GetHiliteSet().SyncWith(iModel->GetSelectionSet());
    vp->SetHilitedFeature(0);  // 框选无单一 hilite 焦点
    Application::Get().GetViewManager().OnSelectionSetChanged();
    return true;
}

// Ported from: SelectionTool.selectByPointsEnd (:371-391).
bool SelectionTool::selectByPointsEnd(BeButtonEvent const& ev)
{
    if (!m_isSelectByPoints)
        return false;

    Viewport* vp = ev.viewport;
    if (vp == nullptr) {
        m_isSelectByPoints = false;
        m_points.clear();
        return false;
    }

    dqGeom::Point3d const origin = vp->WorldToView(m_points[0]);
    dqGeom::Point3d const corner = vp->WorldToView(ev.point);
    // :383-386 —— Line 方法或 Pick 方法 + Reset 键 → 跨线；否则 Box。
    // DanQing 缺省方法 Pick；Reset 起拖走 Line（参考同款）。
    if (BeButton::Reset == ev.button) {
        // Line 模式（:702-717——投影距离 <1.5 device px 的像素集）。
        // EQUIVALENCE：DanQing PickAtRect 无逐像素位置面——跨线经沿线采样
        // 段序列的矩形读近似（步长 1px，段半径 2px）。
        double const dx = corner.x - origin.x;
        double const dy = corner.y - origin.y;
        int32_t const steps = std::max(
            1, static_cast<int32_t>(std::max(std::abs(dx), std::abs(dy))));
        std::vector<uint32_t> contents;
        for (int32_t i = 0; i <= steps; ++i) {
            double const t = steps == 0 ? 0.0
                : static_cast<double>(i) / static_cast<double>(steps);
            int32_t const px = static_cast<int32_t>(std::lround(
                origin.x + t * dx));
            int32_t const py = static_cast<int32_t>(std::lround(
                origin.y + t * dy));
            vp->PickAtRect(px - 2, py - 2, px + 2, py + 2, contents);
        }
        // 复用 process 的 ctrl 分流（构造合成事件直驱）。
        BeButtonEvent lineEv = ev;
        selectByPointsProcess(origin, corner, lineEv, SelectionMethod::Line,
                              true);
        // Line 模式已直接处理——selectByPointsProcess 的 Box 收缩不适用。
        m_isSelectByPoints = false;
        m_points.clear();
        vp->InvalidateDecorations();
        return true;
    }
    selectByPointsProcess(origin, corner, ev, SelectionMethod::Box,
                          useOverlapSelection(ev));

    m_isSelectByPoints = false;
    m_points.clear();
    vp->InvalidateDecorations();
    return true;
}

// Ported from: SelectionTool.onMouseStartDrag (:428-435).
EventHandled SelectionTool::onMouseStartDrag(BeButtonEvent const& event)
{
    // :432-433 —— touch + Pick 方法早退（无 touch 输入面，不可达）。
    return selectByPointsStart(event) ? EventHandled::Yes : EventHandled::No;
}

// Ported from: SelectionTool.onMouseEndDrag (:437-439).
EventHandled SelectionTool::onMouseEndDrag(BeButtonEvent const& event)
{
    return selectByPointsEnd(event) ? EventHandled::Yes : EventHandled::No;
}

// Ported from: SelectionTool.onMouseMotion (:393-396 — 框选中的失效刷新).
void SelectionTool::onMouseMotion(BeButtonEvent const& event)
{
    if (event.viewport != nullptr && m_isSelectByPoints)
        event.viewport->InvalidateDecorations();
}

// Ported from: SelectionTool.selectByPointsDecorate (:287-327).
void SelectionTool::decorate(DecorateContext& context)
{
    if (!m_isSelectByPoints)
        return;

    Viewport* vp = GetViewport();
    if (vp == nullptr || &context.GetViewport() != vp)
        return;

    // 框选光标角点 = m_points[0]（拖拽中经 InvalidateDecorations 每 motion
    // 重画——参考 fillEventFromCursorLocation 的当前光标面以最近 hover 点
    // 近似[EQUIVALENCE：InputState.lastMotion]）。
    auto& inputState = Application::Get().GetToolAdmin().currentInputState();
    dqGeom::Point2d const lastMotion2 = inputState.lastMotion;
    bool const crossingLine = false;  // Pick 方法 + Data 键（Reset 起拖的
                                      // Line 形态在 End 即清——装饰窗口内恒 Box）
    bool const overlapSelection =
        crossingLine || (lastMotion2.x < vp->WorldToView(m_points[0]).x);

    dqGeom::Point3d position = vp->WorldToView(m_points[0]);
    position.x = std::floor(position.x) + 0.5;
    position.y = std::floor(position.y) + 0.5;
    double const x2 = std::floor(lastMotion2.x) + 0.5;
    double const y2 = std::floor(lastMotion2.y) + 0.5;
    dqGeom::Point3d const offset(x2 - position.x, y2 - position.y, 0.0);

    // bestContrastIsBlack（:297——黑底取黑框；DanQing 白底视图恒白框近似
    // [EQUIVALENCE：getContrastToBackgroundColor 未移植]）。
    dqRender::CanvasDecoration decoration;
    decoration.position = dqGeom::Point2d::From(position.x, position.y);
    decoration.drawDecoration = [offset, crossingLine, overlapSelection](
                                    dqRender::CanvasContext& ctx) {
        ctx.setLineWidth(1);
        ctx.setStrokeStyle(dqCommon::ColorDef::from(255, 255, 255));
        // overlap 形态参考画虚线（setLineDash [5,5] :313）——CanvasContext
        // 无 dash 面，实线（EQUIVALENCE 见文件头）。
        (void)overlapSelection;
        if (crossingLine) {
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(offset.x, offset.y);
            ctx.stroke();
        } else {
            // strokeRect(0,0,offset) 经路径 + stroke；fillRect .06 fill。
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(offset.x, 0);
            ctx.lineTo(offset.x, offset.y);
            ctx.lineTo(0, offset.y);
            ctx.lineTo(0, 0);
            ctx.stroke();
            ctx.setGlobalAlpha(0.06);
            ctx.setFillStyle(dqCommon::ColorDef::from(255, 255, 255));
            ctx.fill();
        }
    };
    context.AddCanvasDecoration(std::move(decoration));
}

Viewport* SelectionTool::GetViewport() const
{
    auto& vm = Application::Get().GetViewManager();
    return vm.GetActiveViewport();
}

}  // namespace dqApp
