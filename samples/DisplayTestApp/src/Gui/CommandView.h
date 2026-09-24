// Ported from: FreeCAD src/Gui/CommandView.cpp (header for registration + class declarations)
// Declares command classes + createViewCommands() — registers all View domain commands.
#pragma once

#include "Command.h"
#include "Action.h"

namespace Gui {

class CommandManager;

// =====================================================================
// Camera commands (checkable, mutually exclusive)
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:140-153 (DEF_STD_CMD_AC)
class StdOrthographicCamera : public Command {
public:
    StdOrthographicCamera();
    ~StdOrthographicCamera() override = default;
    const char* className() const override { return "StdOrthographicCamera"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:187-199 (DEF_STD_CMD_AC)
class StdPerspectiveCamera : public Command {
public:
    StdPerspectiveCamera();
    ~StdPerspectiveCamera() override = default;
    const char* className() const override { return "StdPerspectiveCamera"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

// =====================================================================
// Standard view commands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1742-1755 (DEF_STD_CMD_A)
class StdCmdViewFitAll : public Command {
public:
    StdCmdViewFitAll();
    ~StdCmdViewFitAll() override = default;
    const char* className() const override { return "StdCmdViewFitAll"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1771-1784 (DEF_STD_CMD_A)
class StdCmdViewFitSelection : public Command {
public:
    StdCmdViewFitSelection();
    ~StdCmdViewFitSelection() override = default;
    const char* className() const override { return "StdCmdViewFitSelection"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1800-1835 (group command)
class StdCmdViewGroup : public Command {
public:
    StdCmdViewGroup();
    ~StdCmdViewGroup() override = default;
    const char* className() const override { return "StdCmdViewGroup"; }
protected:
    void activated(int iMsg) override;
    Action* createAction() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:4038-4049 (DEF_STD_CMD_A)
class StdCmdAlignToSelection : public Command {
public:
    StdCmdAlignToSelection();
    ~StdCmdAlignToSelection() override = default;
    const char* className() const override { return "StdCmdAlignToSelection"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// =====================================================================
// Individual standard views
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandView.cpp:1607-1620 (DEF_STD_CMD_A)
class StdCmdViewIsometric : public Command {
public:
    StdCmdViewIsometric();
    ~StdCmdViewIsometric() override = default;
    const char* className() const override { return "StdCmdViewIsometric"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1636-1648 (DEF_STD_CMD_A)
class StdCmdViewDimetric : public Command {
public:
    StdCmdViewDimetric();
    ~StdCmdViewDimetric() override = default;
    const char* className() const override { return "StdCmdViewDimetric"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1664-1676 (DEF_STD_CMD_A)
class StdCmdViewTrimetric : public Command {
public:
    StdCmdViewTrimetric();
    ~StdCmdViewTrimetric() override = default;
    const char* className() const override { return "StdCmdViewTrimetric"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1408-1421 (DEF_STD_CMD_A)
class StdCmdViewHome : public Command {
public:
    StdCmdViewHome();
    ~StdCmdViewHome() override = default;
    const char* className() const override { return "StdCmdViewHome"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1461-1474 (DEF_STD_CMD_A)
class StdCmdViewFront : public Command {
public:
    StdCmdViewFront();
    ~StdCmdViewFront() override = default;
    const char* className() const override { return "StdCmdViewFront"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1577-1590 (DEF_STD_CMD_A)
class StdCmdViewTop : public Command {
public:
    StdCmdViewTop();
    ~StdCmdViewTop() override = default;
    const char* className() const override { return "StdCmdViewTop"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1548-1561 (DEF_STD_CMD_A)
class StdCmdViewRight : public Command {
public:
    StdCmdViewRight();
    ~StdCmdViewRight() override = default;
    const char* className() const override { return "StdCmdViewRight"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1519-1532 (DEF_STD_CMD_A)
class StdCmdViewRear : public Command {
public:
    StdCmdViewRear();
    ~StdCmdViewRear() override = default;
    const char* className() const override { return "StdCmdViewRear"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1432-1445 (DEF_STD_CMD_A)
class StdCmdViewBottom : public Command {
public:
    StdCmdViewBottom();
    ~StdCmdViewBottom() override = default;
    const char* className() const override { return "StdCmdViewBottom"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1490-1503 (DEF_STD_CMD_A)
class StdCmdViewLeft : public Command {
public:
    StdCmdViewLeft();
    ~StdCmdViewLeft() override = default;
    const char* className() const override { return "StdCmdViewLeft"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1692-1705 (DEF_STD_CMD_A)
class StdCmdViewRotateLeft : public Command {
public:
    StdCmdViewRotateLeft();
    ~StdCmdViewRotateLeft() override = default;
    const char* className() const override { return "StdCmdViewRotateLeft"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Gui/CommandView.cpp:1717-1730 (DEF_STD_CMD_A)
class StdCmdViewRotateRight : public Command {
public:
    StdCmdViewRotateRight();
    ~StdCmdViewRotateRight() override = default;
    const char* className() const override { return "StdCmdViewRotateRight"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// =====================================================================
// Draw Style group command (7 checkable items)
// =====================================================================

class StdCmdDrawStyle : public Command {
public:
    StdCmdDrawStyle();
    ~StdCmdDrawStyle() override = default;
    const char* className() const override { return "StdCmdDrawStyle"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

// =====================================================================
// Zoom commands
// =====================================================================

class StdViewZoomIn : public Command {
public:
    StdViewZoomIn();
    ~StdViewZoomIn() override = default;
    const char* className() const override { return "StdViewZoomIn"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdViewZoomOut : public Command {
public:
    StdViewZoomOut();
    ~StdViewZoomOut() override = default;
    const char* className() const override { return "StdViewZoomOut"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdViewBoxZoom : public Command {
public:
    StdViewBoxZoom();
    ~StdViewBoxZoom() override = default;
    const char* className() const override { return "StdViewBoxZoom"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// =====================================================================
// View toggle commands
// =====================================================================

class StdCmdAxisCross : public Command {
public:
    StdCmdAxisCross();
    ~StdCmdAxisCross() override = default;
    const char* className() const override { return "StdCmdAxisCross"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdToggleClipPlane : public Command {
public:
    StdCmdToggleClipPlane();
    ~StdCmdToggleClipPlane() override = default;
    const char* className() const override { return "StdCmdToggleClipPlane"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdTextureMapping : public Command {
public:
    StdCmdTextureMapping();
    ~StdCmdTextureMapping() override = default;
    const char* className() const override { return "StdCmdTextureMapping"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// =====================================================================
// Visibility commands
// =====================================================================

class StdCmdToggleVisibility : public Command {
public:
    StdCmdToggleVisibility();
    ~StdCmdToggleVisibility() override = default;
    const char* className() const override { return "StdCmdToggleVisibility"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdToggleTransparency : public Command {
public:
    StdCmdToggleTransparency();
    ~StdCmdToggleTransparency() override = default;
    const char* className() const override { return "StdCmdToggleTransparency"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdToggleSelectability : public Command {
public:
    StdCmdToggleSelectability();
    ~StdCmdToggleSelectability() override = default;
    const char* className() const override { return "StdCmdToggleSelectability"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdShowSelection : public Command {
public:
    StdCmdShowSelection();
    ~StdCmdShowSelection() override = default;
    const char* className() const override { return "StdCmdShowSelection"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdHideSelection : public Command {
public:
    StdCmdHideSelection();
    ~StdCmdHideSelection() override = default;
    const char* className() const override { return "StdCmdHideSelection"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdSelectVisibleObjects : public Command {
public:
    StdCmdSelectVisibleObjects();
    ~StdCmdSelectVisibleObjects() override = default;
    const char* className() const override { return "StdCmdSelectVisibleObjects"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdToggleObjects : public Command {
public:
    StdCmdToggleObjects();
    ~StdCmdToggleObjects() override = default;
    const char* className() const override { return "StdCmdToggleObjects"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdShowObjects : public Command {
public:
    StdCmdShowObjects();
    ~StdCmdShowObjects() override = default;
    const char* className() const override { return "StdCmdShowObjects"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdHideObjects : public Command {
public:
    StdCmdHideObjects();
    ~StdCmdHideObjects() override = default;
    const char* className() const override { return "StdCmdHideObjects"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdToggleNavigation : public Command {
public:
    StdCmdToggleNavigation();
    ~StdCmdToggleNavigation() override = default;
    const char* className() const override { return "StdCmdToggleNavigation"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// =====================================================================
// Workbench, Toolbar, Dock, Status bar
// =====================================================================

class StdCmdWorkbench : public Command {
public:
    StdCmdWorkbench();
    ~StdCmdWorkbench() override = default;
    const char* className() const override { return "StdCmdWorkbench"; }
protected:
    void activated(int iMsg) override;
    Action* createAction() override;
};

class StdCmdToolBarMenu : public Command {
public:
    StdCmdToolBarMenu();
    ~StdCmdToolBarMenu() override = default;
    const char* className() const override { return "StdCmdToolBarMenu"; }
protected:
    void activated(int iMsg) override;
    Action* createAction() override;
};

class StdCmdDockViewMenu : public Command {
public:
    StdCmdDockViewMenu();
    ~StdCmdDockViewMenu() override = default;
    const char* className() const override { return "StdCmdDockViewMenu"; }
protected:
    void activated(int iMsg) override;
    Action* createAction() override;
};

class StdCmdToggleBottomPanels : public Command {
public:
    StdCmdToggleBottomPanels();
    ~StdCmdToggleBottomPanels() override = default;
    const char* className() const override { return "StdCmdToggleBottomPanels"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

class StdCmdStatusBar : public Command {
public:
    StdCmdStatusBar();
    ~StdCmdStatusBar() override = default;
    const char* className() const override { return "StdCmdStatusBar"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

// =====================================================================
// Misc view commands
// =====================================================================

class StdCmdViewCreate : public Command {
public:
    StdCmdViewCreate();
    ~StdCmdViewCreate() override = default;
    const char* className() const override { return "StdCmdViewCreate"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdMainFullscreen : public Command {
public:
    StdMainFullscreen();
    ~StdMainFullscreen() override = default;
    const char* className() const override { return "StdMainFullscreen"; }
protected:
    void activated(int iMsg) override;
};

class StdViewScreenShot : public Command {
public:
    StdViewScreenShot();
    ~StdViewScreenShot() override = default;
    const char* className() const override { return "StdViewScreenShot"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdViewLoadImage : public Command {
public:
    StdViewLoadImage();
    ~StdViewLoadImage() override = default;
    const char* className() const override { return "StdViewLoadImage"; }
protected:
    void activated(int iMsg) override;
};

class StdCmdClarifySelection : public Command {
public:
    StdCmdClarifySelection();
    ~StdCmdClarifySelection() override = default;
    const char* className() const override { return "StdCmdClarifySelection"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdStoreWorkingView : public Command {
public:
    StdStoreWorkingView();
    ~StdStoreWorkingView() override = default;
    const char* className() const override { return "StdStoreWorkingView"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdRecallWorkingView : public Command {
public:
    StdRecallWorkingView();
    ~StdRecallWorkingView() override = default;
    const char* className() const override { return "StdRecallWorkingView"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdCmdSelBoundingBox : public Command {
public:
    StdCmdSelBoundingBox();
    ~StdCmdSelBoundingBox() override = default;
    const char* className() const override { return "StdCmdSelBoundingBox"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

class StdCmdViewVR : public Command {
public:
    StdCmdViewVR();
    ~StdCmdViewVR() override = default;
    const char* className() const override { return "StdCmdViewVR"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// =====================================================================
// Dock/Undock/Fullscreen sub-commands
// =====================================================================

class StdViewDock : public Command {
public:
    StdViewDock();
    ~StdViewDock() override = default;
    const char* className() const override { return "StdViewDock"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdViewUndock : public Command {
public:
    StdViewUndock();
    ~StdViewUndock() override = default;
    const char* className() const override { return "StdViewUndock"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdViewFullscreen : public Command {
public:
    StdViewFullscreen();
    ~StdViewFullscreen() override = default;
    const char* className() const override { return "StdViewFullscreen"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

class StdViewDockUndockFullscreen : public Command {
public:
    StdViewDockUndockFullscreen();
    ~StdViewDockUndockFullscreen() override = default;
    const char* className() const override { return "StdViewDockUndockFullscreen"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

// =====================================================================
// Tree view actions group
// =====================================================================

class StdCmdTreeViewActions : public Command {
public:
    StdCmdTreeViewActions();
    ~StdCmdTreeViewActions() override = default;
    const char* className() const override { return "StdCmdTreeViewActions"; }
protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Action* createAction() override;
};

// =====================================================================
// Registration helper — called by Task 10 (CreateStdCommands)
// =====================================================================

/// Register all View domain commands with the given manager.
/// Ported from: FreeCAD src/Gui/CommandView.cpp:4251-4333 (CreateViewStdCommands)
void createViewCommands(CommandManager& mgr);

} // namespace Gui
