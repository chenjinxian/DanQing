// Ported from: FreeCAD src/Gui/Workbench.cpp:453-480, 699-906
#include "Workbench.h"
#include "Command.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QApplication>

#include <cstring>

namespace Gui {

// Tracks the most recently activated workbench. Set by Workbench::activate(),
// read by Workbench::activeWorkbench(). DTA has a single static workbench
// (StdWorkbench in main.cpp) whose lifetime equals the app lifetime, so the
// raw pointer is safe — no dangling.
Workbench* Workbench::s_activeWorkbench = nullptr;

// Ported from: DTA adaptation (no FreeCAD equivalent — FreeCAD's
// WorkbenchManager owns workbench lifetime; DTA tracks a raw pointer).
// Clears s_activeWorkbench on destruction so unit tests that create/destroy
// workbenches per-test don't leave a dangling pointer.
Workbench::~Workbench()
{
    if (s_activeWorkbench == this) {
        s_activeWorkbench = nullptr;
    }
}

// Ported from: FreeCAD src/Gui/Workbench.cpp:453-480
// Simplified: drops WorkbenchManipulator, DockWindowManager, setupCustomToolbars, setupCustomShortcuts
// (single static workbench, no dock windows, no user params — region 3 for dock support)
bool Workbench::activate() {
    s_activeWorkbench = this;

    // M-L(2)：工具栏构建块已删——StdWorkbench::setupToolBars 是死树（main.cpp
    // 传 nullptr 永不构建；工具栏区由 DtaToolBarSet 按 DTA 功能分类重建）。

    // Menus
    if (m_mm && m_mw) {
        MenuItem* mb = setupMenuBar();
        QMenuBar* bar = m_mw->menuBar();
        bar->clear();
        for (MenuItem* top : mb->getItems()) {
            QMenu* m = bar->addMenu(
                QApplication::translate("Workbench", top->command().c_str()));
            m_mm->setup(top, m);
        }
        delete mb;
    }

    return true;
}

// Ported from: FreeCAD src/Gui/Workbench.cpp:360-364 (Workbench::setupContextMenu base no-op)
void Workbench::setupContextMenu(const char* /*recipient*/, MenuItem* /*item*/) const
{
    // Base no-op — overrides in subclasses (e.g. StdWorkbench).
}

// Ported from: FreeCAD src/Gui/WorkbenchManager.cpp active() accessor
// DTA adaptation: no WorkbenchManager singleton — s_activeWorkbench is set by
// activate() and held for the app lifetime.
const Workbench* Workbench::activeWorkbench()
{
    return s_activeWorkbench;
}

// =====================================================================
// StdWorkbench — Verbatim command-name trees from FreeCAD
// =====================================================================

// Ported from: FreeCAD src/Gui/Workbench.cpp:699-844
// M-L(2) 菜单树裁剪（分析报告 §3.1/§3.4）：只保留真功能命令所在的菜单项——
//   File  = New（blank connection）/ Import（glTF）/ Quit；
//   View  = 克隆/相机/全屏 + Standard Views 子菜单（FitAll + 8 向）+ Panels +
//           Status Bar；
//   Tools = About Qt（原 Help 域唯一真功能项，Help 菜单整体删除后归此）；
//   Windows = 窗口命令域（唯一全活域，不动）。
// Edit/Help/Macro 菜单整体删除（全存根/FreeCAD 生态链接）；Axonometric/Zoom/
// Visibility 子菜单随其命令删除（8 向标准视图平铺在 Standard Views 子菜单）。
MenuItem* StdWorkbench::setupMenuBar() const {
    auto menuBar = new MenuItem;

    // File  (Workbench.cpp:705-712)
    auto file = new MenuItem;
    file->setCommand("&File");
    *file << "Std_New" << "Separator"
          << "Std_Import" << "Separator" << "Std_Quit";

    // Standard Views submenu  (Workbench.cpp:732-738)
    auto stdviews = new MenuItem;
    stdviews->setCommand("Standard &Views");
    *stdviews << "Std_ViewFitAll"
              << "Separator"
              << "Std_ViewIsometric" << "Std_ViewFront" << "Std_ViewTop"
              << "Std_ViewRight" << "Std_ViewRear" << "Std_ViewBottom" << "Std_ViewLeft";

    // View  (Workbench.cpp:753-781)
    auto view = new MenuItem;
    view->setCommand("&View");
    *view << "Std_ViewCreate" << "Std_OrthographicCamera" << "Std_PerspectiveCamera"
          << "Std_MainFullscreen"
          << "Separator" << stdviews
          << "Separator"
          << "Std_DockViewMenu"
          << "Std_ViewStatusBar";

    // Tools 菜单（Std_AboutQt）已删（2026-10-07 删除侧：DTA 无 About 对话框——
    // Tools 菜单仅为它存在，整菜单随删）。

    // Windows  (Workbench.cpp:822-826)
    auto wnd = new MenuItem;
    wnd->setCommand("&Windows");
    *wnd << "Std_ActivateNextWindow" << "Std_ActivatePrevWindow" << "Separator"
         << "Std_TileWindows" << "Std_CascadeWindows" << "Separator"
         << "Std_WindowsMenu";

    *menuBar << file << view << wnd;

    return menuBar;
}

// Ported from: FreeCAD src/Gui/Workbench.cpp:634-660 (StdWorkbench::setupContextMenu)
// DTA adaptation:
//   * createLinkMenu (FreeCAD :639, :369-408) is skipped — it requires
//     App::GetApplication().getActiveDocument(); DTA has no document backend.
//     When the Link Actions submenu lands, restore the call site.
//     TODO: deferred — implement via itwinjs-core; see design 2026-07-16 §5.
//   * The selection-dependent block (FreeCAD :654-659) requires
//     Gui::Selection().getSelection(); DTA has no selection backend, so the
//     block is ported verbatim but guarded to never fire until selection lands.
//     TODO: deferred — implement via itwinjs-core; see design 2026-07-16 §5.
void StdWorkbench::setupContextMenu(const char* recipient, MenuItem* item) const
{
    // M-L(2)：FreeCAD 的选择集守卫块（Workbench.cpp:654-659，DTA 下恒不触发）
    // 与 Tree 分支（:661-683 全程为空）已随其引用的死命令一并删除。
    if (strcmp(recipient, "View") == 0) {
        *item << "Separator";

        // Standard Views submenu (FreeCAD Workbench.cpp:642-648)
        auto StdViews = new MenuItem;
        StdViews->setCommand("Standard Views");

        *StdViews << "Std_ViewIsometric" << "Std_ViewFront"
                  << "Std_ViewTop" << "Std_ViewRight"
                  << "Std_ViewRear" << "Std_ViewBottom" << "Std_ViewLeft";

        *item << "Std_ViewFitAll" << StdViews;
    }
}

} // namespace Gui
