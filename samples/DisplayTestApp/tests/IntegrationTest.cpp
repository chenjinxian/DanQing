// Ported from: Authored — no reference integration test exists in FreeCAD
// for Command system wiring to MainWindow startup.
// Verifies that createStdCommands + StdWorkbench::activate builds
// the menu bar (4 top-level menus — M-L(2) 后) and toolbars from command trees.
#include <gtest/gtest.h>

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>

#include "QtTestFixtures.h"
#include "Gui/Command.h"
#include "Gui/CreateStdCommands.h"
#include "Gui/MenuManager.h"
#include "Gui/ToolBarManager.h"
#include "Gui/Workbench.h"

#include <App/Application.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h (ToolBarManager ctor resolves m_hPref from App::GetApplication()
// .GetUserParameter() — FreeCAD ToolBarManager.cpp:427).

// =====================================================================
// Integration: StdWorkbench::activate builds 4 top-level menus
// =====================================================================

// Ported from: Authored — activate builds the surviving top-level menus
// (File, View, Tools, Windows) from command trees. M-L(2)：Edit/Macro/Help
// 菜单随其全存根命令族删除。
TEST(IntegrationTest, ActivateBuildsFourTopMenus)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    // M-L(2)：setManagers 第三参（ToolBarManager）已随 setupToolBars 死树删除。
    wb.setManagers(&mgr, &mm);
    wb.activate();

    EXPECT_EQ(mw.menuBar()->actions().size(), 4);
}

// =====================================================================
// Integration: StdWorkbench::activate creates visible toolbars
// =====================================================================




// =====================================================================
// Integration: menu names match expected labels
// =====================================================================

// Ported from: Authored — verify menu titles match FreeCAD labels
TEST(IntegrationTest, MenuTitlesMatchFreeCADLabels)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    // M-L(2)：setManagers 第三参（ToolBarManager）已随 setupToolBars 死树删除。
    wb.setManagers(&mgr, &mm);
    wb.activate();

    auto actions = mw.menuBar()->actions();
    ASSERT_EQ(actions.size(), 4);

    // Surviving menu titles — M-L(2) trim of the FreeCAD StdWorkbench tree
    // (Workbench.cpp:705-841 → File/View/Tools/Windows)
    EXPECT_EQ(actions[0]->text().toStdString(), std::string("&File"));
    EXPECT_EQ(actions[1]->text().toStdString(), std::string("&View"));
    EXPECT_EQ(actions[2]->text().toStdString(), std::string("&Tools"));
    EXPECT_EQ(actions[3]->text().toStdString(), std::string("&Windows"));
}

// =====================================================================
// Integration: File menu has correct command count
// =====================================================================

// Ported from: Authored — File menu has ~19 commands from StdWorkbench tree
TEST(IntegrationTest, FileMenuHasExpectedCommandCount)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    // M-L(2)：setManagers 第三参（ToolBarManager）已随 setupToolBars 死树删除。
    wb.setManagers(&mgr, &mm);
    wb.activate();

    auto actions = mw.menuBar()->actions();
    ASSERT_GE(actions.size(), 1);

    QMenu* fileMenu = actions[0]->menu();
    ASSERT_NE(fileMenu, nullptr);

    // File menu from StdWorkbench::setupMenuBar — M-L(2) trim:
    // Std_New, Separator, Std_Import, Separator, Std_Quit = 5 actions
    EXPECT_EQ(fileMenu->actions().size(), 5u);
}

// =====================================================================
// Integration: Std commands are registered after createStdCommands
// =====================================================================

// Ported from: Authored — createStdCommands registers all expected commands
TEST(IntegrationTest, CreateStdCommandsRegistersExpectedCommands)
{
    qtApp();
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);

    // Spot-check key commands exist — M-L(2) trim to the surviving domains
    EXPECT_NE(mgr.getCommandByName("Std_New"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Import"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Quit"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFront"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_AboutQt"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_TileWindows"), nullptr);

    // Registration count: File 3 + View 14 + Std 1 + Window 6 = 24
    EXPECT_EQ(mgr.getAllCommands().size(), 24u);
}
