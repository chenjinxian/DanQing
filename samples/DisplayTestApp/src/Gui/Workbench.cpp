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

    // Toolbars
    if (m_tm) {
        ToolBarItem* tb = setupToolBars();
        m_tm->setup(tb);
        delete tb;
    }

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
MenuItem* StdWorkbench::setupMenuBar() const {
    auto menuBar = new MenuItem;

    // File  (Workbench.cpp:705-712)
    auto file = new MenuItem;
    file->setCommand("&File");
    *file << "Std_New" << "Std_Open" << "Std_RecentFiles" << "Separator"
          << "Std_CloseActiveWindow" << "Std_CloseAllWindows" << "Separator"
          << "Std_Save" << "Std_SaveAs" << "Std_SaveCopy" << "Std_SaveAll" << "Std_Revert"
          << "Separator"
          << "Std_Import" << "Std_Export" << "Std_MergeProjects" << "Std_ProjectInfo"
          << "Separator"
          << "Std_Print" << "Std_PrintPreview" << "Std_PrintPdf"
          << "Separator" << "Std_Quit";

    // Edit  (Workbench.cpp:715-723)
    auto edit = new MenuItem;
    edit->setCommand("&Edit");
    *edit << "Std_Undo" << "Std_Redo" << "Separator"
          << "Std_Cut" << "Std_Copy" << "Std_Paste" << "Std_DuplicateSelection"
          << "Separator"
          << "Std_Refresh" << "Std_BoxSelection" << "Std_BoxElementSelection"
          << "Std_SelectAll" << "Std_Delete" << "Std_SendToPythonConsole"
          << "Separator"
          << "Std_Placement" << "Std_TransformManip" << "Std_Alignment"
          << "Std_Edit" << "Std_Properties"
          << "Separator" << "Std_UserEditMode"
          << "Separator" << "Std_DlgPreferences";

    // Axonometric submenu  (Workbench.cpp:725-729)
    auto axoviews = new MenuItem;
    axoviews->setCommand("A&xonometric");
    *axoviews << "Std_ViewIsometric"
              << "Std_ViewDimetric"
              << "Std_ViewTrimetric";

    // Standard Views submenu  (Workbench.cpp:732-738)
    auto stdviews = new MenuItem;
    stdviews->setCommand("Standard &Views");
    *stdviews << "Std_ViewFitAll" << "Std_ViewFitSelection"
              << "Std_AlignToSelection" << axoviews
              << "Separator"
              << "Std_ViewHome" << "Std_ViewFront" << "Std_ViewTop"
              << "Std_ViewRight" << "Std_ViewRear" << "Std_ViewBottom" << "Std_ViewLeft"
              << "Separator"
              << "Std_ViewRotateLeft" << "Std_ViewRotateRight"
              << "Separator"
              << "Std_StoreWorkingView" << "Std_RecallWorkingView";

    // Zoom submenu  (Workbench.cpp:741-743)
    auto zoom = new MenuItem;
    zoom->setCommand("&Zoom");
    *zoom << "Std_ViewZoomIn" << "Std_ViewZoomOut"
          << "Separator" << "Std_ViewBoxZoom";

    // Visibility submenu  (Workbench.cpp:746-751)
    auto visu = new MenuItem;
    visu->setCommand("V&isibility");
    *visu << "Std_ToggleVisibility" << "Std_ShowSelection" << "Std_HideSelection"
          << "Std_SelectVisibleObjects"
          << "Separator"
          << "Std_ToggleObjects" << "Std_ShowObjects" << "Std_HideObjects"
          << "Separator" << "Std_ToggleSelectability";

    // View  (Workbench.cpp:753-781)
    auto view = new MenuItem;
    view->setCommand("&View");
    *view << "Std_ViewCreate" << "Std_OrthographicCamera" << "Std_PerspectiveCamera"
          << "Std_MainFullscreen"
          << "Separator" << stdviews << "Std_FreezeViews" << "Std_DrawStyle"
          << "Std_SelBoundingBox"
          << "Separator" << zoom << "Std_ViewDockUndockFullscreen"
          << "Std_ViewIvIssueCamPos"
          << "Std_AxisCross"
          << "Std_ToggleClipPlane"
          << "Std_TextureMapping"
          << "Separator" << visu << "Std_ToggleNavigation"
          << "Std_RandomColor"
          << "Std_ToggleTransparency"
          << "Separator"
          << "Std_Workbench"
          << "Std_ToolBarMenu"
          << "Std_DockViewMenu"
          << "Std_ToggleBottomPanels"
          << "Separator"
          << "Std_LinkSelectActions"
          << "Std_TreeViewActions"
          << "Std_ViewStatusBar";

    // Tools  (Workbench.cpp:784-809)
    auto tool = new MenuItem;
    tool->setCommand("&Tools");
    *tool << "Std_Measure"
          << "Std_MassProperties"
          << "Std_AnnotationLabel"
          << "Std_UnitsCalculator"
          << "Std_ClarifySelection"
          << "Separator"
          << "Std_ViewLoadImage"
          << "Std_ViewScreenShot"
          << "Std_TextDocument"
          << "Std_DemoMode"
          << "Separator"
          << "Std_SceneInspector"
          << "Std_DependencyGraph"
          << "Std_ExportDependencyGraph"
          << "Separator"
          << "Std_ProjectUtil"
          << "Std_DlgParameter"
          << "Std_DlgCustomize";

    // Macro  (Workbench.cpp:812-819)
    auto macro = new MenuItem;
    macro->setCommand("&Macro");
    *macro << "Std_DlgMacroRecord"
           << "Std_DlgMacroExecute"
           << "Std_RecentMacros"
           << "Separator"
           << "Std_DlgMacroExecuteDirect"
           << "Std_MacroAttachDebugger";

    // Windows  (Workbench.cpp:822-826)
    auto wnd = new MenuItem;
    wnd->setCommand("&Windows");
    *wnd << "Std_ActivateNextWindow" << "Std_ActivatePrevWindow" << "Separator"
         << "Std_TileWindows" << "Std_CascadeWindows" << "Separator"
         << "Std_WindowsMenu" << "Std_Windows";

    // Help  (Workbench.cpp:833-841)
    auto help = new MenuItem;
    help->setCommand("&Help");
    *help << "Std_WhatsThis"
          << "Separator"
          << "Std_FreeCADUserHub" << "Std_FreeCADForum" << "Std_ReportBug" << "Separator"
          << "Std_RestartInSafeMode" << "Separator"
          << "Std_DevHandbook" << "Std_PythonHelp" << "Separator"
          << "Std_FreeCADWebsite" << "Std_FreeCADDonation" << "Std_About";

    *menuBar << file << edit << view << tool << macro << wnd << help;

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
    // Ported from: FreeCAD Workbench.cpp:636 — Gui::Selection().getSelection()
    // DTA: no Gui::Selection singleton; selection is always empty.
    const bool sels_empty = true;

    if (strcmp(recipient, "View") == 0) {
        // FreeCAD Workbench.cpp:639 — createLinkMenu(item);
        // DTA: skipped (no document backend — Link Actions submenu never populated).
        // TODO: deferred — implement via itwinjs-core; see design 2026-07-16 §5.

        *item << "Separator";

        // Standard Views submenu (FreeCAD Workbench.cpp:642-648)
        auto StdViews = new MenuItem;
        StdViews->setCommand("Standard Views");

        *StdViews << "Std_ViewIsometric" << "Separator" << "Std_ViewHome" << "Std_ViewFront"
                  << "Std_ViewTop" << "Std_ViewRight"
                  << "Std_ViewRear" << "Std_ViewBottom" << "Std_ViewLeft"
                  << "Separator" << "Std_ViewRotateLeft" << "Std_ViewRotateRight";

        *item << "Std_ViewFitAll" << "Std_ViewFitSelection" << "Std_AlignToSelection"
              << "Std_DrawStyle" << StdViews << "Separator"
              << "Std_ViewDockUndockFullscreen";

        // Selection-dependent block (FreeCAD Workbench.cpp:654-659).
        // DTA: selection backend not yet wired; the block structure is preserved
        // verbatim but guarded by sels_empty (always true in DTA) so it never fires.
        // The referenced commands (Std_ToggleVisibility, Std_ToggleSelectability,
        // Std_TreeSelection, Std_RandomColor, Std_ToggleTransparency, Std_Delete,
        // Std_SendToPythonConsole) exist as stubs from region 1+2.
        if (!sels_empty) {
            *item << "Separator" << "Std_ToggleVisibility"
                  << "Std_ToggleSelectability" << "Std_TreeSelection"
                  << "Std_RandomColor" << "Std_ToggleTransparency" << "Separator" << "Std_Delete"
                  << "Std_SendToPythonConsole";
        }
    }
    else if (strcmp(recipient, "Tree") == 0) {
        // FreeCAD Workbench.cpp:661-683 — Tree recipient branch is entirely
        // selection-guarded; with sels_empty the branch produces nothing.
        // Structure preserved for when the selection backend lands.
    }

    // FreeCAD Workbench.cpp:685-690 — single-selection TransformManip/Placement
    // block. DTA: selection backend not wired; structure deferred.
    // TODO: deferred — implement via itwinjs-core; see design 2026-07-16 §5.
}

// Ported from: FreeCAD src/Gui/Workbench.cpp:846-906
ToolBarItem* StdWorkbench::setupToolBars() const {
    auto root = new ToolBarItem;

    // File  (Workbench.cpp:851-853)
    auto file = new ToolBarItem(root);
    file->setCommand("File");
    *file << "Std_New" << "Std_Open" << "Std_Save";

    // Edit  (Workbench.cpp:856-859)
    auto edit = new ToolBarItem(root);
    edit->setCommand("Edit");
    *edit << "Std_Undo" << "Std_Redo"
          << "Separator" << "Std_Refresh";

    // Clipboard [Hidden]  (Workbench.cpp:862-864)
    auto clipboard = new ToolBarItem(root, ToolBarItem::DefaultVisibility::Hidden);
    clipboard->setCommand("Clipboard");
    *clipboard << "Std_Cut" << "Std_Copy" << "Std_Paste";

    // Workbench switcher  (Workbench.cpp:867-869)
    auto wb = new ToolBarItem(root);
    wb->setCommand("Workbench");
    *wb << "Std_Workbench";

    // Macro [Hidden]  (Workbench.cpp:872-875)
    auto macro = new ToolBarItem(root, ToolBarItem::DefaultVisibility::Hidden);
    macro->setCommand("Macro");
    *macro << "Std_DlgMacroRecord" << "Std_DlgMacroExecute"
           << "Std_DlgMacroExecuteDirect";

    // View  (Workbench.cpp:878-882)
    auto view = new ToolBarItem(root);
    view->setCommand("View");
    *view << "Std_ViewFitAll" << "Std_ViewFitSelection"
          << "Std_ViewGroup" << "Std_AlignToSelection"
          << "Separator" << "Std_DrawStyle" << "Separator"
          << "Std_Measure" << "Std_MassProperties";

    // Individual Views [Hidden]  (Workbench.cpp:885-893)
    auto individualViews = new ToolBarItem(root, ToolBarItem::DefaultVisibility::Hidden);
    individualViews->setCommand("Individual Views");
    *individualViews << "Std_ViewIsometric"
                     << "Std_ViewFront"
                     << "Std_ViewTop"
                     << "Std_ViewRight"
                     << "Std_ViewRear"
                     << "Std_ViewBottom"
                     << "Std_ViewLeft";

    // Structure  (Workbench.cpp:896-898)
    auto structure = new ToolBarItem(root);
    structure->setCommand("Structure");
    *structure << "Std_Part" << "Std_Group" << "Std_LinkActions" << "Std_VarSet";

    // Help  (Workbench.cpp:901-903)
    auto help = new ToolBarItem(root);
    help->setCommand("Help");
    *help << "Std_WhatsThis";

    return root;
}

} // namespace Gui
