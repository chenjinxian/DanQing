// Ported from: Authored — no reference unit test exists in FreeCAD for Workbench
// standalone; test scenarios derived from FreeCAD Workbench.cpp/Workbench.h usage patterns.
#include <gtest/gtest.h>

#include "QtTestFixtures.h"
#include "Gui/Workbench.h"

using namespace Gui;

// =====================================================================
// StdWorkbench menu tree tests
// =====================================================================

// Ported from: Authored — StdWorkbench menu bar has 3 top-level menus
// (File, View, Windows) after the M-L(2) trim (Edit/Macro/Help menus
// were all-stub/FreeCAD-ecosystem and are gone with their commands) and the
// 2026-10-07 deletion pass (Tools menu removed with Std_AboutQt — DTA has no
// About dialog).
TEST(StdWorkbenchTest, MenuBarHasThreeTopMenus)
{
    qtApp();
    StdWorkbench wb;
    auto* mb = wb.setupMenuBar();
    ASSERT_NE(mb, nullptr);

    // 3 top-level items: File, View, Windows（Tools 已删——仅为 Std_AboutQt 存在）
    EXPECT_EQ(mb->getItems().size(), 3);
    EXPECT_EQ(mb->getItems().at(0)->command(), std::string("&File"));
    EXPECT_EQ(mb->getItems().at(1)->command(), std::string("&View"));
    EXPECT_EQ(mb->getItems().at(2)->command(), std::string("&Windows"));

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

    MenuItem* view = mb->getItems().at(1);  // second top-level is View (M-L(2)：4 菜单)
    EXPECT_EQ(view->command(), std::string("&View"));

    // Find the "Standard Views" submenu
    bool foundStdViews = false;
    for (MenuItem* child : view->getItems()) {
        if (child->command() == std::string("Standard &Views")) {
            foundStdViews = true;
            // M-L(2)：Axonometric 子菜单随 Dimetric/Trimetric 存根删除——
            // FitAll + Separator + 8 向平铺（Iso/Front/Top/Right/Rear/Bottom/Left）。
            EXPECT_EQ(child->getItems().size(), 9);
            EXPECT_EQ(child->getItems().at(0)->command(), std::string("Std_ViewFitAll"));
            EXPECT_EQ(child->getItems().at(2)->command(), std::string("Std_ViewIsometric"));
            EXPECT_EQ(child->getItems().at(8)->command(), std::string("Std_ViewLeft"));
            break;
        }
    }
    EXPECT_TRUE(foundStdViews);

    delete mb;
}

// M-L(2)：StdWorkbenchTest 的 3 个 toolbar 树测试（ToolBarsCountIsNine/
// FirstToolBarCommandIsFile/ClipboardAndMacroToolbarsAreHidden）随被测对象
// StdWorkbench::setupToolBars 一并移除（死树——main.cpp 传 nullptr 永不构建；
// §5(f) Authored 测试随被测件生命周期）。

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
