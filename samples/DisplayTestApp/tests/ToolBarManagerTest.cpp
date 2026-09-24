// Ported from: Authored — no reference unit test exists in FreeCAD for ToolBarManager
// standalone; test scenarios derived from FreeCAD ToolBarManager.cpp/ToolBarManager.h usage patterns.
#include <gtest/gtest.h>

#include <QMainWindow>
#include <QToolBar>

#include "QtTestFixtures.h"
#include "Gui/ToolBarManager.h"
#include "Gui/Command.h"
#include "StubCmd.h"

#include <App/Application.h>
#include <Base/Parameter.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h (ToolBarManager ctor resolves m_hPref from App::GetApplication()
// .GetUserParameter() — FreeCAD ToolBarManager.cpp:427).

using namespace Gui;

// =====================================================================
// ToolBarItem tests
// =====================================================================

// Ported from: Authored — ToolBarItem default state
TEST(ToolBarItemTest, DefaultState)
{
    qtApp();
    ToolBarItem item;
    EXPECT_TRUE(item.command().empty());
    EXPECT_EQ(item.visibility(), ToolBarItem::Visible);
    EXPECT_TRUE(item.getItems().isEmpty());
}

// Ported from: Authored — ToolBarItem with parent is appended
TEST(ToolBarItemTest, AppendsToParent)
{
    qtApp();
    ToolBarItem root;
    auto* child = new ToolBarItem(&root);
    child->setCommand("File");
    EXPECT_EQ(root.getItems().size(), 1);
    EXPECT_EQ(root.getItems()[0]->command(), std::string("File"));
}

// Ported from: Authored — operator<< appends leaf items
TEST(ToolBarItemTest, OperatorAppendLeaf)
{
    qtApp();
    ToolBarItem root;
    root << "CmdA" << "CmdB";
    EXPECT_EQ(root.getItems().size(), 2);
    EXPECT_EQ(root.getItems()[0]->command(), std::string("CmdA"));
    EXPECT_EQ(root.getItems()[1]->command(), std::string("CmdB"));
}

// Ported from: Authored — chained operator<< appends correctly
TEST(ToolBarItemTest, ChainedOperatorAppend)
{
    qtApp();
    ToolBarItem root;
    auto* toolbar = new ToolBarItem(&root);
    toolbar->setCommand("Std_Toolbar");
    *toolbar << "Cmd1" << "Cmd2" << "Cmd3";
    EXPECT_EQ(toolbar->getItems().size(), 3);
    EXPECT_EQ(toolbar->getItems()[0]->command(), std::string("Cmd1"));
    EXPECT_EQ(toolbar->getItems()[2]->command(), std::string("Cmd3"));
}

// =====================================================================
// ToolBarManager::setup tests
// =====================================================================

// Ported from: Authored — builds toolbar from tree with separator
TEST(ToolBarManagerTest, BuildsToolbarFromTree)
{
    ensureAppReady();
    QMainWindow mw;
    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);

    ToolBarItem root;
    auto* file = new ToolBarItem(&root);
    file->setCommand("File");
    *file << "Std_New" << "Std_Open" << "Std_Save";

    tm.setup(&root);

    auto* tb = mw.findChild<QToolBar*>("File");
    EXPECT_NE(tb, nullptr);
    EXPECT_EQ(tb->iconSize().width(), 24);
}

// Ported from: Authored — separator appears in toolbar
TEST(ToolBarManagerTest, SeparatorAppearsInToolbar)
{
    ensureAppReady();
    QMainWindow mw;
    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);

    ToolBarItem root;
    auto* file = new ToolBarItem(&root);
    file->setCommand("File");
    *file << "Std_New" << "Std_Open" << "Separator" << "Std_Save";

    tm.setup(&root);

    auto* tb = mw.findChild<QToolBar*>("File");
    ASSERT_NE(tb, nullptr);

    bool hasSep = false;
    for (auto* a : tb->actions())
        if (a->isSeparator())
            hasSep = true;
    EXPECT_TRUE(hasSep);
}

// Ported from: Authored — registered command's action appears in toolbar
TEST(ToolBarManagerTest, RegisteredCommandAppearsInToolbar)
{
    ensureAppReady();
    QMainWindow mw;
    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);

    auto cmd = std::make_unique<StubCmd>();
    mgr.addCommand(cmd.get());

    ToolBarItem root;
    auto* tbItem = new ToolBarItem(&root);
    tbItem->setCommand("Test");
    *tbItem << "Stub_Cmd";

    tm.setup(&root);

    auto* tb = mw.findChild<QToolBar*>("Test");
    ASSERT_NE(tb, nullptr);

    bool found = false;
    for (auto* a : tb->actions()) {
        if (a->objectName() == QLatin1String("Stub_Cmd")) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// Ported from: Authored — multiple toolbars created
TEST(ToolBarManagerTest, MultipleToolbarsCreated)
{
    ensureAppReady();
    QMainWindow mw;
    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);

    ToolBarItem root;
    auto* file = new ToolBarItem(&root);
    file->setCommand("File");
    *file << "Std_New";

    auto* edit = new ToolBarItem(&root);
    edit->setCommand("Edit");
    *edit << "Std_Undo";

    tm.setup(&root);

    EXPECT_NE(mw.findChild<QToolBar*>("File"), nullptr);
    EXPECT_NE(mw.findChild<QToolBar*>("Edit"), nullptr);
}

// Ported from: Authored — icon size is 24
TEST(ToolBarManagerTest, IconSizeIs24)
{
    ensureAppReady();
    QMainWindow mw;
    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);
    EXPECT_EQ(tm.toolBarIconSize(), 24);
}

// Ported from: Authored — null root is safe
TEST(ToolBarManagerTest, NullRootIsSafe)
{
    ensureAppReady();
    QMainWindow mw;
    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);
    tm.setup(nullptr);  // should not crash
}

// =====================================================================
// ToolBarManager::saveState — persists toolbar visibility to m_hPref
// =====================================================================

// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:853-873 — saveState iterates toolbars and writes
// each toolbar's isVisible() to hPref (skipping toolbars whose toggleViewAction is hidden, per
// FreeCAD's ignoreSave). Verifies the round-trip: set visible=false, saveState, read pref → false;
// set visible=true, saveState, read pref → true.
TEST(ToolBarManagerTest, SaveStatePersistsVisibility)
{
    ensureAppReady();
    QMainWindow mw;
    // show() so toggleViewAction()->isVisible() is true (FreeCAD ignoreSave check).
    mw.show();
    qApp->processEvents();

    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);

    ToolBarItem root;
    auto* file = new ToolBarItem(&root);
    file->setCommand("SaveStateToolbar");
    *file << "Std_New";

    tm.setup(&root);

    auto* tb = mw.findChild<QToolBar*>("SaveStateToolbar");
    ASSERT_NE(tb, nullptr);
    qApp->processEvents();

    auto hPref = App::GetApplication().GetUserParameter()
                     .GetGroup("BaseApp")
                     ->GetGroup("MainWindow")
                     ->GetGroup("Toolbars");

    // Set toolbar hidden, save → pref should be false
    tb->setVisible(false);
    tm.saveState();
    EXPECT_FALSE(hPref->GetBool("SaveStateToolbar", true))
        << "saveState should persist isVisible()==false";

    // Set toolbar visible, save → pref should be true
    tb->setVisible(true);
    tm.saveState();
    EXPECT_TRUE(hPref->GetBool("SaveStateToolbar", false))
        << "saveState should persist isVisible()==true";
}

// =====================================================================
// ToolBarManager::setup — applies persisted visibility override (round-trip)
// =====================================================================

// Ported from: FreeCAD src/Gui/ToolBarManager.cpp:886 — restoreState applies
// hPref->GetBool(toolbarName, defaultVisibility) over the DefaultVisibility. DanQing wires this
// directly into setup() (no separate restoreState()). Seed pref → false, setup → toolbar hidden.
TEST(ToolBarManagerTest, SetupAppliesPersistedVisibility)
{
    ensureAppReady();

    // Seed preference: SaveStateToolbar -> false
    auto hPref = App::GetApplication().GetUserParameter()
                     .GetGroup("BaseApp")
                     ->GetGroup("MainWindow")
                     ->GetGroup("Toolbars");
    hPref->SetBool("RoundTripToolbar", false);

    QMainWindow mw;
    mw.show();
    qApp->processEvents();

    CommandManager mgr;
    ToolBarManager tm(mgr, &mw);

    ToolBarItem root;
    auto* file = new ToolBarItem(&root, ToolBarItem::Visible);  // default visibility = Visible
    file->setCommand("RoundTripToolbar");
    *file << "Std_New";

    tm.setup(&root);

    auto* tb = mw.findChild<QToolBar*>("RoundTripToolbar");
    ASSERT_NE(tb, nullptr);
    // DefaultVisibility is Visible, but seeded pref false overrides → toolbar hidden
    EXPECT_FALSE(tb->isVisible())
        << "setup() should apply hPref override over DefaultVisibility";
}
