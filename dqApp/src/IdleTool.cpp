// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — IdleTool implementation
// Ported from: itwinjs-core core/frontend/src/tools/IdleTool.ts
//              onMouseStartDrag (IdleTool.ts:37-85),
//              onMiddleButtonUp  (IdleTool.ts:87-101),
//              onMouseWheel      (IdleTool.ts:103).
#include "IdleTool.h"
#include "dqApp/Application.h"
#include "dqApp/ToolAdmin.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewState.h"
#include "dqApp/ViewTool.h"  // ViewManip::startHandleDrag + FitViewTool + ViewHandleType.

namespace dqApp {

// ---------------------------------------------------------------------------
// onMouseStartDrag — faithful port of IdleTool.ts:37-85.
//
// Reference shape (TS):
//   if (!ev.viewport) return EventHandled.No;
//   let toolId; let handleId;
//   switch (ev.button) {
//     case BeButton.Middle:
//       if (ev.isControlKey) {
//         toolId = ev.viewport.view.allow3dManipulations() ? "View.Look" : "View.Scroll";
//         handleId = ... ? ViewHandleType.Look : ViewHandleType.Scroll;
//       } else if (ev.isShiftKey) {
//         toolId = "View.Rotate"; handleId = ViewHandleType.Rotate;
//       } else {
//         toolId = "View.Pan"; handleId = ViewHandleType.Pan;
//       }
//       break;
//     case BeButton.Data:
//       if (undefined !== IModelApp.toolAdmin.activeTool) return EventHandled.No;
//       toolId = "View.Rotate"; handleId = ViewHandleType.Rotate; break;
//     default:  // BeButton.Reset
//       if (undefined !== IModelApp.toolAdmin.activeTool) return EventHandled.No;
//       toolId = "View.Pan"; handleId = ViewHandleType.Pan; break;
//   }
//   const currTool = IModelApp.toolAdmin.viewTool;
//   if (currTool) {
//     if (currTool instanceof ViewManip)
//       return currTool.startHandleDrag(ev, handleId);
//     return EventHandled.No;
//   }
//   const viewTool = IModelApp.tools.create(toolId, ev.viewport, true, true);
//   if (viewTool && await viewTool.run()) return viewTool.startHandleDrag(ev);
//   return EventHandled.Yes;
//
// Step 3 deferrals / adaptations:
//  - View.Look was deferred in Step 3 (ViewLook handle outside Task 10 scope);
//    WindowArea/Look W2 ported ViewLook + LookViewTool and restored the
//    reference Ctrl+Middle branch (:47-48) below — allow3dManipulations() ?
//    "View.Look" : "View.Scroll".
//  - currTool instanceof ViewManip: in Step 3 the viewTool slot is only ever
//    populated by ViewManip subclasses (Pan/Rotate/Scroll via this code path)
//    or FitViewTool. FitViewTool is oneShot in this code path and exits
//    synchronously inside run()→onPostInstall()→doFit(oneShot=true)→exitTool(),
//    so by the time onMouseStartDrag fires again the slot is null. Therefore
//    a non-null currTool is always a ViewManip in Step 3, and the
//    static_cast<ViewManip*>(currTool) below is safe. If the assumption breaks
//    (non-oneShot FitViewTool in the slot), an asViewManip() virtual should
//    be added to ViewTool — TODO note for a future task.
// ---------------------------------------------------------------------------
EventHandled IdleTool::onMouseStartDrag(BeButtonEvent const& ev)
{
    // TS L38-39: if (!ev.viewport) return EventHandled.No;
    if (!ev.viewport)
        return EventHandled::No;

    // TS L41-42: let toolId; let handleId;
    char const* toolId = nullptr;
    std::optional<ViewHandleType> handleId;

    // TS L44-73: switch (ev.button) { ... }
    switch (ev.button) {
        case BeButton::Middle:
            // TS L46: if (ev.isControlKey) — BeButtonEvent has no isControlKey
            // field in DanQing; keyModifiers carries the modifier mask.
            if ((ev.keyModifiers & BeModifierKeys::Control) != BeModifierKeys::None) {
                // TS L47-48: toolId = ev.viewport.view.allow3dManipulations()
                //                          ? "View.Look" : "View.Scroll";
                //             handleId = ... ? ViewHandleType.Look : ViewHandleType.Scroll;
                // Reference branch restored (WindowArea/Look W2 — ViewLook handle
                // + LookViewTool ported, "View.Look" registered). DanQing carries
                // allow3dManipulations on ViewState3d (the reference's ViewState
                // base declares it abstract; ViewState2d returns false), so a null
                // or 2d view takes the Scroll branch — faithful to the reference.
                ViewState* viewBase = ev.viewport->GetView();
                ViewState3d* view3d = viewBase ? viewBase->AsViewState3d() : nullptr;
                bool const allow3d = (view3d != nullptr) && view3d->Allow3dManipulations();
                toolId = allow3d ? "View.Look" : "View.Scroll";
                handleId = allow3d ? ViewHandleType::Look : ViewHandleType::Scroll;
            } else if ((ev.keyModifiers & BeModifierKeys::Shift) != BeModifierKeys::None) {
                // TS L49-50: toolId = "View.Rotate"; handleId = Rotate;
                toolId = "View.Rotate";
                handleId = ViewHandleType::Rotate;
            } else {
                // TS L52-54: toolId = "View.Pan"; handleId = Pan;
                toolId = "View.Pan";
                handleId = ViewHandleType::Pan;
            }
            break;

        case BeButton::Data:
            // TS L58-61: when an active tool is present, IdleTool declines so the
            // gesture stays with the active tool.
            if (Application::Get().GetToolAdmin().activeTool() != nullptr)
                return EventHandled::No;
            // TS L62-63: toolId = "View.Rotate"; handleId = Rotate;
            toolId = "View.Rotate";
            handleId = ViewHandleType::Rotate;
            break;

        default:  // BeButton::Reset
            // TS L67-69: same activeTool guard as the Data case.
            if (Application::Get().GetToolAdmin().activeTool() != nullptr)
                return EventHandled::No;
            // TS L70-71: toolId = "View.Pan"; handleId = Pan;
            toolId = "View.Pan";
            handleId = ViewHandleType::Pan;
            break;
    }

    auto& toolAdmin = Application::Get().GetToolAdmin();

    // TS L75-80: if (currTool) { if (currTool instanceof ViewManip)
    //                          return currTool.startHandleDrag(ev, handleId);
    //                          return EventHandled.No; }
    ViewTool* currTool = toolAdmin.GetViewTool();
    if (currTool) {
        // See "Step 3 deferrals" in the header comment: a non-null currTool is
        // always a ViewManip in Step 3, so the static_cast is safe. The
        // reference's `instanceof ViewManip` guard reduces to a null-check here.
        ViewManip* manip = static_cast<ViewManip*>(currTool);
        // TS L78: return currTool.startHandleDrag(ev, handleId);
        // startHandleDrag returns EventHandled::No if inHandleModify (which the
        // reference also surfaces via the same startHandleDrag call).
        return manip->startHandleDrag(ev, handleId);
        // NOTE: the reference deliberately leaves currTool active regardless of
        // startHandleDrag's return (the comment at TS L78 says "leave it active
        // regardless"); the return short-circuits without installing a new tool.
    }

    // TS L81-84: const viewTool = IModelApp.tools.create(toolId, ev.viewport,
    //                                                    true, true);
    //             if (viewTool && await viewTool.run())
    //               return viewTool.startHandleDrag(ev);
    //             return EventHandled.Yes;
    InteractiveTool* created = toolAdmin.GetRegistry().CreateVP(
        toolId, ev.viewport, /*oneShot=*/true, /*isDraggingRequired=*/true);
    if (!created)
        return EventHandled::Yes;  // TS L84 fallback when create returns undefined.

    // TS L82: await viewTool.run() — ViewTool::run returns bool (faithful
    // mapping of `Promise<boolean>` per §3.4). On false, the tool did not
    // install; surface as EventHandled::Yes (the reference returns the same
    // after a failed run, falling through to L84).
    ViewTool* viewTool = static_cast<ViewTool*>(created);
    if (!viewTool->run())
        return EventHandled::Yes;

    // TS L83: return viewTool.startHandleDrag(ev);
    // No forced handle — the ViewManip uses its configured handleMask to pick
    // the active handle. ViewManip::startHandleDrag(std::nullopt) is the
    // matching overload (ViewTool.h:485-486).
    ViewManip* manip = static_cast<ViewManip*>(viewTool);
    return manip->startHandleDrag(ev);
}

// ---------------------------------------------------------------------------
// onMiddleButtonUp — faithful port of IdleTool.ts:87-101.
//
// Reference shape (TS):
//   if (!ev.viewport) return EventHandled.No;
//   if (ev.isDoubleClick) {
//     const viewTool = new FitViewTool(ev.viewport, true);
//     return await viewTool.run() ? EventHandled.Yes : EventHandled.No;
//   }
//   if (ev.isControlKey || ev.isShiftKey) return EventHandled.No;
//   IModelApp.tentativePoint.process(ev);
//   return EventHandled.Yes;
//
// Step 3 adaptation: FitViewTool construction uses ToolAdmin::GetRegistry
// ::CreateVP("View.Fit", vp, /*oneShot=*/true, /*isDraggingRequired=*/true)
// (per Task 12 brief) rather than a direct `new FitViewTool`. The factory
// ignores isDraggingRequired (FitViewTool ctor takes only vp + oneShot) and
// returns a FitViewTool* whose run() drives onPostInstall → doFit → exitTool.
// ---------------------------------------------------------------------------
EventHandled IdleTool::onMiddleButtonUp(BeButtonEvent const& ev)
{
    // TS L88-89: if (!ev.viewport) return EventHandled.No;
    if (!ev.viewport)
        return EventHandled::No;

    // TS L91-94: if (ev.isDoubleClick) { new FitViewTool(vp, true); run; ... }
    if (ev.isDoubleClick) {
        InteractiveTool* fit = Application::Get().GetToolAdmin().GetRegistry().CreateVP(
            "View.Fit", ev.viewport, /*oneShot=*/true, /*isDraggingRequired=*/true);
        if (!fit)
            return EventHandled::No;
        // TS L93: return await viewTool.run() ? Yes : No;
        bool const ok = static_cast<ViewTool*>(fit)->run();
        return ok ? EventHandled::Yes : EventHandled::No;
    }

    // TS L96-97: if (ev.isControlKey || ev.isShiftKey) return EventHandled.No;
    if ((ev.keyModifiers & (BeModifierKeys::Control | BeModifierKeys::Shift)) != BeModifierKeys::None)
        return EventHandled::No;

    // TS L99: IModelApp.tentativePoint.process(ev);
    // TentativePoint::process is a Step 3 no-op stub (TentativePoint.h:44) —
    // faithful to the reference's early-return when no viewTool is in dynamic
    // update. Returns void (the reference's call site ignores its return).
    Application::Get().GetTentativePoint().process(ev);

    // TS L100: return EventHandled.Yes;
    return EventHandled::Yes;
}

// ---------------------------------------------------------------------------
// onMouseWheel — faithful port of IdleTool.ts:103.
//   public override async onMouseWheel(ev: BeWheelEvent) {
//     return IModelApp.toolAdmin.processWheelEvent(ev, true);
//   }
// ---------------------------------------------------------------------------
EventHandled IdleTool::onMouseWheel(BeWheelEvent& ev)
{
    // Ported from: itwinjs-core IdleTool.onMouseWheel (IdleTool.ts:103).
    // The base InteractiveTool::onMouseWheel signature was relaxed to non-const
    // BeWheelEvent& in Task 15 to match the reference's mutable-ev contract —
    // ToolAdmin.processWheelEvent (ToolAdmin.ts:1965) writes back into ev via
    // WheelEventProcessor::doZoom. IdleTool itself does not modify ev.
    return Application::Get().GetToolAdmin().processWheelEvent(ev, /*doUpdate=*/true);
}

}  // namespace dqApp
