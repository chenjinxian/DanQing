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

// Application singleton — MainWindow/Action fixture
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

// M-L(2)：RecentFilesActionTest ×3 / RecentMacrosActionTest ×1 随被测对象
// RecentFilesAction/RecentMacrosAction 一并移除——RecentFiles/RecentMacros
// 永久空子菜单（无 appendFile 调用方；宏无 Python 宿主，分析报告 §3.1）。

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

// M-L(2)：WorkbenchGroupTest/WorkbenchComboBoxTest（3 项）随被测对象
// WorkbenchGroup/WorkbenchComboBox 一并移除——WorkbenchSelector 死路径
// （无 workbench 注册、承载它的 FreeCAD 工具栏不构建，分析报告 §3.1）。
// §5(f) Authored 测试随被测件生命周期。
