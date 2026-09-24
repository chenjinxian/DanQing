// Ported from: FreeCAD src/Gui/CommandWindow.cpp:43-226,455-524
// Window-domain commands (Tile/Cascade/Activate Next+Prev/Windows/WindowsMenu).
// Each command ctor metadata (sGroup/sMenuText/sToolTipText/sWhatsThis/sStatusTip/
// sPixmap/sAccel/eType) is copied VERBATIM from FreeCAD CommandWindow.cpp.
// activated() delegates to the new MainWindow slots (tile/cascade/activateNextWindow/
// activatePreviousWindow) wired in region 4 task 1.
//
// NOTE: StdCmdCloseActiveWindow and StdCmdCloseAllWindows are ALSO in FreeCAD's
// CommandWindow.cpp, but region 1+2 already ported them to CommandDoc.cpp
// (keeping the File-domain close logic together). They are intentionally NOT
// re-registered here — that would create duplicate command IDs.
#include "Command.h"
#include "Action.h"
#include "MainWindow.h"

#include <QCoreApplication>
#include <QObject>
#include <QString>

namespace Gui {

//===========================================================================
// Std_TileWindows — Ported from: FreeCAD src/Gui/CommandWindow.cpp:43-69
//===========================================================================
DEF_STD_CMD_A(StdCmdTileWindows)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:48-58
StdCmdTileWindows::StdCmdTileWindows()
    : Command("Std_TileWindows")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("&Tile");
    sToolTipText = QT_TR_NOOP("Tiles the windows");
    sWhatsThis = "Std_TileWindows";
    sStatusTip = sToolTipText;
    sPixmap = "Std_WindowTileVer";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:60-64
void StdCmdTileWindows::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    getMainWindow()->tile();
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:66-69
bool StdCmdTileWindows::isActive()
{
    // DTA: QMdiArea::subWindowList drives this; empty list => inactive.
    // FreeCAD queries getMainWindow()->windows(); DTA's MainWindow does not yet
    // expose windows() so we use mdiArea() directly (behaviourally equivalent).
    auto* mw = getMainWindow();
    return mw && mw->mdiArea() && !mw->mdiArea()->subWindowList().isEmpty();
}

//===========================================================================
// Std_CascadeWindows — Ported from: FreeCAD src/Gui/CommandWindow.cpp:71-97
//===========================================================================
DEF_STD_CMD_A(StdCmdCascadeWindows)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:77-86
StdCmdCascadeWindows::StdCmdCascadeWindows()
    : Command("Std_CascadeWindows")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("&Cascade");
    sToolTipText = QT_TR_NOOP("Tiles pragmatic");
    sWhatsThis = "Std_CascadeWindows";
    sStatusTip = sToolTipText;
    sPixmap = "Std_WindowCascade";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:88-92
void StdCmdCascadeWindows::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    getMainWindow()->cascade();
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:94-97
bool StdCmdCascadeWindows::isActive()
{
    auto* mw = getMainWindow();
    return mw && mw->mdiArea() && !mw->mdiArea()->subWindowList().isEmpty();
}

//===========================================================================
// Std_ActivateNextWindow — Ported from: FreeCAD src/Gui/CommandWindow.cpp:159-186
//===========================================================================
DEF_STD_CMD_A(StdCmdActivateNextWindow)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:164-175
StdCmdActivateNextWindow::StdCmdActivateNextWindow()
    : Command("Std_ActivateNextWindow")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("&Next");
    sToolTipText = QT_TR_NOOP("Activates the next window");
    sWhatsThis = "Std_ActivateNextWindow";
    sStatusTip = sToolTipText;
    sPixmap = "Std_WindowNext";
    sAccel = keySequenceToAccel(QKeySequence::NextChild);
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:177-181
void StdCmdActivateNextWindow::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    getMainWindow()->activateNextWindow();
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:183-186
bool StdCmdActivateNextWindow::isActive()
{
    auto* mw = getMainWindow();
    return mw && mw->mdiArea() && !mw->mdiArea()->subWindowList().isEmpty();
}

//===========================================================================
// Std_ActivatePrevWindow — Ported from: FreeCAD src/Gui/CommandWindow.cpp:188-221
//===========================================================================
DEF_STD_CMD_A(StdCmdActivatePrevWindow)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:193-210
StdCmdActivatePrevWindow::StdCmdActivatePrevWindow()
    : Command("Std_ActivatePrevWindow")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("&Previous");
    sToolTipText = QT_TR_NOOP("Switches to the previously active window");
    sWhatsThis = "Std_ActivatePrevWindow";
    sStatusTip = sToolTipText;
    sPixmap = "Std_WindowPrev";
    // Depending on the OS 'QKeySequence::PreviousChild' gives
    // Ctrl+Shift+Backtab instead of Ctrl+Shift+Tab which leads
    // to a strange behaviour when using it.
    // A workaround is to create a shortcut as Shift + QKeySequence::NextChild
    static std::string previousChild = std::string("Shift+")
        + keySequenceToAccel(QKeySequence::NextChild);
    sAccel = previousChild.c_str();
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:212-216
void StdCmdActivatePrevWindow::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    getMainWindow()->activatePreviousWindow();
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:218-221
bool StdCmdActivatePrevWindow::isActive()
{
    auto* mw = getMainWindow();
    return mw && mw->mdiArea() && !mw->mdiArea()->subWindowList().isEmpty();
}

//===========================================================================
// Std_Windows — Ported from: FreeCAD src/Gui/CommandWindow.cpp:223-246
//===========================================================================
DEF_STD_CMD(StdCmdWindows)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:228-239
StdCmdWindows::StdCmdWindows()
    : Command("Std_Windows")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("Choose Open &Window");

    sToolTipText = QT_TR_NOOP("Displays the open windows");
    sWhatsThis = "Std_Windows";
    sStatusTip = sToolTipText;
    sPixmap = "Std_Windows";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:241-246
void StdCmdWindows::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    // FreeCAD opens Gui::Dialog::DlgActivateWindowImp. DTA has no such dialog;
    // leave as a no-op until the dialog is ported.
    // TODO: port DlgActivateWindowImp when region 4 brings up the Windows dialog.
}

//===========================================================================
// Std_WindowsMenu — Ported from: FreeCAD src/Gui/CommandWindow.cpp:451-497
//===========================================================================
DEF_STD_CMD_AC(StdCmdWindowsMenu)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:457-466
StdCmdWindowsMenu::StdCmdWindowsMenu()
    : Command("Std_WindowsMenu")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("Activate Window");  // Replaced with the name of the window
    sToolTipText = QT_TR_NOOP("Activates this window");
    sWhatsThis = "Std_WindowsMenu";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:468-472
void StdCmdWindowsMenu::activated(int iMsg)
{
    // already handled by the main window
    Q_UNUSED(iMsg);
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:474-477
bool StdCmdWindowsMenu::isActive()
{
    return true;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:479-497
Action* StdCmdWindowsMenu::createAction()
{
    // Allow one to show 10 menu items in the 'Window' menu and one separator.
    // If we have more windows then the user can use the 'Windows...' item.
    // DTA deviation: FreeCAD creates the 10 placeholders + separator HERE; region 1+2
    // moved that loop into the WindowAction ctor so a bare `new WindowAction(cmd)` is
    // usable in unit tests without the command system (see Action.cpp:589). Region 4
    // task 1 keeps that structure and adds the triggered -> onWindowTriggered wiring
    // inside the same ctor loop. This function therefore just constructs the action
    // and applies command metadata; the placeholder set + separator come from the ctor.
    WindowAction* pcAction;
    pcAction = new WindowAction(this, getMainWindow());
    applyCommandData(this->className(), pcAction);
    return pcAction;
}

// =====================================================================
// Registration — Ported from: FreeCAD src/Gui/CommandWindow.cpp:504-526
// (CreateWindowStdCommands). StdCmdCloseActiveWindow and StdCmdCloseAllWindows
// are NOT registered here — region 1+2 already registers them from CommandDoc.cpp
// alongside the File-domain close handling (single source of truth for close IDs).
// =====================================================================
void createWindowCommands(CommandManager& mgr)
{
    mgr.addCommand(new StdCmdTileWindows);
    mgr.addCommand(new StdCmdCascadeWindows);
    mgr.addCommand(new StdCmdActivateNextWindow);
    mgr.addCommand(new StdCmdActivatePrevWindow);
    mgr.addCommand(new StdCmdWindows);
    mgr.addCommand(new StdCmdWindowsMenu);
}

} // namespace Gui
