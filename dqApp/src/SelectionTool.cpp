// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — SelectionTool implementation
// Ported from: itwinjs-core core/frontend/src/tools/SelectTool.ts
#include "SelectionTool.h"
#include "dqApp/Application.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewManager.h"
#include "dqApp/IModelConnection.h"

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
        selSet.Replace(ids);
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
    // Ported from: SelectTool.ts:472-477 selectByPoints cleanup.
    //               Step 3: _isSelectByPoints is never set (drag-selection
    //               infrastructure is TODO), so this branch is unreachable.
    // if (this._isSelectByPoints) {
    //     if (undefined !== ev.viewport) ev.viewport.invalidateDecorations();
    //     this.initSelectTool();
    //     return EventHandled::Yes;
    // }

    // Ported from: SelectTool.ts:480-502 overlapping-hit cycling.
    //               const lastHit = (SelectionMode.Remove === this.selectionMode)
    //                   ? undefined : IModelApp.locateManager.currHit;
    //               if (lastHit && this.iModel.selectionSet.elements.has(lastHit.sourceId)) {
    //                 ... IModelApp.accuSnap.currHit; doLocate cycling; processSelection ...
    //               }
    // TODO Step 4+: faithful hit-cycling requires LocateManager.currHit +
    //               AccuSnap.currHit + doLocate + processSelection ports.
    //               Step 3 has none of these, so the reference's hit-cycling
    //               body is unreachable in this port.

    // Ported from: SelectTool.ts:504-505 selectDecoration fallback.
    //               if (EventHandled.Yes === await this.selectDecoration(
    //                       ev, IModelApp.accuSnap.currHit))
    //                   return EventHandled.Yes;
    // TODO: HitDetail + selectDecoration port (faithful decoration pick).

    // Ported from: SelectTool.ts:507 accuSnap.resetButton.
    //               await IModelApp.accuSnap.resetButton();
    // TODO: AccuSnap.resetButton port (Step 3 AccuSnap is a no-op stub).

    // Ported from: SelectTool.ts:508 return EventHandled.Yes.
    //               The reference's onResetButtonUp returns Yes on every
    //               terminal branch (cleanup, hit-cycling handled,
    //               decoration handled, fallback). The Step 3 stub preserves
    //               that contract without altering selection state.
    (void)event;
    return EventHandled::Yes;
}

Viewport* SelectionTool::GetViewport() const
{
    auto& vm = Application::Get().GetViewManager();
    return vm.GetActiveViewport();
}

}  // namespace dqApp
