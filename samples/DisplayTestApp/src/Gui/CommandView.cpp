// Ported from: FreeCAD src/Gui/CommandView.cpp + CommandWindow.cpp + CommandStd.cpp
//              View domain commands. M-L(2) 清理（分析报告 §3.1/§3.5）："可点但
//              无效果"的存根命令族全删——FitSelection/AlignToSelection/ViewGroup/
//              Dimetric/Trimetric/Home/RotateLeft·Right/StoreWorkingView·Recall/
//              ZoomIn·Out·BoxZoom/DrawStyle 组/AxisCross/ToggleClipPlane/
//              TextureMapping/可见性 10 项/ToggleNavigation/ToolBarMenu/
//              ToggleBottomPanels/ViewVR/ClarifySelection/ScreenShot/LoadImage/
//              SelBoundingBox/TreeViewActions 组/Dock·Undock·Fullscreen 子命令族。
//              保留真功能（标准视图/相机/克隆/全屏/面板/状态栏——报告 §3.2 "不动"
//              清单）：ViewCreate、正交/透视相机、FitAll、Iso/Front/Top/Right/
//              Rear/Bottom/Left、MainFullscreen、DockViewMenu、ViewStatusBar。
#include "CommandView.h"
#include "Application.h"  // getGuiApplication() → sendMsgToActiveView / sendHasMsgToActiveView
#include "MainWindow.h"

#include <QStatusBar>

namespace Gui {

// =====================================================================
// Std_ViewCreate
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:2355-2367
StdCmdViewCreate::StdCmdViewCreate()
    : Command("Std_ViewCreate")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("New 3D View");
    sToolTipText = QT_TR_NOOP("Opens a new 3D view window for the active document");
    sWhatsThis = "Std_ViewCreate";
    sStatusTip = sToolTipText;
    sPixmap = "window-new";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:2369-2374
void StdCmdViewCreate::activated(int /*iMsg*/)
{
    getGuiApplication()->sendMsgToActiveView("ViewCreate");
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:2376-2379
bool StdCmdViewCreate::isActive()
{
    return getGuiApplication()->sendHasMsgToActiveView("ViewCreate");
}

// =====================================================================
// Std_OrthographicCamera (checkable, mutually exclusive with Perspective)
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:140-153
StdOrthographicCamera::StdOrthographicCamera()
    : Command("Std_OrthographicCamera")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Orthographic View");
    sToolTipText = QT_TR_NOOP("Switches to orthographic view mode");
    sWhatsThis = "Std_OrthographicCamera";
    sStatusTip = sToolTipText;
    sPixmap = "view-isometric";
    sAccel = "V, O";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:155-160
void StdOrthographicCamera::activated(int /*iMsg*/)
{
    getGuiApplication()->sendMsgToActiveView("OrthographicCamera");
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:162-178
bool StdOrthographicCamera::isActive()
{
    return getGuiApplication()->sendHasMsgToActiveView("OrthographicCamera");
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:180-185
Action* StdOrthographicCamera::createAction()
{
    Action* pcAction = Command::createAction();
    pcAction->setCheckable(true);
    return pcAction;
}

// =====================================================================
// Std_PerspectiveCamera (checkable, mutually exclusive with Orthographic)
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:187-199
StdPerspectiveCamera::StdPerspectiveCamera()
    : Command("Std_PerspectiveCamera")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Perspective View");
    sToolTipText = QT_TR_NOOP("Switches to perspective view mode");
    sWhatsThis = "Std_PerspectiveCamera";
    sStatusTip = sToolTipText;
    sPixmap = "view-perspective";
    sAccel = "V, P";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:202-207
void StdPerspectiveCamera::activated(int /*iMsg*/)
{
    getGuiApplication()->sendMsgToActiveView("PerspectiveCamera");
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:209-225
bool StdPerspectiveCamera::isActive()
{
    return getGuiApplication()->sendHasMsgToActiveView("PerspectiveCamera");
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:227-232
Action* StdPerspectiveCamera::createAction()
{
    Action* pcAction = Command::createAction();
    pcAction->setCheckable(true);
    return pcAction;
}

// =====================================================================
// Std_MainFullscreen
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1902-1915
StdMainFullscreen::StdMainFullscreen()
    : Command("Std_MainFullscreen")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Fullscreen");
    sToolTipText = QT_TR_NOOP("Displays the main window in fullscreen mode");
    sWhatsThis = "Std_MainFullscreen";
    sStatusTip = sToolTipText;
    sPixmap = "view-fullscreen";
    sAccel = "Alt+F11";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1917-1932
void StdMainFullscreen::activated(int /*iMsg*/)
{
    if (auto* mw = getMainWindow()) {
        if (mw->isFullScreen())
            mw->showNormal();
        else
            mw->showFullScreen();
    }
}

// =====================================================================
// Std_ViewFitAll
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1742-1755
StdCmdViewFitAll::StdCmdViewFitAll()
    : Command("Std_ViewFitAll")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Fit All");
    sToolTipText = QT_TR_NOOP("Fits all content into the 3D view");
    sWhatsThis = "Std_ViewFitAll";
    sStatusTip = sToolTipText;
    sPixmap = "zoom-all";
    sAccel = "V, F";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1757-1761
void StdCmdViewFitAll::activated(int /*iMsg*/)
{
    getGuiApplication()->sendMsgToActiveView("ViewFit");
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1763-1766
bool StdCmdViewFitAll::isActive()
{
    return getGuiApplication()->sendHasMsgToActiveView("ViewFit");
}

// =====================================================================
// Standard view commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1607-1620
StdCmdViewIsometric::StdCmdViewIsometric()
    : Command("Std_ViewIsometric")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Isometric");
    sToolTipText = QT_TR_NOOP("Sets the camera to the isometric view");
    sWhatsThis = "Std_ViewIsometric";
    sStatusTip = sToolTipText;
    sPixmap = "view-axonometric";
    sAccel = "0";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1622-1626
void StdCmdViewIsometric::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewIsometric"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1628-1630
bool StdCmdViewIsometric::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewIsometric"); }

// Ported from: FreeCAD src/Gui/CommandView.cpp:1461-1474
StdCmdViewFront::StdCmdViewFront()
    : Command("Std_ViewFront")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Front");
    sToolTipText = QT_TR_NOOP("Sets the camera to the front view");
    sWhatsThis = "Std_ViewFront";
    sStatusTip = sToolTipText;
    sPixmap = "view-front";
    sAccel = "1";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1476-1480
void StdCmdViewFront::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewFront"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1482-1484
bool StdCmdViewFront::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewFront"); }

// Ported from: FreeCAD src/Gui/CommandView.cpp:1577-1590
StdCmdViewTop::StdCmdViewTop()
    : Command("Std_ViewTop")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Top");
    sToolTipText = QT_TR_NOOP("Sets the camera to the top view");
    sWhatsThis = "Std_ViewTop";
    sStatusTip = sToolTipText;
    sPixmap = "view-top";
    sAccel = "2";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1592-1596
void StdCmdViewTop::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewTop"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1598-1600
bool StdCmdViewTop::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewTop"); }

// Ported from: FreeCAD src/Gui/CommandView.cpp:1548-1561
StdCmdViewRight::StdCmdViewRight()
    : Command("Std_ViewRight")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Right");
    sToolTipText = QT_TR_NOOP("Sets the camera to the right view");
    sWhatsThis = "Std_ViewRight";
    sStatusTip = sToolTipText;
    sPixmap = "view-right";
    sAccel = "3";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1563-1567
void StdCmdViewRight::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewRight"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1569-1571
bool StdCmdViewRight::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewRight"); }

// Ported from: FreeCAD src/Gui/CommandView.cpp:1519-1532
StdCmdViewRear::StdCmdViewRear()
    : Command("Std_ViewRear")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Rear");
    sToolTipText = QT_TR_NOOP("Sets the camera to the rear view");
    sWhatsThis = "Std_ViewRear";
    sStatusTip = sToolTipText;
    sPixmap = "view-rear";
    sAccel = "4";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1534-1538 (activated → sendMsgToActiveView "ViewRear")
void StdCmdViewRear::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewRear"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1540-1542 (isActive → sendHasMsgToActiveView "ViewRear")
bool StdCmdViewRear::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewRear"); }

// Ported from: FreeCAD src/Gui/CommandView.cpp:1432-1445
StdCmdViewBottom::StdCmdViewBottom()
    : Command("Std_ViewBottom")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Bottom");
    sToolTipText = QT_TR_NOOP("Sets the camera to the bottom view");
    sWhatsThis = "Std_ViewBottom";
    sStatusTip = sToolTipText;
    sPixmap = "view-bottom";
    sAccel = "5";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1447-1451 (activated → sendMsgToActiveView "ViewBottom")
void StdCmdViewBottom::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewBottom"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1453-1455 (isActive → sendHasMsgToActiveView "ViewBottom")
bool StdCmdViewBottom::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewBottom"); }

// Ported from: FreeCAD src/Gui/CommandView.cpp:1490-1503
StdCmdViewLeft::StdCmdViewLeft()
    : Command("Std_ViewLeft")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Left");
    sToolTipText = QT_TR_NOOP("Sets the camera to the left view");
    sWhatsThis = "Std_ViewLeft";
    sStatusTip = sToolTipText;
    sPixmap = "view-left";
    sAccel = "6";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1505-1509 (activated → sendMsgToActiveView "ViewLeft")
void StdCmdViewLeft::activated(int) { getGuiApplication()->sendMsgToActiveView("ViewLeft"); }
// Ported from: FreeCAD src/Gui/CommandView.cpp:1511-1513 (isActive → sendHasMsgToActiveView "ViewLeft")
bool StdCmdViewLeft::isActive() { return getGuiApplication()->sendHasMsgToActiveView("ViewLeft"); }

// =====================================================================
// Dock, Status bar
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandWindow.cpp (DockViewMenu section)
StdCmdDockViewMenu::StdCmdDockViewMenu()
    : Command("Std_DockViewMenu")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("&Panels");
    sToolTipText = QT_TR_NOOP("Toggles dock panels");
    sWhatsThis = "Std_DockViewMenu";
    sStatusTip = sToolTipText;
    eType = 0;
    bCanLog = false;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp — DockViewMenu section
Action* StdCmdDockViewMenu::createAction()
{
    auto pcAction = new ActionGroup(this, getMainWindow());
    pcAction->setDropDownMenu(true);
    applyCommandData(this->className(), pcAction);
    // Ported from: FreeCAD src/Gui/MainWindow.cpp:1720-1723 — populate on aboutToShow
    QObject::connect(pcAction, &ActionGroup::aboutToShow, [](QMenu* menu) {
        MainWindow::getInstance()->populateDockWindowMenu(menu);
    });
    return pcAction;
}

void StdCmdDockViewMenu::activated(int) { /* TODO */ }

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:409-420
StdCmdStatusBar::StdCmdStatusBar()
    : Command("Std_ViewStatusBar")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("Status Bar");
    sToolTipText = QT_TR_NOOP("Toggles the status bar");
    sWhatsThis = "Std_ViewStatusBar";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:422-431
Action* StdCmdStatusBar::createAction()
{
    Action* pcAction = Command::createAction();
    pcAction->setCheckable(true);
    pcAction->setChecked(false);
    // TODO: wire FilterStatusBar event filter when MainWindow status bar is ready
    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:433-436
void StdCmdStatusBar::activated(int iMsg)
{
    if (auto* mw = getMainWindow())
        mw->statusBar()->setVisible(iMsg != 0);
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:438-449
bool StdCmdStatusBar::isActive()
{
    return true;
}

// =====================================================================
// Registration helper — called by Task 10 (CreateStdCommands)
// =====================================================================

void createViewCommands(CommandManager& mgr)
{
    // View creation / camera
    mgr.addCommand(new StdCmdViewCreate());
    mgr.addCommand(new StdOrthographicCamera());
    mgr.addCommand(new StdPerspectiveCamera());

    // Fullscreen
    mgr.addCommand(new StdMainFullscreen());

    // Fit
    mgr.addCommand(new StdCmdViewFitAll());

    // Individual standard views
    mgr.addCommand(new StdCmdViewIsometric());
    mgr.addCommand(new StdCmdViewFront());
    mgr.addCommand(new StdCmdViewTop());
    mgr.addCommand(new StdCmdViewRight());
    mgr.addCommand(new StdCmdViewRear());
    mgr.addCommand(new StdCmdViewBottom());
    mgr.addCommand(new StdCmdViewLeft());

    // Panel menus
    mgr.addCommand(new StdCmdDockViewMenu());

    // Status bar
    mgr.addCommand(new StdCmdStatusBar());
}

} // namespace Gui
