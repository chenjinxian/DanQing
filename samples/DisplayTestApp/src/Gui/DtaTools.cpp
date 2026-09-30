// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — DTA tool registration + status-bar assembly
// implementation
// Ported from: itwinjs-core display-test-app App.ts:393-458 + index.html
// status-bar div + Surface.ts:52-60; tool-assistance display half from
// ViewTool.ts:628-655 (see setupToolAssistanceHints).
#include "DtaTools.h"

#include <QKeySequence>
#include <QLabel>
#include <QObject>
#include <QShortcut>

#include <cstring>

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewTool.h>

#include "FpsMonitor.h"
#include "KeyinField.h"
#include "MainWindow.h"
#include "SaveImageTool.h"
#include "SyncViewportsTool.h"
#include "TileLoadIndicator.h"

namespace Gui {

void registerDtaTools()
{
    // Ported from: App.ts:393-458 — the app tool sweep registers every frontend
    // tool class; the M-L(3) wiring subset below are the tools whose engine
    // surfaces exist in DanQing (SaveImageTool / RecordFpsTool / SyncViewportsTool
    // / SyncViewportFrustaTool). The keyin strings are the reference en-locale
    // values (SVTTools.json).
    auto& registry = dqApp::Application::Get().GetToolAdmin().GetRegistry();
    registry.Register("SaveImage", []() -> dqApp::InteractiveTool* { return new SaveImageTool(); },
                      "dta save image");
    registry.Register("RecordFps", []() -> dqApp::InteractiveTool* { return new RecordFpsTool(); },
                      "dta record fps");
    registry.Register("SyncViewports",
                      []() -> dqApp::InteractiveTool* { return new SyncViewportsTool(); },
                      "dta viewport sync");
    registry.Register("SyncFrusta",
                      []() -> dqApp::InteractiveTool* { return new SyncViewportFrustaTool(); },
                      "dta frustum sync");
}

void setupDtaStatusBar(MainWindow* mainWindow)
{
    if (!mainWindow)
        return;

    // Ported from: index.html status-bar div children (Surface.ts:52-60 mounts
    // them) — DOM order: keyin-entry, fps-container, tileLoadIndicatorContainer.
    // The DTA container sits above the tool bar; DanQing's equivalent chrome
    // surface is the MainWindow status bar (addStatusBarItem registry).
    auto* keyin = new KeyinField(50);  // Surface.ts:58 — historyLength: 50
    keyin->setWindowTitle(QObject::tr("Keyin"));
    mainWindow->addStatusBarItem(
        keyin, StatusBarItemSpec("Keyin", QString(), StatusBarSlot::Left, -5, true, 0));

    auto* fps = new FpsMonitor;
    fps->setWindowTitle(QObject::tr("FPS"));
    mainWindow->addStatusBarItem(
        fps, StatusBarItemSpec("FpsMonitor", QString(), StatusBarSlot::Left, -4, true, 0));

    auto* tiles = new TileLoadIndicator;
    mainWindow->addStatusBarItem(
        tiles, StatusBarItemSpec("TileLoadIndicator", QString(), StatusBarSlot::Left, -3, true, 0));

    // Ported from: Surface.ts:229-291 keyboard shortcuts — "`" focuses the key-in
    // field (the field itself handles Escape/` as lose-focus).
    auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("`")), mainWindow);
    shortcut->setObjectName(QStringLiteral("DTA.KeyinFocus"));
    shortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(shortcut, &QShortcut::activated, keyin, &KeyinField::focusField);
}

std::list<InputHint> toolAssistanceHintsFor(std::string const& toolId)
{
    // Prompt strings = the reference en-locale values (core/frontend/src/public/
    // locales/en/CoreTools.json — tools.View.<Tool>.Prompts.FirstPoint and
    // tools.ElementSet.Inputs.{AcceptPoint,Exit}).
    struct ToolHints {
        const char* toolId;
        const char* mainPrompt;
    };
    static const ToolHints kHints[] = {
        { "View.Pan", "Define point to pan from" },
        { "View.Rotate", "Identify point on element to rotate about" },
        { "View.Look", "Enter point to begin looking around" },
        { "View.Scroll", "Enter point to start scrolling" },
        { "View.Fit", "Select view to fit" },
        { "View.WindowArea", "Define first corner point" },
    };

    for (auto const& entry : kHints) {
        if (entry.toolId != toolId)
            continue;
        // ViewTool.ts:630-641 — mouse sections: LeftClick = AcceptPoint,
        // RightClick = Exit (the %1 placeholder renders as the keycap).
        return {
            InputHint{ QString::fromUtf8(entry.mainPrompt), {} },
            InputHint{ QStringLiteral("%1 Accept point"),
                       { InputHint::InputSequence(InputHint::UserInput::MouseLeft) } },
            InputHint{ QStringLiteral("%1 Exit"),
                       { InputHint::InputSequence(InputHint::UserInput::MouseRight) } },
        };
    }
    return {};
}

void setupToolAssistanceHints()
{
    // Ported from: ViewTool.provideToolAssistance (ViewTool.ts:628-655) — the
    // main instruction (per-tool prompt key) + mouse Accept/Exit sections, shown
    // through notifications.setToolAssistance. DanQing's display surface is the
    // status-bar InputHints widget (MainWindow::showHints). The reference
    // prompts update mid-tool (e.g. WindowArea FirstPoint → NextPoint,
    // ViewTool.ts:3564) from the engine-side provideToolAssistance call sites;
    // DanQing's engine stubs are registered as the remaining gap — this wiring
    // shows the install-time (FirstPoint) prompt.
    dqApp::Application::Get().GetToolAdmin().OnActiveToolChanged.AddListener([]() {
        auto* mw = MainWindow::getInstance();
        if (!mw)
            return;

        dqApp::InteractiveTool* tool
            = dqApp::Application::Get().GetToolAdmin().activeTool();
        if (nullptr == tool) {
            mw->hideHints();
            return;
        }

        auto hints = toolAssistanceHintsFor(tool->getToolId());
        if (!hints.empty())
            mw->showHints(hints);
        else
            mw->hideHints();  // no prompt table entry (one-shot tools, Select/Idle)
    });
}

}  // namespace Gui
