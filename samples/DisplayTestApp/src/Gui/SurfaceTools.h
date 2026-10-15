// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Surface 窗口 keyin 工具族（MDI 承载）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/Surface.ts
//              (CreateWindowTool :451-487 + WindowIdTool :489-506 +
//              FocusWindowTool :508-513 + MaximizeWindowTool :515-521 +
//              RestoreWindowTool :523-528 + CloseWindowTool :530-535 +
//              ResizeWindowTool :536-559 + DockWindowTool :560-598)
//              + Window.ts (Dock :110-120 + Window ctor 默认几何 :358-385 +
//              WindowHeader.dock/undock/applyDock :185-245 +
//              ensureInSurface :247-281 + resizeContent :318-335 +
//              WindowProps/NamedWindowProps :339-346/:430-432)
//
// EQUIVALENCE 登记（§11.10——Electron/browser 浮动窗 → MDI 子窗逐面对照）：
//   E-Y1 窗口模型：参考 Window = Surface DOM 浮动窗（Window.ts:348），DanQing
//     = QMdiSubWindow（用户裁决保留 MDI 壳）。逐工具对照：
//       createNamedWindow → 新建**无视口** MDI 子窗（objectName=id——
//         NamedWindow._windowId 承载；内容=空 QWidget 占位，无 header 件）；
//       focusedWindow → mdiArea.activeSubWindow()（参考 _windows[0]=焦点序首，
//         :403-405——focus() 提前语义 :331-332 ≡ QMdiArea 激活序；不改全局
//         ActivationOrder[避免牵连 M-O(1) 轮换既有面]）；
//       findWindowById → 视口子窗 = viewportId 串（Viewer.ts:609）｜命名窗 =
//         内容 widget 的 objectName；
//       focus → setActiveSubWindow（参考 surface.focus→updateFocus 换序——
//         QMdiArea 激活等价）；
//       close → sub->close()（WA_DeleteOnClose；参考 close 的 isCloseable 门
//         :407-410 恒 true 坍塌）；
//       dock(Dock) → mdiArea viewport 内几何半边/四分/全幅（applyDock
//         :216-245 公式逐行）+ dock 态与存档几何存子窗动态属性
//         （"dtaDockState"/"dtaDockSavedGeom"——WindowHeader._dockState 承载）。
//         **Dock.Full = 几何充满（0,0,sw,sh）而非 Qt showMaximized**——参考
//         dock 与 OS maximize 本就不同概念（双击标题=dock/undock 往复，
//         Window.ts:163-169）；
//       undock → 恢复存档几何 + 清态 + focus（:200-213）；
//       resizeContent → w/h 各 +4 边框 kludge + min 50 钳（:319-321 与
//         ResizeState minSize=50，:61）+ 清 dock 态（:323）+ 按内容 delta
//         改 container（:324-332）；**NaN 侧跳过**（参考 NaN→"NaNpx" 被浏览器
//         忽略的同效语义——E-Y4 的另一腿）；
//     发散 = DOM 浮动层↔MDI 子窗的平台形态、header 件（标题条/关闭钮/角柄）
//     缺席（MDI 子窗自带等价 chrome）、pin/drag 交互未移植（参考 Window.ts
//     DragState/pin——DTA 拖拽面，登记 TODO）。验证法 = WindowToolsTest 全族锁。
//   E-Y2 CreateWindowTool.maxArgs=undefined（Surface.ts:455）→ maxArgs()=-1
//     （DanQing ToolRegistry 的不限哨兵——ToolAdmin.cpp:2107 的 `maxArgs >= 0`
//     门；-1 恒过=undefined 语义）。
//   E-Y3 parseInt → strtol 前导整数语义（ParseArgs.cpp:70-76 先例——前导空白
//     跳过/前导数字段/无数字=NaN）。
//   E-Y4 ResizeWindowTool parseAndRun 的 `!isNaN(w) || !isNaN(h)` **OR 怪癖**
//     （:551-554——单侧重法也 run，NaN 侧经 resizeContent 落 no-op）原样保留。
//   E-Y5 DockWindowTool 字母环的 `default: return true` **早退怪癖**
//     （:578-590——未知字母立即返 true，已累积位不落窗）原样保留；空串 →
//     dock=0 不 run（:592-593）。
//   E-Y6 firstViewer（:216-221——焦点序窗表中首个 Viewer）→ 激活序追踪器
//     （subWindowActivated 懒装、QPointer 防悬垂）+ 未追踪 viewer 创建序补位；
//     发散 = 本层首次调用前的激活史不可见（补位兜底）。验证法 =
//     IModelFileToolsTest.FirstViewerFollowsActivationOrder。
//   E-Y7 ReopenIModelTool 的 `viewer.openFile(viewer.viewport.iModel.key)`
//     （:660-663）→ dump key（源 bim 绝对路径——rpcProps.key 回放）经
//     dta::modelIdForIModelKey basename 映射 → dta::dumpPackageForModel →
//     openDumpIModel **同一视图重开**（参考 resetIModel 同 viewer 语义）；
//     发散 = DanQing 只认五 dump 键域；未映射 key → no-op + 状态栏 Err
//     （参考 openIModel catch → alert("Error - could not open file.")，
//     Viewer.ts:579-582——showStatus Err 为等价消息面）。验证法 =
//     IModelFileToolsTest 映射锁 + 真窗 E2E（主会话收口跑）。
//   E-Y8 blank connection（key 空）上 reopen → no-op + Err 消息（参考
//     openFile("")→openIModel 失败 alert 同面——弹文件选择器（selectIModel）
//     不在本件范围，登记）。
#pragma once

#include <dqApp/ToolAdmin.h>

#include <QMdiSubWindow>

#include <optional>
#include <string>

class QMdiArea;

namespace Gui {

class View3DInventor;

// ---------------------------------------------------------------------------
// Dock — Ported from: Window.ts:110-120（位掩码；Full=0 为合法 dock 态——
// 属性存在性承载 docked 判定，非零值判定）。
// ---------------------------------------------------------------------------
enum class Dock : int {
    Full = 0,                          // :111
    Top = 1 << 0,                      // :112
    Left = 1 << 1,                     // :113
    Right = 1 << 2,                    // :114
    Bottom = 1 << 3,                   // :115
    TopLeft = Top | Left,              // :117
    TopRight = Top | Right,            // :118
    BottomLeft = Bottom | Left,        // :119
    BottomRight = Bottom | Right,      // :120
};

// ---------------------------------------------------------------------------
// WindowProps / NamedWindowProps — Ported from: Window.ts:339-346/:430-432。
// scrollbars（:345）不承载（MDI 子窗无对应——E-Y1 登记）。
// ---------------------------------------------------------------------------
struct WindowProps {
    std::optional<std::string> title;
    std::optional<int> top;
    std::optional<int> left;
    std::optional<int> width;
    std::optional<int> height;
};

struct NamedWindowProps : WindowProps {
    std::string id;  // :431（必填——Caller 保证）
};

// ---------------------------------------------------------------------------
// Surface — DTA Surface 窗管面的 MDI 承载（Surface.ts 静态实例面 :216-420 的
// 工具消费子集 + Window/WindowHeader 的 dock/undock/resizeContent 段）。
// 默认作用于 MainWindow::getInstance()->mdiArea()；测试经 setMdiAreaForTest
// 注入裸 QMdiArea（无 MainWindow 依赖）。
// ---------------------------------------------------------------------------
class Surface {
public:
    // --- 作用域缝（测试） ---
    static void setMdiAreaForTest(QMdiArea* area);
    // 清测试缝 + 激活序追踪器（测试收尾）。
    static void resetForTest();

    // --- 窗表查询（Surface.ts:399-405） ---
    static QMdiSubWindow* findWindowById(QString const& id);
    static QMdiSubWindow* focusedWindow();
    // firstViewer（:216-221）——E-Y6。
    static Gui::View3DInventor* firstViewer();

    // --- 窗操作（Surface.ts:293-297/:322-338/:407-415 + Window.ts） ---
    static QMdiSubWindow* createNamedWindow(NamedWindowProps const& props);
    static void focus(QMdiSubWindow* window);
    static void close(QMdiSubWindow* window);
    static void closeAllViewers();

    // --- dock 族（WindowHeader.dock/undock/applyDock/isDocked/ensureInSurface/
    //     resizeContent——Window.ts:185-335） ---
    static void dock(QMdiSubWindow* window, Dock dockState);
    static void undock(QMdiSubWindow* window);
    static bool isDocked(QMdiSubWindow* window);
    static void applyDock(QMdiSubWindow* window);
    static void ensureInSurface(QMdiSubWindow* window);
    // w/h 允许 NaN（参考 parseInt 直传语义——NaN 侧 no-op，E-Y1/E-Y4）。
    static void resizeContent(QMdiSubWindow* window, double w, double h);

private:
    static QMdiArea* mdiArea();
};

// ---------------------------------------------------------------------------
// CreateWindowTool — Ported from: Surface.ts:451-487。
// ---------------------------------------------------------------------------
class CreateWindowTool final : public dqApp::InteractiveTool {
public:
    const char* getToolId() const override { return "CreateWindow"; }  // :452
    int minArgs() const override { return 1; }                         // :454
    int maxArgs() const override { return -1; }  // :455 undefined → -1（E-Y2）
    std::string englishKeyin() const override { return "win create"; }

    // Ported from: run (:457-460)。
    bool run(NamedWindowProps const& props);
    // Ported from: parseAndRun (:462-486)。
    bool parseAndRun(std::vector<std::string> const& args) override;
};

// ---------------------------------------------------------------------------
// WindowIdTool — Ported from: Surface.ts:489-506（abstract 基类）。
// ---------------------------------------------------------------------------
class WindowIdTool : public dqApp::InteractiveTool {
public:
    int minArgs() const override { return 0; }  // :490
    int maxArgs() const override { return 1; }  // :491

    // Ported from: execute (:493——abstract)。
    virtual void execute(QMdiSubWindow* window) = 0;

    // Ported from: run (:495-501——id 缺席 = focusedWindow）。
    bool run(std::optional<std::string> const& windowId);
    bool run() override { return run(std::nullopt); }  // 框架无参入口
    // Ported from: parseAndRun (:503-505——args[0] 直传）。
    bool parseAndRun(std::vector<std::string> const& args) override;
};

class FocusWindowTool final : public WindowIdTool {
public:
    const char* getToolId() const override { return "FocusWindow"; }  // :509
    std::string englishKeyin() const override { return "win focus"; }
    void execute(QMdiSubWindow* window) override;  // :510-512 window.focus()
};

class MaximizeWindowTool final : public WindowIdTool {
public:
    const char* getToolId() const override { return "MaximizeWindow"; }  // :516
    std::string englishKeyin() const override { return "win max"; }
    // :518-520——`if (!window.isDocked) window.dock(Dock.Full)`。
    void execute(QMdiSubWindow* window) override;
};

class RestoreWindowTool final : public WindowIdTool {
public:
    const char* getToolId() const override { return "RestoreWindow"; }  // :524
    std::string englishKeyin() const override { return "win restore"; }
    void execute(QMdiSubWindow* window) override;  // :526-527 window.undock()
};

class CloseWindowTool final : public WindowIdTool {
public:
    const char* getToolId() const override { return "CloseWindow"; }  // :531
    std::string englishKeyin() const override { return "win close"; }
    void execute(QMdiSubWindow* window) override;  // :533-534 Surface.close(window)
};

// ---------------------------------------------------------------------------
// ResizeWindowTool — Ported from: Surface.ts:536-559。
// ---------------------------------------------------------------------------
class ResizeWindowTool final : public dqApp::InteractiveTool {
public:
    const char* getToolId() const override { return "ResizeWindow"; }  // :537
    int minArgs() const override { return 2; }                         // :538
    int maxArgs() const override { return 3; }                         // :539
    std::string englishKeyin() const override { return "win resize"; }

    // Ported from: run (:541-547)。w/h 允许 NaN（parseInt 直传——E-Y4）。
    bool run(double width, double height, std::optional<std::string> const& id);
    bool run() override { return true; }  // 框架无参入口（参考无此形——恒 true 兜底）
    // Ported from: parseAndRun (:549-558——**OR 怪癖原样**，E-Y4）。
    bool parseAndRun(std::vector<std::string> const& args) override;
};

// ---------------------------------------------------------------------------
// DockWindowTool — Ported from: Surface.ts:560-598。
// ---------------------------------------------------------------------------
class DockWindowTool final : public dqApp::InteractiveTool {
public:
    const char* getToolId() const override { return "DockWindow"; }  // :561
    int minArgs() const override { return 1; }                       // :562
    int maxArgs() const override { return 2; }                       // :563
    std::string englishKeyin() const override { return "win dock"; }

    // Ported from: run (:565-571)。
    bool run(Dock dockState, std::optional<std::string> const& windowId);
    bool run() override { return true; }  // 框架无参入口（同上兜底）
    // Ported from: parseAndRun (:573-597——l/r/t/b 位累积 + 未知字母早退
    // true 怪癖 + dock=0 不 run，E-Y5）。
    bool parseAndRun(std::vector<std::string> const& args) override;
};

}  // namespace Gui
