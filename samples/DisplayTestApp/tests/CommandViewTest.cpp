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

// Ported from: Authored — no reference unit test exists in FreeCAD for DrawStyle group
TEST(CommandViewTest, DrawStyleCreateActionReturnsGroupWith7Items)
{
    qtApp();
    StdCmdDrawStyle cmd;
    cmd.initAction();
    Action* action = cmd.getAction();
    ASSERT_NE(action, nullptr);

    // Cast to ActionGroup to check children
    auto* group = qobject_cast<ActionGroup*>(action);
    ASSERT_NE(group, nullptr);

    QList<QAction*> items = group->actions();
    // DrawStyle has 7 checkable items (AsIs, Points, Wireframe, HiddenLine, NoShading, Shaded, FlatLines)
    EXPECT_EQ(items.size(), 7);

    // All items should be checkable
    for (QAction* item : items) {
        EXPECT_TRUE(item->isCheckable()) << "Item " << item->objectName().toStdString() << " should be checkable";
    }

    // First item (AsIs) should be checked by default
    EXPECT_TRUE(items[0]->isChecked());
}

// Ported from: Authored — no reference unit test exists in FreeCAD for DrawStyle shortcuts
TEST(CommandViewTest, DrawStyleItemShortcuts)
{
    qtApp();
    StdCmdDrawStyle cmd;
    cmd.initAction();
    Action* action = cmd.getAction();
    ASSERT_NE(action, nullptr);

    auto* group = qobject_cast<ActionGroup*>(action);
    ASSERT_NE(group, nullptr);

    QList<QAction*> items = group->actions();
    ASSERT_EQ(items.size(), 7);

    // Qt formats multi-key shortcuts with a space: "V,1" input → "V, 1" output
    const char* expectedShortcuts[] = {"V, 1", "V, 2", "V, 3", "V, 4", "V, 5", "V, 6", "V, 7"};
    for (int i = 0; i < 7; ++i) {
        EXPECT_STREQ(items[i]->shortcut().toString().toLatin1().constData(), expectedShortcuts[i])
            << "Item " << i << " shortcut mismatch";
    }
}

// Ported from: Authored — no reference unit test exists in FreeCAD for ToggleBottomPanels
TEST(CommandViewTest, ToggleBottomPanelsIsCheckable)
{
    qtApp();
    StdCmdToggleBottomPanels cmd;
    cmd.initAction();
    Action* action = cmd.getAction();
    ASSERT_NE(action, nullptr);
    EXPECT_TRUE(action->action()->isCheckable());
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

// Ported from: Authored — no reference unit test exists in FreeCAD for SelBoundingBox
TEST(CommandViewTest, SelBoundingBoxIsCheckable)
{
    qtApp();
    StdCmdSelBoundingBox cmd;
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
    EXPECT_NE(mgr.getCommandByName("Std_OrthographicCamera"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_PerspectiveCamera"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewGroup"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DrawStyle"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Workbench"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewStatusBar"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ToggleBottomPanels"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewIsometric"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFront"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewTop"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_TreeViewActions"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ToggleVisibility"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_SelBoundingBox"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewDockUndockFullscreen"), nullptr);
}
