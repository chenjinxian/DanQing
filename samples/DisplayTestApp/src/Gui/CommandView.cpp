// Ported from: FreeCAD src/Gui/CommandView.cpp + CommandWindow.cpp + CommandStd.cpp
//              View domain commands. Each command ctor metadata is ported verbatim from
//              FreeCAD source; activated()/isActive() bodies are stubs until backend is wired.
#include "CommandView.h"
#include "Application.h"  // getGuiApplication() → sendMsgToActiveView / sendHasMsgToActiveView
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QKeySequence>
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
// Std_ViewFitSelection
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1771-1784
StdCmdViewFitSelection::StdCmdViewFitSelection()
    : Command("Std_ViewFitSelection")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Fit &Selection");
    sToolTipText = QT_TR_NOOP("Fits the selected content into the 3D view");
    sWhatsThis = "Std_ViewFitSelection";
    sStatusTip = sToolTipText;
    sAccel = "V, S";
    sPixmap = "zoom-selection";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1786-1790
void StdCmdViewFitSelection::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1792-1795
bool StdCmdViewFitSelection::isActive()
{
    return false;  // deferred — Step 3 (view backend)
}

// =====================================================================
// Std_ViewGroup (Standard Views dropdown)
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1803-1812
StdCmdViewGroup::StdCmdViewGroup()
    : Command("Std_ViewGroup")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Standard &Views");
    sToolTipText = QT_TR_NOOP("Changes to a standard view");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ViewGroup";
    sPixmap = "view-isometric";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1800-1835
// C++ adaptation: manual ActionGroup instead of GroupCommand
Action* StdCmdViewGroup::createAction()
{
    auto pcAction = new ActionGroup(this, getMainWindow());
    pcAction->setDropDownMenu(true);
    applyCommandData(this->className(), pcAction);

    const char* viewNames[] = {
        "Isometric", "Front", "Top", "Right", "Rear", "Bottom", "Left"
    };
    const char* viewAccels[] = {
        "0", "1", "2", "3", "4", "5", "6"
    };
    for (int i = 0; i < 7; ++i) {
        QAction* a = pcAction->addAction(QString());
        a->setText(QCoreApplication::translate("Std_ViewGroup", viewNames[i]));
        a->setObjectName(QStringLiteral("Std_View%1").arg(QLatin1String(viewNames[i])));
        a->setShortcut(QKeySequence(QString::fromLatin1(viewAccels[i])));
        a->setWhatsThis(QString::fromLatin1(getWhatsThis()));
    }

    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1826-1834
void StdCmdViewGroup::activated(int /*iMsg*/)
{
    // TODO: wire to switch standard view
}

// =====================================================================
// Std_AlignToSelection
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:4038-4049
StdCmdAlignToSelection::StdCmdAlignToSelection()
    : Command("Std_AlignToSelection")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("&Align to Selection");
    sToolTipText = QT_TR_NOOP("Aligns the camera view to the selected elements in the 3D view");
    sWhatsThis = "Std_AlignToSelection";
    sPixmap = "align-to-selection";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:4051-4055
void StdCmdAlignToSelection::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:4057-4060
bool StdCmdAlignToSelection::isActive()
{
    return false;  // deferred — Step 3 (view backend)
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

// Ported from: FreeCAD src/Gui/CommandView.cpp:1636-1648
StdCmdViewDimetric::StdCmdViewDimetric()
    : Command("Std_ViewDimetric")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Dimetric");
    sToolTipText = QT_TR_NOOP("Sets the camera to the dimetric view");
    sWhatsThis = "Std_ViewDimetric";
    sStatusTip = sToolTipText;
    sPixmap = "Std_ViewDimetric";
    eType = Alter3DView;
}

void StdCmdViewDimetric::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdViewDimetric::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1664-1676
StdCmdViewTrimetric::StdCmdViewTrimetric()
    : Command("Std_ViewTrimetric")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Trimetric");
    sToolTipText = QT_TR_NOOP("Sets the camera to the trimetric view");
    sWhatsThis = "Std_ViewTrimetric";
    sStatusTip = sToolTipText;
    sPixmap = "Std_ViewTrimetric";
    eType = Alter3DView;
}

void StdCmdViewTrimetric::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdViewTrimetric::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1408-1421
StdCmdViewHome::StdCmdViewHome()
    : Command("Std_ViewHome")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Home");
    sToolTipText = QT_TR_NOOP("Sets the camera to the default home view");
    sWhatsThis = "Std_ViewHome";
    sStatusTip = sToolTipText;
    sPixmap = "Std_ViewHome";
    sAccel = "Home";
    eType = Alter3DView;
}

void StdCmdViewHome::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdViewHome::isActive() { return false; }  // deferred — Step 3 (view backend)

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

// Ported from: FreeCAD src/Gui/CommandView.cpp:1692-1705
StdCmdViewRotateLeft::StdCmdViewRotateLeft()
    : Command("Std_ViewRotateLeft")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Rotate &Left");
    sToolTipText = QT_TR_NOOP("Rotates the view by 90 degrees counter-clockwise");
    sWhatsThis = "Std_ViewRotateLeft";
    sStatusTip = sToolTipText;
    sPixmap = "view-rotate-left";
    sAccel = "Shift+Left";
    eType = Alter3DView;
}

void StdCmdViewRotateLeft::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdViewRotateLeft::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1717-1730
StdCmdViewRotateRight::StdCmdViewRotateRight()
    : Command("Std_ViewRotateRight")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Rotates &Right");
    sToolTipText = QT_TR_NOOP("Rotates the view by 90 degrees clockwise");
    sWhatsThis = "Std_ViewRotateRight";
    sStatusTip = sToolTipText;
    sPixmap = "view-rotate-right";
    sAccel = "Shift+Right";
    eType = Alter3DView;
}

void StdCmdViewRotateRight::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdViewRotateRight::isActive() { return false; }  // deferred — Step 3 (view backend)

// =====================================================================
// Std_DrawStyle (group command with 7 checkable items)
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:758-772
StdCmdDrawStyle::StdCmdDrawStyle()
    : Command("Std_DrawStyle")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Draw Style");
    sToolTipText = QT_TR_NOOP("Changes the draw style of the objects");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_DrawStyle";
    sPixmap = "DrawStyleAsIs";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:774-830
Action* StdCmdDrawStyle::createAction()
{
    auto pcAction = new ActionGroup(this, getMainWindow());
    pcAction->setDropDownMenu(true);
    pcAction->setIsMode(true);
    applyCommandData(this->className(), pcAction);

    const char* modeNames[] = {
        "AsIs", "Points", "Wireframe", "HiddenLine", "NoShading", "Shaded", "FlatLines"
    };
    const char* modeShortcuts[] = {
        "V,1", "V,2", "V,3", "V,4", "V,5", "V,6", "V,7"
    };

    for (int i = 0; i < 7; ++i) {
        QAction* a = pcAction->addAction(QString());
        a->setCheckable(true);
        a->setObjectName(QStringLiteral("Std_DrawStyle%1").arg(QLatin1String(modeNames[i])));
        a->setShortcut(QKeySequence(QString::fromLatin1(modeShortcuts[i])));
        a->setWhatsThis(QString::fromLatin1(getWhatsThis()));
        if (i == 0) {
            a->setChecked(true);
            pcAction->setIcon(a->icon());
        }
    }

    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:907-953
void StdCmdDrawStyle::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:955-958
bool StdCmdDrawStyle::isActive()
{
    return false;  // deferred — Step 3 (view backend)
}

// =====================================================================
// Zoom commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:2625-2638
StdViewZoomIn::StdViewZoomIn()
    : Command("Std_ViewZoomIn")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Zoom &In");
    sToolTipText = QT_TR_NOOP("Increases the zoom factor by a fixed amount");
    sWhatsThis = "Std_ViewZoomIn";
    sStatusTip = sToolTipText;
    sPixmap = "zoom-in";
    sAccel = keySequenceToAccel(QKeySequence::ZoomIn);
    eType = Alter3DView;
}

void StdViewZoomIn::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewZoomIn::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2654-2667
StdViewZoomOut::StdViewZoomOut()
    : Command("Std_ViewZoomOut")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Zoom &Out");
    sToolTipText = QT_TR_NOOP("Decreases the zoom factor by a fixed amount");
    sWhatsThis = "Std_ViewZoomOut";
    sStatusTip = sToolTipText;
    sPixmap = "zoom-out";
    sAccel = keySequenceToAccel(QKeySequence::ZoomOut);
    eType = Alter3DView;
}

void StdViewZoomOut::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewZoomOut::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2820-2833
StdViewBoxZoom::StdViewBoxZoom()
    : Command("Std_ViewBoxZoom")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Box Zoom");
    sToolTipText = QT_TR_NOOP("Activates the box zoom tool");
    sWhatsThis = "Std_ViewBoxZoom";
    sStatusTip = sToolTipText;
    sPixmap = "zoom-border";
    sAccel = "Ctrl+B";
    eType = Alter3DView;
}

void StdViewBoxZoom::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewBoxZoom::isActive() { return false; }  // deferred — Step 3 (view backend)

// =====================================================================
// View toggle commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:2432-2444
StdCmdAxisCross::StdCmdAxisCross()
    : Command("Std_AxisCross")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Toggle A&xis Cross");
    sToolTipText = QT_TR_NOOP("Toggles the axis cross at the origin");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_AxisCross";
    sPixmap = "Std_AxisCross";
    sAccel = "A,C";
}

void StdCmdAxisCross::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdAxisCross::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:672-682
StdCmdToggleClipPlane::StdCmdToggleClipPlane()
    : Command("Std_ToggleClipPlane")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Clippin&g View");
    sToolTipText = QT_TR_NOOP("Toggles clipping of the active view");
    sWhatsThis = "Std_ToggleClipPlane";
    sStatusTip = sToolTipText;
    sPixmap = "Std_ToggleClipPlane";
    eType = Alter3DView;
}

void StdCmdToggleClipPlane::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdToggleClipPlane::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:3155-3168
StdCmdTextureMapping::StdCmdTextureMapping()
    : Command("Std_TextureMapping")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Text&ure Mapping");
    sToolTipText = QT_TR_NOOP("Maps textures to shapes");
    sWhatsThis = "Std_TextureMapping";
    sStatusTip = sToolTipText;
    sPixmap = "Std_TextureMapping";
    eType = Alter3DView;
}

void StdCmdTextureMapping::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdTextureMapping::isActive() { return false; }  // deferred — Step 3 (view backend)

// =====================================================================
// Visibility commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:963-976
StdCmdToggleVisibility::StdCmdToggleVisibility()
    : Command("Std_ToggleVisibility")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Toggle &Visibility");
    sToolTipText = QT_TR_NOOP("Toggles the visibility of the selection");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ToggleVisibility";
    sPixmap = "Std_ToggleVisibility";
    sAccel = "Space";
    eType = Alter3DView;
}

void StdCmdToggleVisibility::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdToggleVisibility::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:997-1014
StdCmdToggleTransparency::StdCmdToggleTransparency()
    : Command("Std_ToggleTransparency")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Toggle Transparenc&y");
    sToolTipText = QT_TR_NOOP(
        "Toggles the transparency of the selected objects. Transparency "
        "can be fine-tuned in the appearance task dialog"
    );
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ToggleTransparency";
    sPixmap = "Std_ToggleTransparency";
    sAccel = "V,T";
    eType = Alter3DView;
}

void StdCmdToggleTransparency::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdToggleTransparency::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1110-1122
StdCmdToggleSelectability::StdCmdToggleSelectability()
    : Command("Std_ToggleSelectability")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Toggle Se&lectability");
    sToolTipText = QT_TR_NOOP(
        "Toggles the property of the objects to get selected in the 3D view"
    );
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ToggleSelectability";
    sPixmap = "view-unselectable";
    eType = Alter3DView;
}

void StdCmdToggleSelectability::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdToggleSelectability::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1172-1184
StdCmdShowSelection::StdCmdShowSelection()
    : Command("Std_ShowSelection")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Sho&w Selection");
    sToolTipText = QT_TR_NOOP("Shows all selected objects");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ShowSelection";
    sPixmap = "Std_ShowSelection";
    eType = Alter3DView;
}

void StdCmdShowSelection::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdShowSelection::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1200-1212
StdCmdHideSelection::StdCmdHideSelection()
    : Command("Std_HideSelection")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Hide Selection");
    sToolTipText = QT_TR_NOOP("Hides all selected objects");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_HideSelection";
    sPixmap = "Std_HideSelection";
    eType = Alter3DView;
}

void StdCmdHideSelection::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdHideSelection::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1228-1240
StdCmdSelectVisibleObjects::StdCmdSelectVisibleObjects()
    : Command("Std_SelectVisibleObjects")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Select Visible Objects");
    sToolTipText = QT_TR_NOOP("Selects all visible objects in the active document");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_SelectVisibleObjects";
    sPixmap = "Std_SelectVisibleObjects";
    eType = Alter3DView;
}

void StdCmdSelectVisibleObjects::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdSelectVisibleObjects::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1272-1284
StdCmdToggleObjects::StdCmdToggleObjects()
    : Command("Std_ToggleObjects")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("To&ggle All Objects");
    sToolTipText = QT_TR_NOOP("Toggles the visibility of all objects in the active document");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ToggleObjects";
    sPixmap = "Std_ToggleObjects";
    eType = Alter3DView;
}

void StdCmdToggleObjects::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdToggleObjects::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1324-1336
StdCmdShowObjects::StdCmdShowObjects()
    : Command("Std_ShowObjects")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Show &All Objects");
    sToolTipText = QT_TR_NOOP("Shows all objects in the document");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ShowObjects";
    sPixmap = "Std_ShowObjects";
    eType = Alter3DView;
}

void StdCmdShowObjects::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdShowObjects::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1366-1378
StdCmdHideObjects::StdCmdHideObjects()
    : Command("Std_HideObjects")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Hide All &Objects");
    sToolTipText = QT_TR_NOOP("Hides all objects in the document");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_HideObjects";
    sPixmap = "Std_HideObjects";
    eType = Alter3DView;
}

void StdCmdHideObjects::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdHideObjects::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2384-2398
StdCmdToggleNavigation::StdCmdToggleNavigation()
    : Command("Std_ToggleNavigation")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Toggle Navigation/&Edit Mode");
    sToolTipText = QT_TR_NOOP("Toggles between navigation and edit mode");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ToggleNavigation";
    sAccel = "Esc";
    sPixmap = "Std_ToggleNavigation";
    eType = Alter3DView;
}

void StdCmdToggleNavigation::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdToggleNavigation::isActive() { return false; }  // deferred — Step 3 (view backend)

// =====================================================================
// Workbench, Toolbar, Dock, Status bar
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandStd.cpp:70-82
StdCmdWorkbench::StdCmdWorkbench()
    : Command("Std_Workbench")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("&Workbench");
    sToolTipText = QT_TR_NOOP("Switches between workbenches");
    sWhatsThis = "Std_Workbench";
    sStatusTip = sToolTipText;
    sPixmap = "freecad";
    eType = 0;
}

void StdCmdWorkbench::activated(int) { /* TODO */ }

// Ported from: FreeCAD src/Gui/CommandStd.cpp:123-135
Action* StdCmdWorkbench::createAction()
{
    Action* pcAction = new WorkbenchGroup(this, getMainWindow());
    pcAction->setShortcut(QString::fromLatin1(getAccel()));
    applyCommandData(this->className(), pcAction);
    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp (ToolBarMenu section)
StdCmdToolBarMenu::StdCmdToolBarMenu()
    : Command("Std_ToolBarMenu")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("&Toolbars");
    sToolTipText = QT_TR_NOOP("Toggles toolbars");
    sWhatsThis = "Std_ToolBarMenu";
    sStatusTip = sToolTipText;
    eType = 0;
    bCanLog = false;
}

// TODO: wire dynamic toggle list from ToolBarManager
Action* StdCmdToolBarMenu::createAction()
{
    auto pcAction = new ActionGroup(this, getMainWindow());
    pcAction->setDropDownMenu(true);
    applyCommandData(this->className(), pcAction);
    // TODO: wire dynamic toggle list
    return pcAction;
}

void StdCmdToolBarMenu::activated(int) { /* TODO */ }

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

// Ported from: FreeCAD src/Gui/CommandView.cpp:3884-3897
StdCmdToggleBottomPanels::StdCmdToggleBottomPanels()
    : Command("Std_ToggleBottomPanels")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("Toggle Bottom Panels");
    sToolTipText = QT_TR_NOOP("Toggles the bottom dock panels");
    sWhatsThis = "Std_ToggleBottomPanels";
    sStatusTip = sToolTipText;
    sAccel = "Ctrl+0";
    sPixmap = "Std_ToggleBottomPanels";
    eType = NoTransaction;
}

void StdCmdToggleBottomPanels::activated(int) { /* TODO */ }

// Ported from: FreeCAD src/Gui/CommandView.cpp:3965-3971
Action* StdCmdToggleBottomPanels::createAction()
{
    auto* pcAction = Command::createAction();
    pcAction->setCheckable(true);
    return pcAction;
}

bool StdCmdToggleBottomPanels::isActive() { return true; }

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
// Misc view commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:2080-2091
StdCmdViewVR::StdCmdViewVR()
    : Command("Std_ViewVR")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("FreeCAD VR");
    sToolTipText = QT_TR_NOOP("Extends the FreeCAD 3D Window to a VR device");
    sWhatsThis = "Std_ViewVR";
    sStatusTip = sToolTipText;
    eType = Alter3DView;
}

void StdCmdViewVR::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdViewVR::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:4066-4082
StdCmdClarifySelection::StdCmdClarifySelection()
    : Command("Std_ClarifySelection")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("Clarify Selection");
    sToolTipText = QT_TR_NOOP(
        "Displays a context menu at the mouse cursor to select overlapping "
        "or obstructed geometry in the 3D view."
    );
    sWhatsThis = "Std_ClarifySelection";
    sStatusTip = sToolTipText;
    sAccel = "G, G";
    sPixmap = "tree-pre-sel";
    eType = NoTransaction | AlterSelection;
}

void StdCmdClarifySelection::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdClarifySelection::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:3982-3994
StdStoreWorkingView::StdStoreWorkingView()
    : Command("Std_StoreWorkingView")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("St&ore Working View");
    sToolTipText = QT_TR_NOOP("Stores a temporary working view for the current document");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_StoreWorkingView";
    sAccel = "Shift+End";
    eType = NoTransaction;
}

void StdStoreWorkingView::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdStoreWorkingView::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:4010-4022
StdRecallWorkingView::StdRecallWorkingView()
    : Command("Std_RecallWorkingView")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("R&ecall Working View");
    sToolTipText = QT_TR_NOOP("Recalls a previously stored temporary working view");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_RecallWorkingView";
    sAccel = "End";
    eType = NoTransaction;
}

void StdRecallWorkingView::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdRecallWorkingView::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2108-2120
StdViewScreenShot::StdViewScreenShot()
    : Command("Std_ViewScreenShot")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Save &Image...");
    sToolTipText = QT_TR_NOOP("Creates a screenshot of the active view");
    sWhatsThis = "Std_ViewScreenShot";
    sStatusTip = sToolTipText;
    sPixmap = "Std_ViewScreenShot";
    eType = Alter3DView;
}

void StdViewScreenShot::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewScreenShot::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2308-2320
StdViewLoadImage::StdViewLoadImage()
    : Command("Std_ViewLoadImage")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Load Image...");
    sToolTipText = QT_TR_NOOP("Loads an image");
    sWhatsThis = "Std_ViewLoadImage";
    sStatusTip = sToolTipText;
    sPixmap = "image-open";
    eType = 0;
}

void StdViewLoadImage::activated(int) { /* TODO */ }

// Ported from: FreeCAD src/Gui/CommandView.cpp:3557-3569
StdCmdSelBoundingBox::StdCmdSelBoundingBox()
    : Command("Std_SelBoundingBox")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("&Bounding Box");
    sToolTipText = QT_TR_NOOP("Shows selection bounding box");
    sWhatsThis = "Std_SelBoundingBox";
    sStatusTip = sToolTipText;
    sPixmap = "sel-bbox";
    eType = Alter3DView;
}

void StdCmdSelBoundingBox::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdSelBoundingBox::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:3593-3598
Action* StdCmdSelBoundingBox::createAction()
{
    Action* pcAction = Command::createAction();
    pcAction->setCheckable(true);
    return pcAction;
}

// =====================================================================
// Dock/Undock/Fullscreen sub-commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1840-1855
StdViewDock::StdViewDock()
    : Command("Std_ViewDock")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Docked");
    sToolTipText = QT_TR_NOOP(
        "Displays the active view either in fullscreen, undocked, or docked mode"
    );
    sWhatsThis = "Std_ViewDock";
    sStatusTip = sToolTipText;
    sAccel = "V, D";
    eType = Alter3DView;
    bCanLog = false;
}

void StdViewDock::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewDock::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1871-1886
StdViewUndock::StdViewUndock()
    : Command("Std_ViewUndock")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Undocked");
    sToolTipText = QT_TR_NOOP(
        "Displays the active view either in fullscreen, undocked, or docked mode"
    );
    sWhatsThis = "Std_ViewUndock";
    sStatusTip = sToolTipText;
    sAccel = "V, U";
    eType = Alter3DView;
    bCanLog = false;
}

void StdViewUndock::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewUndock::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1937-1953
StdViewFullscreen::StdViewFullscreen()
    : Command("Std_ViewFullscreen")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Fullscreen");
    sToolTipText = QT_TR_NOOP(
        "Displays the active view either in fullscreen, undocked, or docked mode"
    );
    sWhatsThis = "Std_ViewFullscreen";
    sStatusTip = sToolTipText;
    sPixmap = "view-fullscreen";
    sAccel = "F11";
    eType = Alter3DView;
    bCanLog = false;
}

void StdViewFullscreen::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdViewFullscreen::isActive() { return false; }  // deferred — Step 3 (view backend)

// Ported from: FreeCAD src/Gui/CommandView.cpp:1969-1987
StdViewDockUndockFullscreen::StdViewDockUndockFullscreen()
    : Command("Std_ViewDockUndockFullscreen")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("D&ocument Window");
    sToolTipText = QT_TR_NOOP(
        "Displays the active view either in fullscreen, undocked, or docked mode"
    );
    sWhatsThis = "Std_ViewDockUndockFullscreen";
    sStatusTip = sToolTipText;
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:1989-2004
Action* StdViewDockUndockFullscreen::createAction()
{
    auto pcAction = new ActionGroup(this, getMainWindow());
    pcAction->setDropDownMenu(true);
    pcAction->setText(QCoreApplication::translate(this->className(), getMenuText()));

    const char* labels[] = {"&Docked", "&Undocked", "&Fullscreen"};
    const char* names[] = {"Std_ViewDock", "Std_ViewUndock", "Std_ViewFullscreen"};
    for (int i = 0; i < 3; ++i) {
        QAction* a = pcAction->addAction(QString());
        a->setText(QCoreApplication::translate("Std_ViewDockUndockFullscreen", labels[i]));
        a->setObjectName(QStringLiteral("%1").arg(QLatin1String(names[i])));
        a->setWhatsThis(QString::fromLatin1(getWhatsThis()));
    }

    return pcAction;
}

void StdViewDockUndockFullscreen::activated(int) { /* TODO */ }
bool StdViewDockUndockFullscreen::isActive() { return true; }

// =====================================================================
// Tree View Actions group
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:3514-3523
StdCmdTreeViewActions::StdCmdTreeViewActions()
    : Command("Std_TreeViewActions")
{
    sGroup = "TreeView";
    sMenuText = QT_TR_NOOP("Tree View Actions");
    sToolTipText = QT_TR_NOOP("Tree view behavior options and actions");
    sWhatsThis = "Std_TreeViewActions";
    sStatusTip = sToolTipText;
    eType = 0;
    bCanLog = false;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:3511-3551
Action* StdCmdTreeViewActions::createAction()
{
    auto pcAction = new ActionGroup(this, getMainWindow());
    pcAction->setDropDownMenu(true);
    applyCommandData(this->className(), pcAction);

    // Sync view/selection/placement/preselection/recordselection (checkable)
    const char* syncCmds[] = {
        "SyncView", "SyncSelection", "SyncPlacement", "PreSelection", "RecordSelection"
    };
    for (int i = 0; i < 5; ++i) {
        QAction* a = pcAction->addAction(QString());
        a->setCheckable(true);
        a->setObjectName(QStringLiteral("Std_Tree") + QLatin1String(syncCmds[i]));
        a->setWhatsThis(QString::fromLatin1(getWhatsThis()));
    }

    pcAction->addAction(QStringLiteral(""))->setSeparator(true);

    // Document mode (checkable)
    const char* docCmds[] = {
        "SingleDocument", "MultiDocument", "CollapseDocument"
    };
    for (int i = 0; i < 3; ++i) {
        QAction* a = pcAction->addAction(QString());
        a->setCheckable(true);
        a->setObjectName(QStringLiteral("Std_Tree") + QLatin1String(docCmds[i]));
        a->setWhatsThis(QString::fromLatin1(getWhatsThis()));
    }

    pcAction->addAction(QStringLiteral(""))->setSeparator(true);

    // Drag + Go to Selection
    QAction* drag = pcAction->addAction(QString());
    drag->setObjectName(QStringLiteral("Std_TreeDrag"));
    drag->setWhatsThis(QString::fromLatin1(getWhatsThis()));

    QAction* sel = pcAction->addAction(QString());
    sel->setObjectName(QStringLiteral("Std_TreeSelection"));
    sel->setWhatsThis(QString::fromLatin1(getWhatsThis()));

    pcAction->addAction(QStringLiteral(""))->setSeparator(true);

    // SelBack + SelForward
    QAction* back = pcAction->addAction(QString());
    back->setObjectName(QStringLiteral("Std_SelBack"));
    back->setWhatsThis(QString::fromLatin1(getWhatsThis()));

    QAction* fwd = pcAction->addAction(QString());
    fwd->setObjectName(QStringLiteral("Std_SelForward"));
    fwd->setWhatsThis(QString::fromLatin1(getWhatsThis()));

    return pcAction;
}

void StdCmdTreeViewActions::activated(int)
{
    // TODO: deferred — Step 3 (view backend); see CLAUDE.md §0. Backend not yet implemented.
}
bool StdCmdTreeViewActions::isActive() { return false; }  // deferred — Step 3 (view backend)

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
    mgr.addCommand(new StdViewDockUndockFullscreen());

    // Fit
    mgr.addCommand(new StdCmdViewFitAll());
    mgr.addCommand(new StdCmdViewFitSelection());
    mgr.addCommand(new StdCmdAlignToSelection());

    // Standard views group
    mgr.addCommand(new StdCmdViewGroup());

    // Individual standard views
    mgr.addCommand(new StdCmdViewIsometric());
    mgr.addCommand(new StdCmdViewDimetric());
    mgr.addCommand(new StdCmdViewTrimetric());
    mgr.addCommand(new StdCmdViewHome());
    mgr.addCommand(new StdCmdViewFront());
    mgr.addCommand(new StdCmdViewTop());
    mgr.addCommand(new StdCmdViewRight());
    mgr.addCommand(new StdCmdViewRear());
    mgr.addCommand(new StdCmdViewBottom());
    mgr.addCommand(new StdCmdViewLeft());
    mgr.addCommand(new StdCmdViewRotateLeft());
    mgr.addCommand(new StdCmdViewRotateRight());

    // Working view
    mgr.addCommand(new StdStoreWorkingView());
    mgr.addCommand(new StdRecallWorkingView());

    // Zoom
    mgr.addCommand(new StdViewZoomIn());
    mgr.addCommand(new StdViewZoomOut());
    mgr.addCommand(new StdViewBoxZoom());

    // Draw style group
    mgr.addCommand(new StdCmdDrawStyle());

    // View toggles
    mgr.addCommand(new StdCmdAxisCross());
    mgr.addCommand(new StdCmdToggleClipPlane());
    mgr.addCommand(new StdCmdTextureMapping());

    // Visibility
    mgr.addCommand(new StdCmdToggleVisibility());
    mgr.addCommand(new StdCmdToggleTransparency());
    mgr.addCommand(new StdCmdToggleSelectability());
    mgr.addCommand(new StdCmdShowSelection());
    mgr.addCommand(new StdCmdHideSelection());
    mgr.addCommand(new StdCmdSelectVisibleObjects());
    mgr.addCommand(new StdCmdToggleObjects());
    mgr.addCommand(new StdCmdShowObjects());
    mgr.addCommand(new StdCmdHideObjects());
    mgr.addCommand(new StdCmdToggleNavigation());

    // Workbench
    mgr.addCommand(new StdCmdWorkbench());

    // Toolbar / Panel menus
    mgr.addCommand(new StdCmdToolBarMenu());
    mgr.addCommand(new StdCmdDockViewMenu());
    mgr.addCommand(new StdCmdToggleBottomPanels());

    // Status bar
    mgr.addCommand(new StdCmdStatusBar());

    // Selection bounding box
    mgr.addCommand(new StdCmdSelBoundingBox());
    mgr.addCommand(new StdCmdClarifySelection());

    // Screen shot / Load image
    mgr.addCommand(new StdViewScreenShot());
    mgr.addCommand(new StdViewLoadImage());

    // Tree view actions group
    mgr.addCommand(new StdCmdTreeViewActions());

    // VR
    mgr.addCommand(new StdCmdViewVR());

    // Sub-commands (registered but typically used via groups)
    mgr.addCommand(new StdViewDock());
    mgr.addCommand(new StdViewUndock());
    mgr.addCommand(new StdViewFullscreen());
}

} // namespace Gui
