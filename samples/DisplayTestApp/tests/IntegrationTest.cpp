// Ported from: Authored — no reference integration test exists in FreeCAD
// for Command system wiring to MainWindow startup.
// Verifies that createStdCommands + StdWorkbench::activate builds
// the full menu bar (7 top-level menus) and toolbars from command trees.
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
// Integration: StdWorkbench::activate builds 7 top-level menus
// =====================================================================

// Ported from: Authored — activate builds all 7 FreeCAD top-level menus
// (File, Edit, View, Tools, Macro, Windows, Help) from command trees
TEST(IntegrationTest, ActivateBuildsSevenTopMenus)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    wb.setManagers(&mgr, &mm, &tm);
    wb.activate();

    EXPECT_EQ(mw.menuBar()->actions().size(), 7);
}

// =====================================================================
// Integration: StdWorkbench::activate creates visible toolbars
// =====================================================================

// Ported from: Authored — activate creates File toolbar
TEST(IntegrationTest, ActivateCreatesFileToolbar)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    wb.setManagers(&mgr, &mm, &tm);
    wb.activate();

    auto* fileTb = mw.findChild<QToolBar*>(QStringLiteral("File"));
    ASSERT_NE(fileTb, nullptr);
    EXPECT_FALSE(fileTb->actions().isEmpty());
}

// Ported from: Authored — activate creates Edit toolbar
TEST(IntegrationTest, ActivateCreatesEditToolbar)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    wb.setManagers(&mgr, &mm, &tm);
    wb.activate();

    auto* editTb = mw.findChild<QToolBar*>(QStringLiteral("Edit"));
    ASSERT_NE(editTb, nullptr);
    EXPECT_FALSE(editTb->actions().isEmpty());
}

// Ported from: Authored — activate creates View toolbar
TEST(IntegrationTest, ActivateCreatesViewToolbar)
{
    ensureAppReady();
    QMainWindow mw;
    Gui::CommandManager mgr;
    Gui::createStdCommands(mgr);
    Gui::MenuManager mm(mgr);
    Gui::ToolBarManager tm(mgr, &mw);

    Gui::StdWorkbench wb;
    wb.setMainWindow(&mw);
    wb.setManagers(&mgr, &mm, &tm);
    wb.activate();

    auto* viewTb = mw.findChild<QToolBar*>(QStringLiteral("View"));
    ASSERT_NE(viewTb, nullptr);
    EXPECT_FALSE(viewTb->actions().isEmpty());
}

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
    wb.setManagers(&mgr, &mm, &tm);
    wb.activate();

    auto actions = mw.menuBar()->actions();
    ASSERT_EQ(actions.size(), 7);

    // FreeCAD StdWorkbench menu titles (Workbench.cpp:705-841)
    EXPECT_EQ(actions[0]->text().toStdString(), std::string("&File"));
    EXPECT_EQ(actions[1]->text().toStdString(), std::string("&Edit"));
    EXPECT_EQ(actions[2]->text().toStdString(), std::string("&View"));
    EXPECT_EQ(actions[3]->text().toStdString(), std::string("&Tools"));
    EXPECT_EQ(actions[4]->text().toStdString(), std::string("&Macro"));
    EXPECT_EQ(actions[5]->text().toStdString(), std::string("&Windows"));
    EXPECT_EQ(actions[6]->text().toStdString(), std::string("&Help"));
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
    wb.setManagers(&mgr, &mm, &tm);
    wb.activate();

    auto actions = mw.menuBar()->actions();
    ASSERT_GE(actions.size(), 1);

    QMenu* fileMenu = actions[0]->menu();
    ASSERT_NE(fileMenu, nullptr);

    // File menu from StdWorkbench::setupMenuBar has many items (Std_New through Std_Quit)
    // Including separators: ~19 commands + ~5 separators = ~24 actions
    EXPECT_GT(fileMenu->actions().size(), 15u);
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

    // Spot-check key commands exist
    EXPECT_NE(mgr.getCommandByName("Std_New"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Open"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Save"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Quit"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Undo"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Redo"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ViewFront"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Measure"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_About"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Workbench"), nullptr);

    // Registration count should be > 100 (FreeCAD has ~127 standard commands)
    EXPECT_GT(mgr.getAllCommands().size(), 100u);
}
