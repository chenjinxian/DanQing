// Ported from: Authored — no reference test exists in FreeCAD for status bar registry
// Tests for MainWindow status bar item registration, context menu, customEvent,
// and messageChanged wiring.
#include <gtest/gtest.h>

#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

#include "QtTestFixtures.h"
#include "Gui/MainWindow.h"
#include "Gui/MainWindow_p.h"
#include "Gui/StatusBarLabel.h"
#include "Gui/InputHintWidget.h"

// Application singleton — needed by MainWindow ctor (App::GetApplication())
#include <App/Application.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h. MainWindow ctor uses App::GetApplication() (FreeCAD MainWindow.cpp:84).

using namespace Gui;

// =====================================================================
// Construction: status bar items are registered in the registry
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for status bar registry
TEST(StatusBarTest, WidgetsRegisteredInRegistry)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* sb = mw.statusBar();
    ASSERT_NE(sb, nullptr);

    // Verify each widget is placed in the status bar by object name
    auto* actionLabel = sb->findChild<QLabel*>(QStringLiteral("Preselection"));
    ASSERT_NE(actionLabel, nullptr);
    EXPECT_EQ(actionLabel->windowTitle().toStdString(), std::string("Preselection"));

    auto* hintLabel = sb->findChild<QWidget*>(QStringLiteral("InputHints"));
    ASSERT_NE(hintLabel, nullptr);

    auto* sizeLabel = sb->findChild<QWidget*>(QStringLiteral("UnitSystem"));
    ASSERT_NE(sizeLabel, nullptr);

    auto* rightLabel = sb->findChild<QLabel*>(QStringLiteral("QuickMeasure"));
    ASSERT_NE(rightLabel, nullptr);
    EXPECT_EQ(rightLabel->windowTitle().toStdString(), std::string("Quick Measure"));

    // New widgets from Task 4
    auto* progressBar = sb->findChild<QProgressBar*>(QStringLiteral("progressBar"));
    ASSERT_NE(progressBar, nullptr);
    EXPECT_EQ(progressBar->maximumWidth(), 200);

    auto* toggleBtn = sb->findChild<QToolButton*>(QStringLiteral("toggleBottomPanelsButton"));
    ASSERT_NE(toggleBtn, nullptr);
    EXPECT_TRUE(toggleBtn->isCheckable());
    EXPECT_TRUE(toggleBtn->isChecked());

    auto* notifyWidget = sb->findChild<QWidget*>(QStringLiteral("Notifications"));
    ASSERT_NE(notifyWidget, nullptr);
}

// =====================================================================
// Context menu: buildStatusBarContextMenu populates toggle actions
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for status bar context menu
TEST(StatusBarTest, ContextMenuHasToggleActions)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    QMenu menu(&mw);
    mw.buildStatusBarContextMenu(menu);

    // 7 registered widgets, but progressBar has no title → 6 toggle actions
    // (progress bars don't appear in the context menu, matching FreeCAD behavior)
    auto actions = menu.actions();
    EXPECT_EQ(actions.size(), 6u);

    // Each action should be checkable and checked by default
    for (auto* action : actions) {
        EXPECT_TRUE(action->isCheckable());
        EXPECT_TRUE(action->isChecked());
    }

    // Verify expected titles
    QStringList titles;
    for (auto* action : actions) {
        titles.append(action->text());
    }
    EXPECT_TRUE(titles.contains(QStringLiteral("Preselection")));
    EXPECT_TRUE(titles.contains(QStringLiteral("Input Hints")));
    EXPECT_TRUE(titles.contains(QStringLiteral("Unit System")));
    EXPECT_TRUE(titles.contains(QStringLiteral("Quick Measure")));
    EXPECT_TRUE(titles.contains(QStringLiteral("Bottom Panel Toggle")));
    EXPECT_TRUE(titles.contains(QStringLiteral("Notifications")));
}

// =====================================================================
// customEvent: dispatches Tmp message to actionLabel
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for customEvent dispatch
TEST(StatusBarTest, CustomEventDispatchesTmpMessage)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    // Post a Tmp-type custom event and verify the label text changes
    auto* event = new CustomMessageEvent(MainWindow::Tmp, QStringLiteral("EvtMsg"), 0);
    QApplication::postEvent(&mw, event);
    qApp->processEvents();

    // showMessage sets d->actionLabel text; verify via the label's text property
    // Use findChildren<QLabel*> to handle any internal reparenting
    auto labels = mw.statusBar()->findChildren<QLabel*>();
    bool found = false;
    for (auto* label : labels) {
        if (label->text().contains(QStringLiteral("EvtMsg"))) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "customEvent should dispatch Tmp message to showMessage";
}

// =====================================================================
// customEvent: subclass verifies dispatch is called
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for customEvent dispatch verification
namespace {
class TestMainWindow : public Gui::MainWindow {
public:
    bool customEventCalled = false;
    int lastEventType = -1;
    using Gui::MainWindow::MainWindow;
protected:
    void customEvent(QEvent* e) override {
        customEventCalled = true;
        lastEventType = e->type();
        Gui::MainWindow::customEvent(e);
    }
};
}

TEST(StatusBarTest, CustomEventIsCalledOnPost)
{
    ensureAppReady();
    TestMainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* event = new CustomMessageEvent(MainWindow::Tmp, QStringLiteral("TestDispatch"), 0);
    QApplication::postEvent(&mw, event);
    qApp->processEvents();

    EXPECT_TRUE(mw.customEventCalled) << "customEvent should be called when event is posted";
    EXPECT_EQ(mw.lastEventType, static_cast<int>(QEvent::User));
}

// =====================================================================
// customEvent: dispatches Pane message to statusBar
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for customEvent Pane dispatch
TEST(StatusBarTest, CustomEventDispatchesPaneMessage)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    // Post a Pane-type custom event
    auto* event = new CustomMessageEvent(MainWindow::Pane, QStringLiteral("Pane text"), 0);
    QApplication::postEvent(&mw, event);
    qApp->processEvents();

    // StatusBar should show the Pane message
    auto* sb = mw.statusBar();
    EXPECT_FALSE(sb->currentMessage().isEmpty());
}

// =====================================================================
// messageChanged: clears status on empty message
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for messageChanged
TEST(StatusBarTest, MessageChangedClearsStatus)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    // Show a message then clear it
    mw.showMessage(QStringLiteral("Hello"), 0);
    qApp->processEvents();
    EXPECT_FALSE(mw.statusBar()->currentMessage().isEmpty());

    // Clear the message — statusMessageChanged should handle cleanup
    mw.statusBar()->clearMessage();
    qApp->processEvents();
    EXPECT_TRUE(mw.statusBar()->currentMessage().isEmpty());
}

// =====================================================================
// addStatusBarItem: single widget registration
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for addStatusBarItem
TEST(StatusBarTest, AddStatusBarItemRegistersWidget)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* label = new QLabel(QStringLiteral("Custom widget"), &mw);
    mw.addStatusBarItem(label, StatusBarItemSpec("CustomId", "Custom Widget", StatusBarSlot::Right, 500, true, 0));
    qApp->processEvents();

    // Widget should be in the status bar
    auto* sb = mw.statusBar();
    auto* found = sb->findChild<QLabel*>(QStringLiteral("CustomId"));
    ASSERT_NE(found, nullptr);

    // Context menu should include the new item (6 original + 1 custom = 7)
    QMenu menu(&mw);
    mw.buildStatusBarContextMenu(menu);
    EXPECT_EQ(menu.actions().size(), 7u);  // 6 original (progressBar hidden) + 1 custom
    bool foundAction = false;
    for (auto* action : menu.actions()) {
        if (action->text() == QStringLiteral("Custom Widget")) {
            foundAction = true;
            break;
        }
    }
    EXPECT_TRUE(foundAction);
}

// =====================================================================
// removeStatusBarItem: removes widget from registry
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for removeStatusBarItem
TEST(StatusBarTest, RemoveStatusBarItemUnregistersWidget)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* label = new QLabel(QStringLiteral("Temp"), &mw);
    mw.addStatusBarItem(label, StatusBarItemSpec("TempId", "Temp", StatusBarSlot::Left, 999, false, 0));
    qApp->processEvents();

    // Verify it's there (6 original in menu + 1 new = 7)
    QMenu menu1(&mw);
    mw.buildStatusBarContextMenu(menu1);
    EXPECT_EQ(menu1.actions().size(), 7u);  // 6 original (progressBar hidden) + 1 new

    // Remove it
    mw.removeStatusBarItem("TempId");
    qApp->processEvents();

    QMenu menu2(&mw);
    mw.buildStatusBarContextMenu(menu2);
    EXPECT_EQ(menu2.actions().size(), 6u);  // back to 6 original
}

// =====================================================================
// setStatusBarItemEnabled: toggles visibility
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for setStatusBarItemEnabled
TEST(StatusBarTest, SetStatusBarItemEnabledTogglesVisibility)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* label = new QLabel(QStringLiteral("Toggle me"), &mw);
    mw.addStatusBarItem(label, StatusBarItemSpec("ToggleId", "Toggle", StatusBarSlot::Right, 600, false, 0));
    qApp->processEvents();

    // Initially visible
    EXPECT_TRUE(label->isVisible());

    // Disable
    mw.setStatusBarItemEnabled("ToggleId", false);
    qApp->processEvents();
    EXPECT_FALSE(label->isVisible());

    // Re-enable
    mw.setStatusBarItemEnabled("ToggleId", true);
    qApp->processEvents();
    EXPECT_TRUE(label->isVisible());
}

// =====================================================================
// DimensionWidget: initial schema checkmark after construction
// =====================================================================

// Ported from: FreeCAD src/Gui/MainWindow.cpp:195-234 (ctor) + :278-294 (unitChanged)
TEST(StatusBarTest, DimensionWidgetInitialSchemaCheckmark)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    // Find DimensionWidget via objectName set by addStatusBarItem("UnitSystem")
    auto* dimWidget = mw.statusBar()->findChild<QWidget*>(QStringLiteral("UnitSystem"));
    ASSERT_NE(dimWidget, nullptr);

    // DimensionWidget should be a QPushButton with a menu
    auto* btn = qobject_cast<QPushButton*>(dimWidget);
    ASSERT_NE(btn, nullptr);
    ASSERT_NE(btn->menu(), nullptr);

    // After construction, unitChanged() should have checked exactly one action
    auto actions = btn->menu()->actions();
    ASSERT_GT(actions.size(), 0);

    int checkedCount = 0;
    for (auto* action : actions) {
        if (action->isChecked()) {
            checkedCount++;
        }
    }
    EXPECT_EQ(checkedCount, 1) << "unitChanged() should check exactly one action after construction";
}

// =====================================================================
// DimensionWidget: setUserSchema checks the correct action
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for setUserSchema
TEST(StatusBarTest, DimensionWidgetSetUserSchemaChecksAction)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* dimWidget = mw.statusBar()->findChild<QWidget*>(QStringLiteral("UnitSystem"));
    ASSERT_NE(dimWidget, nullptr);

    auto* btn = qobject_cast<QPushButton*>(dimWidget);
    ASSERT_NE(btn, nullptr);

    auto actions = btn->menu()->actions();
    ASSERT_GT(actions.size(), 2);

    // Switch to schema index 2
    mw.setUserSchema(2);
    qApp->processEvents();

    // Action at index 2 should be checked, others unchecked
    EXPECT_TRUE(actions[2]->isChecked());
    EXPECT_FALSE(actions[0]->isChecked());
    EXPECT_FALSE(actions[1]->isChecked());
}

// =====================================================================
// DimensionWidget: retranslateUi refreshes action texts
// =====================================================================

// Ported from: FreeCAD src/Gui/MainWindow.cpp:296-305 (retranslateUi)
TEST(StatusBarTest, DimensionWidgetRetranslateUiUpdatesTexts)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* dimWidget = mw.statusBar()->findChild<QWidget*>(QStringLiteral("UnitSystem"));
    ASSERT_NE(dimWidget, nullptr);

    auto* btn = qobject_cast<QPushButton*>(dimWidget);
    ASSERT_NE(btn, nullptr);

    auto actions = btn->menu()->actions();
    ASSERT_GT(actions.size(), 0);

    // Verify action texts match UnitsApi::getDescriptions()
    auto descriptions = Base::UnitsApi::getDescriptions();
    ASSERT_EQ(static_cast<int>(descriptions.size()), actions.size());

    for (int i = 0; i < actions.size(); ++i) {
        EXPECT_EQ(actions[i]->text().toStdString(), descriptions[i])
            << "Action text at index " << i << " should match getDescriptions()";
    }
}
