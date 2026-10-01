// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — DTA tool registration + status-bar assembly
// Ported from: itwinjs-core display-test-app App.ts:393-458 (SVTTools — the app's
// tool registration sweep: every frontend tool class is registered in one place)
//              + index.html status-bar div (keyin-entry / fps-container /
//              tileLoadIndicatorContainer — Surface.ts:52-60 mounts them).
#pragma once

#include <InputHint.h>

#include <list>
#include <string>

namespace Gui {
class MainWindow;

// Register the app tools into the dqApp tool registry (the SVTTools sweep).
// Ported from: App.ts:393-458 — `[...].forEach((tool) => tool.register(svtToolNamespace))`.
void registerDtaTools();

// Mount the status-bar trio of the DTA surface: key-in field, FPS monitor, tile
// load indicator. Ported from: Surface.ts:52-60 (index.html status-bar container
// children, in DOM order: keyin-entry, fps-container, tileLoadIndicatorContainer;
// snapModesContainer is not wired — AccuSnap mode switching surface is registered
// as a remaining gap).
void setupDtaStatusBar(MainWindow* mainWindow);

// The tool-assistance hints for a tool id (empty when the tool has no prompt
// table entry). Ported from: ViewTool.provideToolAssistance (ViewTool.ts:628-655)
// — main instruction + mouse Accept/Exit sections. Factored out as the testable
// half of setupToolAssistanceHints (the reference's engine-side call sites update
// prompts mid-tool; that chain is the registered gap).
std::list<InputHint> toolAssistanceHintsFor(std::string const& toolId);

// Wire the active view tool's tool-assistance prompts to the status-bar input
// hints widget via ToolAdmin.OnActiveToolChanged (display half on
// MainWindow::showHints — the InputHintWidget chain).
void setupToolAssistanceHints();

// Wire the hover decoration tooltip to QToolTip (the host render half of the
// engine's NotificationManager tooltip face — DTA Notifications.ts:106-125
// _showToolTip 的 HTML-div 等价物；offset (+15,-20) 对齐参考 div 定位)。
void setupDecorationToolTip();

}  // namespace Gui
