// Ported from: Authored — no reference unit test exists in FreeCAD for MenuManager
// standalone; test scenarios derived from FreeCAD MenuManager.cpp/MenuManager.h usage patterns.
#include <gtest/gtest.h>

#include <memory>
#include <QMenu>
#include <QMenuBar>

#include "QtTestFixtures.h"
#include "Gui/Command.h"
#include "Gui/MenuManager.h"
#include "StubCmd.h"

using namespace Gui;

// =====================================================================
// MenuItem tests
// =====================================================================

// Ported from: Authored — MenuItem default state
TEST(MenuItemTest, DefaultState)
{
    qtApp();
    MenuItem item;
    EXPECT_TRUE(item.command().empty());
    EXPECT_FALSE(item.hasItems());
    EXPECT_TRUE(item.getItems().isEmpty());
}

// Ported from: Authored — MenuItem with command
TEST(MenuItemTest, CommandIsStored)
{
    qtApp();
    MenuItem item("MyCmd");
    EXPECT_EQ(item.command(), std::string("MyCmd"));
}

// Ported from: Authored — operator<< appends leaf items
TEST(MenuItemTest, OperatorAppendLeaf)
{
    qtApp();
    MenuItem parent;
    parent << "CmdA" << "CmdB";
    EXPECT_TRUE(parent.hasItems());
    EXPECT_EQ(parent.getItems().size(), 2);
    EXPECT_EQ(parent.getItems()[0]->command(), std::string("CmdA"));
    EXPECT_EQ(parent.getItems()[1]->command(), std::string("CmdB"));
}

// Ported from: Authored — operator<< appends submenu pointer
TEST(MenuItemTest, OperatorAppendSubmenu)
{
    qtApp();
    MenuItem parent;
    auto* sub = new MenuItem("Submenu");
    parent << sub;
    EXPECT_TRUE(parent.hasItems());
    EXPECT_EQ(parent.getItems().size(), 1);
    EXPECT_EQ(parent.getItems()[0], sub);
}

// =====================================================================
// MenuManager::setup tests
// =====================================================================

// Ported from: Authored — builds menu from tree with separator
TEST(MenuManagerTest, BuildsMenuFromTree)
{
    qtApp();
    CommandManager mgr;
    MenuManager mm(mgr);

    MenuItem file;
    file.setCommand("&File");
    file << "Std_New" << "Std_Open" << "Separator" << "Std_Quit";

    QMenu menu;
    mm.setup(&file, &menu);

    // Separator should be present even if Std_* commands are not registered
    bool hasSep = false;
    for (auto* a : menu.actions())
        if (a->isSeparator())
            hasSep = true;
    EXPECT_TRUE(hasSep);
}

// Ported from: Authored — registered command's action appears in menu
TEST(MenuManagerTest, RegisteredCommandAppearsInMenu)
{
    qtApp();
    CommandManager mgr;
    auto cmd = std::make_unique<StubCmd>();
    mgr.addCommand(cmd.get());

    MenuManager mm(mgr);

    MenuItem file;
    file.setCommand("&File");
    file << "Stub_Cmd" << "Separator";

    QMenu menu;
    mm.setup(&file, &menu);

    // Stub_Cmd should have its action added to the menu
    bool found = false;
    for (auto* a : menu.actions()) {
        if (a->objectName() == QLatin1String("Stub_Cmd")) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    // Separator should also be present
    bool hasSep = false;
    for (auto* a : menu.actions())
        if (a->isSeparator())
            hasSep = true;
    EXPECT_TRUE(hasSep);
}

// Ported from: Authored — unregistered command produces no action in menu
TEST(MenuManagerTest, UnregisteredCommandProducesNoAction)
{
    qtApp();
    CommandManager mgr;
    MenuManager mm(mgr);

    MenuItem file;
    file.setCommand("&File");
    file << "NonExistent";

    QMenu menu;
    mm.setup(&file, &menu);

    // No actions should be added (addTo returns false, action not created)
    EXPECT_TRUE(menu.actions().isEmpty());
}

// Ported from: Authored — submenu with children builds nested menu
TEST(MenuManagerTest, SubmenuWithChildrenBuildsNestedMenu)
{
    qtApp();
    CommandManager mgr;
    MenuManager mm(mgr);

    MenuItem file;
    file.setCommand("&File");

    auto* recent = new MenuItem("Recent Files");
    *recent << "Std_OpenRecent1" << "Std_OpenRecent2";
    file << recent;

    QMenu menu;
    mm.setup(&file, &menu);

    // The menu should contain one action: the "Recent Files" submenu
    ASSERT_EQ(menu.actions().size(), 1);
    QAction* subAction = menu.actions().first();
    QMenu* subMenu = subAction->menu();
    ASSERT_NE(subMenu, nullptr);

    // The submenu title should be "Recent Files"
    EXPECT_EQ(subMenu->title().toStdString(), std::string("Recent Files"));

    // The submenu should have 2 actions for the unregistered commands
    // (addTo returns false for unregistered, so no actions added)
    EXPECT_TRUE(subMenu->actions().isEmpty());
}
