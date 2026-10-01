// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — DTA tool registration + status-bar assembly
// implementation
// Ported from: itwinjs-core display-test-app App.ts:393-458 + index.html
// status-bar div + Surface.ts:52-60; tool-assistance display half from
// ViewTool.ts:628-655 (see setupToolAssistanceHints).
#include "DtaTools.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QObject>
#include <QPoint>
#include <QShortcut>
#include <QToolTip>
#include <QWidget>

#include <QCursor>

#include <cstring>

#include <vector>

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewTool.h>

#include "FpsMonitor.h"
#include "KeyinField.h"
#include "MainWindow.h"
#include "SaveImageTool.h"
#include "SnapModeTool.h"
#include "SyncViewportsTool.h"
#include "ZoomToSelectedTool.h"
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
    // M-M(6)：Snap modes 接线（App.ts:486-489 setActiveSnapMode 的 keyin 形态）。
    registry.Register("SetActiveSnapMode",
                      []() -> dqApp::InteractiveTool* { return new SetActiveSnapModeTool(); },
                      "dta snapmode");
    // M-N(2)：ZoomToSelectedElements（Viewer.ts:49-89——keyin "dta zoom selected"）。
    registry.Register("ZoomToSelectedElements",
                      []() -> dqApp::InteractiveTool* { return new ZoomToSelectedElementsTool(); },
                      "dta zoom selected");
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

    // Ported from: Surface.ts:53 addSnapModes(document.getElementById(
    // "snapModesContainer")) + SnapModes.ts:30-50 — the snap-mode combo box
    // ("Snap Mode: " label + 8 entries; Multi-snap = the 7-mode array of
    // SnapModes.ts:10-18).
    {
        auto* snapBox = new QWidget(mainWindow);
        auto* snapLayout = new QHBoxLayout(snapBox);
        snapLayout->setContentsMargins(0, 0, 0, 0);
        snapLayout->addWidget(new QLabel(QObject::tr("Snap Mode: "), snapBox));
        auto* combo = new QComboBox(snapBox);
        combo->setObjectName(QStringLiteral("snapModes"));
        // SnapModes.ts:38-46 entries（userData = SnapMode 位值；Multi-snap = -1）。
        combo->addItem(QStringLiteral("Keypoint"), static_cast<int>(dqApp::SnapMode::NearestKeypoint));
        combo->addItem(QStringLiteral("Nearest"), static_cast<int>(dqApp::SnapMode::Nearest));
        combo->addItem(QStringLiteral("Center"), static_cast<int>(dqApp::SnapMode::Center));
        combo->addItem(QStringLiteral("Origin"), static_cast<int>(dqApp::SnapMode::Origin));
        combo->addItem(QStringLiteral("Intersection"), static_cast<int>(dqApp::SnapMode::Intersection));
        combo->addItem(QStringLiteral("Perpendicular Point"), static_cast<int>(dqApp::SnapMode::PerpendicularPoint));
        combo->addItem(QStringLiteral("Tangent Point"), static_cast<int>(dqApp::SnapMode::TangentPoint));
        constexpr int kMultiSnapMode = -1;  // SnapModes.ts:20
        combo->addItem(QStringLiteral("Multi-snap"), kMultiSnapMode);
        snapLayout->addWidget(combo);
        // SnapModes.ts:10-18 multiSnapModes（逐项序）。
        std::vector<dqApp::SnapMode> const kMultiSnapModes{
            dqApp::SnapMode::NearestKeypoint, dqApp::SnapMode::Nearest,
            dqApp::SnapMode::Intersection,    dqApp::SnapMode::MidPoint,
            dqApp::SnapMode::Origin,          dqApp::SnapMode::Center,
            dqApp::SnapMode::Bisector,
        };
        // SnapModes.ts:22-28 changeSnapModes.
        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), mainWindow,
                         [combo, kMultiSnapModes](int index) {
                             int const value = combo->itemData(index).toInt();
                             if (kMultiSnapMode != value)
                                 dqApp::Application::Get().GetAccuSnap().setActiveSnapMode(
                                     static_cast<dqApp::SnapMode>(value));
                             else
                                 dqApp::Application::Get().GetAccuSnap().setActiveSnapModes(kMultiSnapModes);
                         });
        snapBox->setWindowTitle(QObject::tr("Snap Mode"));
        mainWindow->addStatusBarItem(
            snapBox, StatusBarItemSpec("SnapModes", QString(), StatusBarSlot::Left, -2, true, 0));
    }

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

void setupDecorationToolTip()
{
    // M-O(1) I3：hover 装饰 tooltip 宿主半边。参考链 = AccuSnap.displayToolTip
    // → vp.openToolTip → IModelApp.notifications.showToolTip → DTA
    // Notifications.ts:106-125 _showToolTip（div 定位于 hover 点 (x+15, y-20)）。
    // 引擎半边在 Viewport hover locate（renderFrame Step 13）；本函数订阅
    // OnToolTip → QToolTip（+15/-20 偏移对齐参考）。EQUIVALENCE: 参考锚定
    // 视口内坐标（div absolute 于 canvas），Qt 宿主以光标全局位定位——hover
    // 点即光标位，视觉等价；验证法=DtaToolsWiring 引擎事件锁（消息+坐标）。
    auto& notifications = dqApp::Application::Get().GetNotificationManager();
    notifications.SetToolTipSupported(true);   // DTA isToolTipSupported 覆写等价物
    notifications.OnToolTip.AddListener([](std::string const& message, double, double) {
        QToolTip::showText(QCursor::pos() + QPoint(15, -20), QString::fromStdString(message));
    });
    notifications.OnToolTipCleared.AddListener([]() { QToolTip::hideText(); });
}

}  // namespace Gui
