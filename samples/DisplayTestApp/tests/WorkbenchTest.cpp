// Ported from: Authored — no reference unit test exists in FreeCAD for Workbench
// standalone; test scenarios derived from FreeCAD Workbench.cpp/Workbench.h usage patterns.
#include <gtest/gtest.h>

#include "QtTestFixtures.h"
#include "Gui/Workbench.h"

using namespace Gui;

// =====================================================================
// StdWorkbench menu tree tests
// =====================================================================

// Ported from: Authored — StdWorkbench menu bar has 7 top-level menus
// (File, Edit, View, Tools, Macro, Windows, Help)
TEST(StdWorkbenchTest, MenuBarHasSevenTopMenus)
{
    qtApp();
    StdWorkbench wb;
    auto* mb = wb.setupMenuBar();
    ASSERT_NE(mb, nullptr);

    // 7 top-level items: File, Edit, View, Tools, Macro, Windows, Help
    EXPECT_EQ(mb->getItems().size(), 7);

    delete mb;
}

// Ported from: Authored — File menu's first item command is "Std_New"
TEST(StdWorkbenchTest, FileMenuFirstItemIsStdNew)
{
    qtApp();
    StdWorkbench wb;
    auto* mb = wb.setupMenuBar();
    ASSERT_NE(mb, nullptr);

    MenuItem* file = mb->getItems().at(0);
    EXPECT_EQ(file->command(), std::string("&File"));
    EXPECT_FALSE(file->getItems().isEmpty());
    EXPECT_EQ(file->getItems().at(0)->command(), std::string("Std_New"));

    delete mb;
}

// Ported from: Authored — View menu contains Standard Views submenu
TEST(StdWorkbenchTest, ViewMenuContainsStandardViewsSubmenu)
{
    qtApp();
    StdWorkbench wb;
    auto* mb = wb.setupMenuBar();
    ASSERT_NE(mb, nullptr);

    MenuItem* view = mb->getItems().at(2);  // third top-level is View
    EXPECT_EQ(view->command(), std::string("&View"));

    // Find the "Standard Views" submenu
    bool foundStdViews = false;
    for (MenuItem* child : view->getItems()) {
        if (child->command() == std::string("Standard &Views")) {
            foundStdViews = true;
            // Standard Views submenu should have Axonometric submenu
            bool foundAxonometric = false;
            for (MenuItem* sv : child->getItems()) {
                if (sv->command() == std::string("A&xonometric")) {
                    foundAxonometric = true;
                    EXPECT_EQ(sv->getItems().size(), 3);  // Isometric, Dimetric, Trimetric
                }
            }
            EXPECT_TRUE(foundAxonometric);
            break;
        }
    }
    EXPECT_TRUE(foundStdViews);

    delete mb;
}

// =====================================================================
// StdWorkbench toolbar tree tests
// =====================================================================

// Ported from: Authored — StdWorkbench has 9 toolbars
// (File, Edit, Clipboard, Workbench, Macro, View, Individual Views, Structure, Help)
TEST(StdWorkbenchTest, ToolBarsCountIsNine)
{
    qtApp();
    StdWorkbench wb;
    auto* tb = wb.setupToolBars();
    ASSERT_NE(tb, nullptr);

    EXPECT_EQ(tb->getItems().size(), 9);

    delete tb;
}

// Ported from: Authored — First toolbar command is "File"
TEST(StdWorkbenchTest, FirstToolBarCommandIsFile)
{
    qtApp();
    StdWorkbench wb;
    auto* tb = wb.setupToolBars();
    ASSERT_NE(tb, nullptr);

    ToolBarItem* file = tb->getItems().at(0);
    EXPECT_EQ(file->command(), std::string("File"));

    delete tb;
}

// Ported from: Authored — Clipboard and Macro toolbars are Hidden by default
TEST(StdWorkbenchTest, ClipboardAndMacroToolbarsAreHidden)
{
    qtApp();
    StdWorkbench wb;
    auto* tb = wb.setupToolBars();
    ASSERT_NE(tb, nullptr);

    // Clipboard is index 2, Macro is index 4
    EXPECT_EQ(tb->getItems().at(2)->visibility(), ToolBarItem::DefaultVisibility::Hidden);
    EXPECT_EQ(tb->getItems().at(4)->visibility(), ToolBarItem::DefaultVisibility::Hidden);

    // Other toolbars are Visible
    EXPECT_EQ(tb->getItems().at(0)->visibility(), ToolBarItem::DefaultVisibility::Visible);
    EXPECT_EQ(tb->getItems().at(1)->visibility(), ToolBarItem::DefaultVisibility::Visible);

    delete tb;
}

// =====================================================================
// Workbench::activate tests
// =====================================================================

// Ported from: Authored — activate returns true with null managers (no crash)
TEST(WorkbenchActivateTest, ActivateWithNullManagers)
{
    qtApp();
    StdWorkbench wb;
    // setManagers not called — all null
    EXPECT_TRUE(wb.activate());
}
