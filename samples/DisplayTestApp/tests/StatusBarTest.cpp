// Ported from: Authored — no reference test exists in FreeCAD for status bar registry
// Tests for MainWindow status bar item registration, context menu, customEvent,
// and DTA output spans (Utils.ts showStatus/showError semantics).
#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

#include "QtTestFixtures.h"
#include "Gui/MainWindow.h"
#include "Gui/MainWindow_p.h"
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
    auto* hintLabel = sb->findChild<QWidget*>(QStringLiteral("InputHints"));
    ASSERT_NE(hintLabel, nullptr);

    // 2026-10-07 删除侧：Preselection（瞬态消息条）与 UnitSystem（单位 schema
    // 选择器）死 chrome 已删——DTA 状态栏无对应面，断言其不再注册。
    EXPECT_EQ(sb->findChild<QWidget*>(QStringLiteral("Preselection")), nullptr);
    EXPECT_EQ(sb->findChild<QWidget*>(QStringLiteral("UnitSystem")), nullptr);
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

    // 1 registered widget → 1 toggle action（2026-10-07 删除侧后余 InputHints
    // 一项——DTA.StatusBar 行与 showstatus/showerror span 由 DtaTools 装配，
    // 不入 MainWindow 级上下文菜单）
    auto actions = menu.actions();
    EXPECT_EQ(actions.size(), 1u);

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
    EXPECT_TRUE(titles.contains(QStringLiteral("Input Hints")));
}

// =====================================================================
// showStatus: DTA Utils.ts span semantics (status → #showstatus, Err → #showerror)
// =====================================================================

// Ported from: Authored — locks the 2026-10-07 simplified showStatus to
// Utils.ts:8-26 (showStatus writes #showstatus; error-level writes #showerror;
// no transient QStatusBar::showMessage overlay — DTA has none).
TEST(StatusBarTest, ShowStatusWritesDtaSpans)
{
    ensureAppReady();
    MainWindow mw;
    mw.installDtaOutputSpans();
    mw.show();
    qApp->processEvents();

    mw.showStatus(MainWindow::None, QStringLiteral("All good"));
    qApp->processEvents();

    auto* span = mw.statusBar()->findChild<QLabel*>(QStringLiteral("showstatus"));
    ASSERT_NE(span, nullptr);
    EXPECT_EQ(span->text().toStdString(), std::string("All good"));

    auto* err = mw.statusBar()->findChild<QLabel*>(QStringLiteral("showerror"));
    ASSERT_NE(err, nullptr);
    EXPECT_TRUE(err->text().isEmpty()) << "non-Err level must not touch #showerror";

    // Err level routes to #showerror ONLY (Utils.showError :18-40——不双写
    // #showstatus；审计 B17 语义)。
    mw.showStatus(MainWindow::Err, QStringLiteral("Boom"));
    qApp->processEvents();
    EXPECT_EQ(err->text().toStdString(), std::string("Boom"));
    EXPECT_EQ(span->text().toStdString(), std::string("All good"))
        << "Err level must not overwrite #showstatus";

    // No transient overlay: QStatusBar's own message area stays empty (DTA has
    // no such transient message; the FreeCAD form was deleted with Preselection).
    EXPECT_TRUE(mw.statusBar()->currentMessage().isEmpty());
}

// =====================================================================
// customEvent: dispatches status message to the DTA span
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for customEvent dispatch
TEST(StatusBarTest, CustomEventDispatchesStatusToSpan)
{
    ensureAppReady();
    MainWindow mw;
    mw.installDtaOutputSpans();
    mw.show();
    qApp->processEvents();

    // Post a status-type custom event and verify the span text changes
    auto* event = new CustomMessageEvent(MainWindow::None, QStringLiteral("EvtMsg"));
    QApplication::postEvent(&mw, event);
    qApp->processEvents();

    auto* span = mw.statusBar()->findChild<QLabel*>(QStringLiteral("showstatus"));
    ASSERT_NE(span, nullptr);
    EXPECT_TRUE(span->text().contains(QStringLiteral("EvtMsg")))
        << "customEvent should dispatch status message to showStatus";
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

    auto* event = new CustomMessageEvent(MainWindow::None, QStringLiteral("TestDispatch"));
    QApplication::postEvent(&mw, event);
    qApp->processEvents();

    EXPECT_TRUE(mw.customEventCalled) << "customEvent should be called when event is posted";
    EXPECT_EQ(mw.lastEventType, static_cast<int>(QEvent::User));
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

    // Context menu should include the new item (1 original + 1 custom = 2)
    QMenu menu(&mw);
    mw.buildStatusBarContextMenu(menu);
    EXPECT_EQ(menu.actions().size(), 2u);  // 1 original (2026-10-07 删除侧后) + 1 custom
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

    // Verify it is there (1 original in menu + 1 new = 2)
    QMenu menu1(&mw);
    mw.buildStatusBarContextMenu(menu1);
    EXPECT_EQ(menu1.actions().size(), 2u);  // 1 original (2026-10-07 删除侧后) + 1 new

    // Remove it
    mw.removeStatusBarItem("TempId");
    qApp->processEvents();

    QMenu menu2(&mw);
    mw.buildStatusBarContextMenu(menu2);
    EXPECT_EQ(menu2.actions().size(), 1u);  // back to 1 original
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
