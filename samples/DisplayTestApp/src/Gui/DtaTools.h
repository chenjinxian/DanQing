// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — DTA tool registration + status-bar assembly
// Ported from: itwinjs-core display-test-app App.ts:393-458 (SVTTools — the app's
// tool registration sweep: every frontend tool class is registered in one place)
//              + index.html status-bar div (keyin-entry / fps-container /
//              tileLoadIndicatorContainer — Surface.ts:52-60 mounts them).
#pragma once

#include <InputHint.h>

#include <dqApp/ToolAssistance.h>  // M-O(2) 3i——payload→InputHints 转换签名面

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

// The status-bar InputHints form of a ToolAssistance payload. Ported from:
// ViewTool.provideToolAssistance 的宿主呈现半边（ViewTool.ts:630-655——
// 主指令 + mouse 段键帽；touch 段不呈现[InputHints 是桌面键帽面]）。
// M-O(2) 3i：入参从 install-time toolId 表（M-O(1) 过渡态）改为引擎事件
// payload（mid-tool 跃迁随引擎调用点直达）。Factored out as the testable
// half of setupToolAssistanceHints.
std::list<InputHint> toolAssistanceHintsFor(
    dqApp::ToolAssistanceInstructions const& instructions);

// Wire the engine tool-assistance channel to the status-bar input hints widget:
// NotificationManager.OnToolAssistance → showHints（engine provideToolAssistance
// 实装后的动态面——install 与 mid-tool 跃迁统一）；工具切换（无 prompt 的
// 工具不发事件）经 ToolAdmin.OnActiveToolChanged 清面。Display half on
// MainWindow::showHints — the InputHintWidget chain.
void setupToolAssistanceHints();

// Wire the hover decoration tooltip to QToolTip (the host render half of the
// engine's NotificationManager tooltip face — DTA Notifications.ts:106-125
// _showToolTip 的 HTML-div 等价物；offset (+15,-20) 对齐参考 div 定位)。
void setupDecorationToolTip();

// Wire the MDI-applicable subset of the DTA keyboard shortcuts
// (Surface.ts:229-291 — Ctrl+[ / Ctrl+] focus cycling, Ctrl+\ clone,
// Ctrl+| close; the pin/dock single-keys are floating-window-only, ➖).
void setupDtaShortcuts(MainWindow* mainWindow);

}  // namespace Gui
