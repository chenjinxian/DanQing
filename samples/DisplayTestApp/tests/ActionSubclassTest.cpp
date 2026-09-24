// Ported from: Authored — no reference unit test exists in FreeCAD for Action subclasses
// standalone; test scenarios derived from FreeCAD Action.cpp/Action.h usage patterns.
#include <gtest/gtest.h>

#include <QApplication>
#include <QMenu>
#include <QToolBar>
#include <QWidget>

#include "QtTestFixtures.h"

// Qt application fixture — singleton definition (shared across test TUs)
static int   g_argc2 = 1;
static char  g_arg02[] = "test";
static char* g_argv2[] = { g_arg02, nullptr };

// Application singleton — needed by RecentFilesAction/RecentMacrosAction
#include <App/Application.h>
App::Application* App::Application::_pcSingleton = nullptr;
std::map<std::string, std::string> App::Application::m_config;

// MainWindow stubs removed — real MainWindow.cpp now in test target.
// The App::Application singleton definitions above are still needed.

// Fixture that ensures both QApplication and App::Application exist
struct QtAppWithFullInit {
    bool ownsApp = false;
    QtAppWithFullInit() {
        qtApp();  // ensure QApplication singleton exists
        if (!App::Application::_pcSingleton) {
            App::Application::_pcSingleton = new App::Application();
            ownsApp = true;
        }
    }
    ~QtAppWithFullInit() {
        if (ownsApp) {
            delete App::Application::_pcSingleton;
            App::Application::_pcSingleton = nullptr;
        }
    }
};
static QtAppWithFullInit& qtApp2() { static QtAppWithFullInit i; return i; }

#include "Gui/Command.h"
#include "Gui/Action.h"
#include "Gui/WorkbenchSelector.h"
#include "StubCmd.h"

using namespace Gui;

// =====================================================================
// ActionGroup tests
// =====================================================================

// Ported from: Authored — ActionGroup manages a group of child actions
TEST(ActionGroupTest, ManagesChildActions)
{
    qtApp2();
    auto* cmd = new StubCmd();
    ActionGroup* group = new ActionGroup(cmd);

    // Initially empty
    EXPECT_EQ(group->actions().count(), 0);

    // Add actions
    QAction* a1 = group->addAction("Action 1");
    QAction* a2 = group->addAction("Action 2");
    EXPECT_EQ(group->actions().count(), 2);
    EXPECT_TRUE(group->actions().contains(a1));
    EXPECT_TRUE(group->actions().contains(a2));

    delete group;
    delete cmd;
}

// Ported from: Authored — ActionGroup addTo adds all actions to widget
TEST(ActionGroupTest, AddToWidgetAddsAllActions)
{
    qtApp2();
    auto* cmd = new StubCmd();
    ActionGroup* group = new ActionGroup(cmd);
    group->addAction("Action 1");
    group->addAction("Action 2");

    QWidget widget;
    group->addTo(&widget);

    QList<QAction*> widgetActions = widget.actions();
    EXPECT_GE(widgetActions.count(), 2);

    delete group;
    delete cmd;
}

// Ported from: Authored — ActionGroup exclusive mode
TEST(ActionGroupTest, ExclusiveMode)
{
    qtApp2();
    auto* cmd = new StubCmd();
    ActionGroup* group = new ActionGroup(cmd);

    group->setExclusive(true);
    EXPECT_TRUE(group->isExclusive());

    group->setExclusive(false);
    EXPECT_FALSE(group->isExclusive());

    delete group;
    delete cmd;
}

// =====================================================================
// RecentFilesAction tests
// =====================================================================

// Ported from: Authored — RecentFilesAction creates submenu with Clear action
TEST(RecentFilesActionTest, CreatesSubmenuWithClearAction)
{
    qtApp2();
    auto* cmd = new StubCmd();
    RecentFilesAction* rfa = new RecentFilesAction(cmd);

    // The action group should have actions (placeholders + separator + clear)
    QList<QAction*> actions = rfa->actions();
    EXPECT_GT(actions.count(), 0);

    // Last action should be "Clear Recent Files"
    QAction* lastAction = actions.last();
    EXPECT_EQ(lastAction->text(), QStringLiteral("Clear Recent Files"));

    delete rfa;
    delete cmd;
}

// Ported from: Authored — RecentFilesAction empty state has no visible file entries
TEST(RecentFilesActionTest, EmptyStateNoVisibleFiles)
{
    qtApp2();
    auto* cmd = new StubCmd();
    RecentFilesAction* rfa = new RecentFilesAction(cmd);

    // files() returns empty list when no MRU entries exist
    QStringList fileList;
    // files() is private, but we can check visible actions
    QList<QAction*> actions = rfa->actions();
    int visibleCount = 0;
    for (QAction* a : actions) {
        if (a->isVisible() && !a->isSeparator() &&
            a->text() != QStringLiteral("Clear Recent Files")) {
            visibleCount++;
        }
    }
    EXPECT_EQ(visibleCount, 0);

    delete rfa;
    delete cmd;
}

// Ported from: Authored — RecentFilesAction appendFile adds to list
TEST(RecentFilesActionTest, AppendFileAddsEntry)
{
    qtApp2();
    auto* cmd = new StubCmd();
    RecentFilesAction* rfa = new RecentFilesAction(cmd);

    rfa->appendFile(QStringLiteral("/tmp/test_file.fcstd"));

    // After append, there should be at least one visible action with the file
    QList<QAction*> actions = rfa->actions();
    bool found = false;
    for (QAction* a : actions) {
        if (a->isVisible() && a->toolTip() == QStringLiteral("/tmp/test_file.fcstd")) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    delete rfa;
    delete cmd;
}

// =====================================================================
// RecentMacrosAction tests
// =====================================================================

// Ported from: Authored — RecentMacrosAction creates with default 12 visible items
TEST(RecentMacrosActionTest, CreatesWithDefaultVisibleItems)
{
    qtApp2();
    auto* cmd = new StubCmd();
    RecentMacrosAction* rma = new RecentMacrosAction(cmd);

    // Should have actions (placeholders for macros)
    QList<QAction*> actions = rma->actions();
    EXPECT_GT(actions.count(), 0);

    delete rma;
    delete cmd;
}

// =====================================================================
// WindowAction tests
// =====================================================================

// Ported from: Authored — WindowAction has 10 placeholder actions
TEST(WindowActionTest, HasTenPlaceholderActions)
{
    qtApp2();
    auto* cmd = new StubCmd();
    WindowAction* wa = new WindowAction(cmd);

    QList<QAction*> actions = wa->actions();
    // 10 placeholders + 1 separator = 11
    EXPECT_EQ(actions.count(), 11);

    // First 10 should be checkable (the window placeholders)
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(actions[i]->isCheckable());
    }

    // Last action should be a separator
    EXPECT_TRUE(actions.last()->isSeparator());

    delete wa;
    delete cmd;
}

// Ported from: Authored — WindowAction addTo adds child actions to menu
TEST(WindowActionTest, AddToMenuAddsChildActions)
{
    qtApp2();
    auto* cmd = new StubCmd();
    WindowAction* wa = new WindowAction(cmd);

    QMenu menu;
    wa->addTo(&menu);

    // ActionGroup::addTo adds the group's child actions (not the parent action) to the widget
    QList<QAction*> menuActions = menu.actions();
    QList<QAction*> groupActions = wa->actions();
    for (QAction* ga : groupActions) {
        EXPECT_TRUE(menuActions.contains(ga));
    }

    delete wa;
    delete cmd;
}

// =====================================================================
// WorkbenchGroup tests
// =====================================================================

// Ported from: Authored — WorkbenchGroup creates with StdWorkbench action
TEST(WorkbenchGroupTest, CreatesWithStdWorkbenchAction)
{
    qtApp2();
    auto* cmd = new StubCmd();
    WorkbenchGroup* wbg = new WorkbenchGroup(cmd);

    QList<QAction*> actions = wbg->actions();
    EXPECT_EQ(actions.count(), 1);
    EXPECT_EQ(actions[0]->text(), QStringLiteral("StdWorkbench"));
    EXPECT_TRUE(actions[0]->isChecked());

    delete wbg;
    delete cmd;
}

// =====================================================================
// WorkbenchComboBox tests
// =====================================================================

// Ported from: Authored — WorkbenchComboBox can be created and populated
TEST(WorkbenchComboBoxTest, CanBeCreatedAndPopulated)
{
    qtApp2();
    auto* cmd = new StubCmd();
    WorkbenchGroup* wbg = new WorkbenchGroup(cmd);

    QWidget parent;
    WorkbenchComboBox* combo = new WorkbenchComboBox(wbg, &parent);

    EXPECT_EQ(combo->count(), 1);
    EXPECT_EQ(combo->itemText(0), QStringLiteral("StdWorkbench"));
    EXPECT_EQ(combo->iconSize(), QSize(16, 16));

    delete combo;
    delete wbg;
    delete cmd;
}

// Ported from: Authored — WorkbenchComboBox refreshList updates entries
TEST(WorkbenchComboBoxTest, RefreshListUpdatesEntries)
{
    qtApp2();
    auto* cmd = new StubCmd();
    WorkbenchGroup* wbg = new WorkbenchGroup(cmd);

    QWidget parent;
    WorkbenchComboBox* combo = new WorkbenchComboBox(wbg, &parent);
    EXPECT_EQ(combo->count(), 1);

    // Simulate refresh with empty list
    combo->refreshList({});
    EXPECT_EQ(combo->count(), 0);

    // Simulate refresh with original actions
    combo->refreshList(wbg->actions());
    EXPECT_EQ(combo->count(), 1);

    delete combo;
    delete wbg;
    delete cmd;
}
