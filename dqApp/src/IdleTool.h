// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — IdleTool
//
// Ported from: itwinjs-core core/frontend/src/tools/IdleTool.ts
//              IdleTool class (line 33) — onMouseStartDrag (37-85),
//              onMiddleButtonUp (87-101), onMouseWheel (103).
//
// Task 12 rewrote the bodies from the Task 2 preservation stubs to the
// faithful itwinjs surface: middle-mouse drag/wheel/double-click now drive
// the registered View.* tools (View.Pan / View.Rotate / View.Scroll /
// View.Fit) through ToolAdmin's registry + run() + startHandleDrag.
#pragma once

#include "dqApp/ToolAdmin.h"

namespace dqApp {

class Viewport;

// IdleTool — handles camera interaction when no other tool is active.
// Ported from: itwinjs-core IdleTool (IdleTool.ts:33)
class IdleTool : public InteractiveTool {
public:
    // Ported from: itwinjs-core IdleTool.toolId (IdleTool.ts:34)
    static constexpr const char* ToolId = "Idle";

    const char* getToolId() const override { return ToolId; }

    // Ported from: itwinjs-core IdleTool.onMouseStartDrag() (IdleTool.ts:37-85).
    // Middle+Ctrl→View.Look/View.Scroll, Middle+Shift→View.Rotate, plain
    // Middle→View.Pan; Data (no activeTool)→View.Rotate; Reset (no activeTool)
    // →View.Pan. If a viewTool is already installed and is a ViewManip, forwards
    // the drag with the resolved handle type; otherwise creates the resolved
    // View.* tool via ToolAdmin's registry (oneShot=true, isDraggingRequired=
    // true), runs it, then startHandleDrag(ev).
    EventHandled onMouseStartDrag(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core IdleTool.onMiddleButtonUp() (IdleTool.ts:87-101).
    // isDoubleClick→FitViewTool.run(); else (no ctrl/shift)→
    // tentativePoint.process(ev) (Step 3 stub).
    EventHandled onMiddleButtonUp(BeButtonEvent const& event) override;

    // Ported from: itwinjs-core IdleTool.onMouseWheel() (IdleTool.ts:103).
    // Delegates directly to ToolAdmin.processWheelEvent(ev, true).
    EventHandled onMouseWheel(BeWheelEvent& event) override;
};

}  // namespace dqApp
