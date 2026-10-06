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
#include <QMdiSubWindow>
#include <QMainWindow>
#include <QObject>
#include <QPoint>
#include <QShortcut>
#include <QToolBar>
#include <QToolTip>
#include <QWidget>

#include <QCursor>

#include <cstring>

#include <functional>
#include <optional>
#include <vector>

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewTool.h>

#include "FpsMonitor.h"
#include "Application.h"
#include "GltfDecorationTool.h"
#include "OutputShadersTool.h"        // M-O(2) 3d OutputShaders 工具注册
#include "TiledGraphics.h"            // M-O(2) I11 第二 iModel 叠加工具注册
#include "MacroTool.h"                // M-O(4) P9 Macro 播放器注册
#include "CesiumExampleTool.h"        // M-O(4) P8 Cesium 陈列馆注册
#include "GridSettingsTool.h"
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
    // M-O(1) I5：ChangeGridSettingsTool（Grid.ts:11-94——keyin "dta grid settings"）。
    registry.Register("GridSettings",
                      []() -> dqApp::InteractiveTool* { return new ChangeGridSettingsTool(); },
                      "dta grid settings");
    // M-O(2) 3f：GltfDecorationTool（GltfDecoration.ts:103-228——keyin "dta gltf"
    // [SVTTools.json 无该键——以工具类名注册面为准]；i>/s>/c>/r>/f> 实例化参数面）。
    registry.Register("AddGltfDecoration",
                      []() -> dqApp::InteractiveTool* { return new GltfDecorationTool(); },
                      "dta gltf");
    // M-O(2) 3d：OutputShadersTool（OutputShadersTool.ts:322-367——keyin
    // "dta output shaders"；c 编译 + u/n/v/f/g/h 六向过滤 + d= 目录）。
    registry.Register("OutputShaders",
                      []() -> dqApp::InteractiveTool* { return new OutputShadersTool(); },
                      "dta output shaders");
    // M-O(2) I11：ToggleSecondaryIModelTool（TiledGraphics.ts:123-135——keyin
    // "dta tiled graphics"；可选参 = 包根路径）。
    registry.Register("ToggleSecondaryIModel",
                      []() -> dqApp::InteractiveTool* { return new ToggleSecondaryIModelTool(); },
                      "dta tiled graphics");
    // M-O(4) P9：MacroTool（MacroTools.ts:8-55——keyin "dta macro <file>"；
    // keyin 序列逐行播放 + 三分支告警）。
    registry.Register("Macro",
                      []() -> dqApp::InteractiveTool* { return new MacroTool(); },
                      "dta macro");
    // M-O(4) P8：CesiumExampleTool（EmptyExample.ts:14-48——keyin
    // "dta cesium example"；8 族装饰形态陈列 toggle）。
    registry.Register("CesiumExample",
                      []() -> dqApp::InteractiveTool* { return new CesiumExampleTool(); },
                      "dta cesium example");
}

void setupDtaStatusBar(MainWindow* mainWindow)
{
    if (!mainWindow)
        return;

    // M-R：DTA #status-bar 位于工具栏之上（index.html 顺序：status-bar →
    // toolBar）。Qt 形态 = 不可移动 QToolBar（objectName DTA.StatusBar），
    // 经 addToolBar 后 insertToolBar 置于 DTA 工具栏之前（菜单栏正下方）。
    // 子件序 1:1 index.html：keyin-entry | fps-container |
    // tileLoadIndicatorContainer | snapModesContainer。
    QMainWindow* mw = mainWindow;   // MainWindow 即 QMainWindow（继承）
    QToolBar* dtaStatus = new QToolBar(mw);
    dtaStatus->setObjectName(QStringLiteral("DTA.StatusBar"));
    dtaStatus->setWindowTitle(QStringLiteral("DTA Status"));
    dtaStatus->setMovable(false);
    dtaStatus->setFloatable(false);
    dtaStatus->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mw->addToolBar(Qt::TopToolBarArea, dtaStatus);
    mw->insertToolBar(dtaStatus, dtaStatus);   // 首位（后续工具栏 insertToolBar 其后）

    auto* keyin = new KeyinField(50, dtaStatus);  // Surface.ts:58 — historyLength: 50
    dtaStatus->addWidget(keyin);

    auto* fps = new FpsMonitor(dtaStatus);
    dtaStatus->addWidget(fps);

    auto* tiles = new TileLoadIndicator(dtaStatus);
    dtaStatus->addWidget(tiles);

    // Ported from: Surface.ts:53 addSnapModes(document.getElementById(
    // "snapModesContainer")) + SnapModes.ts:30-50 — the snap-mode combo box
    // ("Snap Mode: " label + 8 entries; Multi-snap = the 7-mode array of
    // SnapModes.ts:10-18).
    {
        auto* snapBox = new QWidget(dtaStatus);
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
        dtaStatus->addWidget(snapBox);
    }

    // M-R：bottomdiv 双 span（index.html :66-69 showstatus/showerror——
    // Utils.ts:8-15 showStatus 写 #showstatus、showError 写 #showerror）。
    // Qt 形态 = 底部状态栏两 QLabel（objectName 同 id）；showStatus 面 =
    // MainWindow::showStatus 既有链（M-L(3) ⑦ NotificationManager→状态栏
    // 消息），此处在底部加常驻 span 并让 showStatus 双写。
    {
        mainWindow->installDtaOutputSpans();
    }

    // Ported from: Surface.ts:229-291 keyboard shortcuts — "`" focuses the key-in
    // field (the field itself handles Escape/` as lose-focus).
    auto* shortcut = new QShortcut(QKeySequence(QStringLiteral("`")), mainWindow);
    shortcut->setObjectName(QStringLiteral("DTA.KeyinFocus"));
    shortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(shortcut, &QShortcut::activated, keyin, &KeyinField::focusField);
}

std::list<InputHint> toolAssistanceHintsFor(
    dqApp::ToolAssistanceInstructions const& instructions)
{
    // ViewTool.ts:630-655 的宿主呈现半边（M-O(2) 3i：入参 = 引擎事件
    // payload）——主指令（无输入序列）+ mouse 段键帽（%1 占位符 = 键帽位，
    // InputHintWidget 既有约定）；touch 段不呈现（InputHints 是桌面键帽面）。
    std::list<InputHint> hints;
    hints.push_back(InputHint{
        QString::fromStdString(instructions.mainInstruction.text), {} });
    if (instructions.sections.has_value()) {
        for (auto const& section : *instructions.sections) {
            for (auto const& instr : section.instructions) {
                if (dqApp::ToolAssistanceInputMethod::Touch == instr.inputMethod)
                    continue;
                // 键帽面（ToolAssistanceImage → InputHint::UserInput；参考
                // 的 WebFont 图标族中 InputHints 有对应键帽的子集）。
                std::optional<InputHint::InputSequence> sequence;
                switch (instr.image) {
                    case dqApp::ToolAssistanceImage::LeftClick:
                        sequence = InputHint::InputSequence(
                            InputHint::UserInput::MouseLeft);
                        break;
                    case dqApp::ToolAssistanceImage::RightClick:
                        sequence = InputHint::InputSequence(
                            InputHint::UserInput::MouseRight);
                        break;
                    default:
                        break;  // 无键帽对应的图标（Keyboard/拖拽族/触摸族）➖
                }
                if (!sequence.has_value())
                    continue;
                hints.push_back(InputHint{
                    QStringLiteral("%1 ")
                        + QString::fromStdString(instr.text),
                    { *sequence } });
            }
        }
    }
    return hints;
}

void setupToolAssistanceHints()
{
    // Ported from: ViewTool.provideToolAssistance (ViewTool.ts:628-655) 的
    // 消费接线（M-O(2) 3i）——引擎 provideToolAssistance 实装后经
    // NotificationManager.setToolAssistance → OnToolAssistance 扇出；本函数
    // 订阅并转 InputHints 显示（MainWindow::showHints）。mid-tool 跃迁
    //（WindowArea FirstPoint→NextPoint，ViewTool.ts:3564）随引擎调用点直达；
    // M-O(1) 3c 的 install-time 表驱动 wiring 删除（事件在
    // onReinitialize→provideInitialToolAssistance 同样覆盖安装态）。
    // EQUIVALENCE: 参考源=NotificationManager.ts:204（no-op 基类 + appui
    // 消费；DTA 无 override）；发散=DanQing 以事件为宿主缝、显示面 = 状态栏
    // InputHints；验证法=ViewToolTest ToolAssistance 引擎锁 +
    // DtaToolsWiring 显示锁。
    dqApp::Application::Get().GetNotificationManager().OnToolAssistance.AddListener(
        [](dqApp::ToolAssistanceInstructions const& instructions) {
            auto* mw = MainWindow::getInstance();
            if (!mw)
                return;
            mw->showHints(toolAssistanceHintsFor(instructions));
        });
    // 工具切换清面：无 prompt 的工具（Select/Idle/one-shot）安装时不发事件
    // ——切换即隐藏陈旧提示（参考 appui 的 tool-assistance 区随工具重置）。
    dqApp::Application::Get().GetToolAdmin().OnActiveToolChanged.AddListener([]() {
        if (auto* mw = MainWindow::getInstance())
            mw->hideHints();
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

void setupDtaShortcuts(MainWindow* mainWindow)
{
    // M-O(1) I7：DTA 快捷键族的 MDI 可用子集（Surface.ts:229-291
    // getKeyboardShortcutHandler——"`" 聚焦 keyin 已在 setupDtaStatusBar）。
    // EQUIVALENCE: 参考源=Surface.ts:240-258（浮窗模型的 focusNextOrPrevious/
    // addViewer[clone]/close）；发散=DanQing 为 MDI 模型（视图=MDI 子窗）——
    // 焦点轮换→MainWindow::activate*Window、克隆→ViewCreate 消息、关闭→活动
    // 子窗 close；Ctrl+n[聚焦通知浮窗]与 Ctrl+p/i/m/h/l/k/j[图钉/停靠 8 键]
    // 为浮窗专属（MDI 无对应——模型差异登记➖）；验证法=DtaToolsWiring
    // 快捷键注册锁 + 既有 MDIChrome/CommandWindow 行为锁。
    if (!mainWindow)
        return;
    struct Shortcut {
        char const* key;
        char const* name;
    };
    auto addShortcut = [mainWindow](char const* key, char const* name,
                                    std::function<void()> handler) {
        auto* sc = new QShortcut(QKeySequence(QString::fromLatin1(key)), mainWindow);
        sc->setObjectName(QStringLiteral("DTA.Shortcut.") + QString::fromLatin1(name));
        sc->setContext(Qt::ApplicationShortcut);
        QObject::connect(sc, &QShortcut::activated, mainWindow, std::move(handler));
    };
    // Surface.ts:241-243——Ctrl+[ 前一个 / Ctrl+] 后一个。
    addShortcut("Ctrl+[", "FocusPrev", [] {
        if (auto* mw = MainWindow::getInstance())
            mw->activatePreviousWindow();
    });
    addShortcut("Ctrl+]", "FocusNext", [] {
        if (auto* mw = MainWindow::getInstance())
            mw->activateNextWindow();
    });
    // Surface.ts:252-256——Ctrl+\ 克隆当前 Viewer（MDI：ViewCreate 新视图窗）。
    addShortcut("Ctrl+\\", "CloneView", [] {
        Application::Instance()->sendMsgToActiveView("ViewCreate");
    });
    // Surface.ts:257-258——Ctrl+| 关闭聚焦窗。
    addShortcut("Ctrl+|", "CloseView", [] {
        auto* mw = MainWindow::getInstance();
        if (mw && mw->mdiArea()) {
            if (auto* sub = mw->mdiArea()->activeSubWindow())
                sub->close();
        }
    });
}

}  // namespace Gui
