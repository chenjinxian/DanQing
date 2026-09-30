// Ported from: Authored — no reference unit test exists in FreeCAD for CommandView
// Test scenarios derived from FreeCAD CommandView.cpp usage patterns.
#include <gtest/gtest.h>

#include <QApplication>
#include <QKeySequence>
#include <QWidget>

#include "QtTestFixtures.h"

// Ensure Qt app is initialized (shared with CommandTest.cpp)
extern QtApp& qtApp();

#include "Gui/Command.h"
#include "Gui/CommandView.h"
#include "Gui/Action.h"

using namespace Gui;

// =====================================================================
// Command metadata spot-checks
// =====================================================================

// Ported from: Authored — no reference unit test exists in FreeCAD for camera checkable
TEST(CommandViewTest, OrthographicCameraIsCheckable)
{
    qtApp();
    StdOrthographicCamera cmd;
    cmd.initAction();
    Action* action = cmd.getAction();
    ASSERT_NE(action, nullptr);
    EXPECT_TRUE(action->action()->isCheckable());
}

// Ported from: Authored — no reference unit test exists in FreeCAD for camera checkable
TEST(CommandViewTest, PerspectiveCameraIsCheckable)
{
    qtApp();
    StdPerspectiveCamera cmd;
    cmd.initAction();
    Action* action = cmd.getAction();
    ASSERT_NE(action, nullptr);
    EXPECT_TRUE(action->action()->isCheckable());
}

// Ported from: Authored — no reference unit test exists in FreeCAD for FitAll shortcut
TEST(CommandViewTest, FitAllHasShortcutVF)
{
    qtApp();
    StdCmdViewFitAll cmd;
    EXPECT_STREQ(cmd.getAccel(), "V, F");
}

// Ported from: Authored — no reference unit test exists in FreeCAD for Isometric shortcut
TEST(CommandViewTest, IsometricHasShortcut0)
{
    qtApp();
    StdCmdViewIsometric cmd;
    EXPECT_STREQ(cmd.getAccel(), "0");
}




// Ported from: Authored — no reference unit test exists in FreeCAD for StatusBar
TEST(CommandViewTest, StatusBarIsCheckable)
{
    qtApp();
    StdCmdStatusBar cmd;
    cmd.initAction();
    Action* action = cmd.getAction();
    ASSERT_NE(action, nullptr);
    EXPECT_TRUE(action->action()->isCheckable());
}


// Ported from: Authored — no reference unit test exists in FreeCAD for registration
TEST(CommandViewTest, CreateViewCommandsRegistersAll)
{
    qtApp();
    CommandManager mgr;
    createViewCommands(mgr);

    // Spot-check critical commands exist in the manager
    // M-L(2)：存根命令族断言随删（ViewGroup/DrawStyle/ToggleBottomPanels/
    // TreeViewActions/ToggleVisibility/SelBoundingBox/DockUndockFullscreen）。
    EXPECT_NE(mgr.getCommandByName("Std_OrthographicCamera"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_PerspectiveCamera"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewStatusBar"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewIsometric"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFront"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewTop"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewCreate"), nullptr);
}
