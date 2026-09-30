// Ported from: FreeCAD src/Gui/CommandView.cpp (header for registration + class declarations)
// Declares command classes + createViewCommands() — registers all View domain commands.
// M-L(2) 清理：存根命令族（FitSelection/ViewGroup/Dimetric/Trimetric/Home/Rotate×2/
// Zoom×3/DrawStyle/AxisCross/ToggleClipPlane/TextureMapping/可见性 10 项/
// ToggleNavigation/ToolBarMenu/ToggleBottomPanels/VR/ClarifySelection/ScreenShot/
// LoadImage/SelBoundingBox/StoreWorkingView/RecallWorkingView/TreeViewActions/
// Dock·Undock·Fullscreen 族）的类声明随实现一并移除（分析报告 §3.1/§3.5）。
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

// =====================================================================
// Dock, Status bar
// =====================================================================

class StdCmdDockViewMenu : public Command {
public:
    StdCmdDockViewMenu();
    ~StdCmdDockViewMenu() override = default;
    const char* className() const override { return "StdCmdDockViewMenu"; }
protected:
    void activated(int iMsg) override;
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

// =====================================================================
// Registration helper — called by Task 10 (CreateStdCommands)
// =====================================================================

/// Register all View domain commands with the given manager.
/// Ported from: FreeCAD src/Gui/CommandView.cpp:4251-4333 (CreateViewStdCommands)
void createViewCommands(CommandManager& mgr);

} // namespace Gui
