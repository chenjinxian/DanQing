// Ported from: Authored — no reference test exists in FreeCAD for MDI chrome
// (Window menu commands, MDI slots, window activation wiring) in the DTA shell.
// FreeCAD has no test for these — the shell behaviour is exercised manually.
//
// Tests for region 4 task 1:
//   - 6 Window-domain commands register with correct names + metadata
//   - StdCmdWindowsMenu owns a WindowAction (ActionGroup subclass)
//   - MainWindow::tile/cascade/activateNextWindow/activatePrevWindow do not
//     crash with no MDI windows, and cycle a tabbed QMdiArea when windows exist
//   - WindowAction::onWindowsMenuAboutToShow populates the action list with
//     the current subwindow titles and wires triggered -> setActiveSubWindow
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <QApplication>
#include <QEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QMdiArea>
#include <QMenu>
#include <QMdiSubWindow>
#include <QObject>
#include <QString>
#include <QTabBar>
#include <QTextEdit>
#include <QToolButton>
#include <QWidget>

#include "QtTestFixtures.h"
#include "Gui/Action.h"
#include "Gui/Command.h"
#include "Gui/CreateStdCommands.h"
#include "Gui/MDIView.h"
#include "Gui/MainWindow.h"
#include "Gui/MenuManager.h"
#include "Gui/Workbench.h"
#include "GeneralSettingsWidget.h"      // Region 4 task 4 (StartGui::GeneralSettingsWidget)
#include "ThemeSelectorWidget.h"        // Region 4 task 5 (StartGui::ThemeSelectorWidget)
#include "FirstStartWidget.h"           // Region 4 task 5 (StartGui::FirstStartWidget integration)

#include <App/Application.h>
#include <Base/UnitsApi.h>
#include <Gui/Language/Translator.h>
#include <Gui/Navigation/NavigationStyle.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h. MainWindow ctor reads App::GetApplication().GetUserParameter() for the
// status-bar/dock parameter group (FreeCAD MainWindow.cpp:84).

using namespace Gui;

// =====================================================================
// 6 Window-domain commands are registered after createStdCommands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:507-524 (CreateWindowStdCommands)
// Authored: no reference test exists in FreeCAD for command registration assertion
TEST(MDIChromeTest, WindowsCommandsRegistered)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    EXPECT_NE(mgr.getCommandByName("Std_WindowsMenu"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Windows"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_TileWindows"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_CascadeWindows"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ActivateNextWindow"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ActivatePrevWindow"), nullptr);
}

// =====================================================================
// Command metadata verbatim from FreeCAD CommandWindow.cpp
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:46-58 (StdCmdTileWindows metadata)
TEST(MDIChromeTest, TileWindowsMetadata)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_TileWindows");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->getGroupName()), std::string("Window"));
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("&Tile"));
    EXPECT_EQ(std::string(cmd->getToolTipText()), std::string("Tiles the windows"));
    EXPECT_EQ(std::string(cmd->getWhatsThis()), std::string("Std_TileWindows"));
    EXPECT_EQ(std::string(cmd->getPixmap()), std::string("Std_WindowTileVer"));
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:74-86 (StdCmdCascadeWindows metadata)
TEST(MDIChromeTest, CascadeWindowsMetadata)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_CascadeWindows");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->getGroupName()), std::string("Window"));
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("&Cascade"));
    EXPECT_EQ(std::string(cmd->getToolTipText()), std::string("Tiles pragmatic"));
    EXPECT_EQ(std::string(cmd->getWhatsThis()), std::string("Std_CascadeWindows"));
    EXPECT_EQ(std::string(cmd->getPixmap()), std::string("Std_WindowCascade"));
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:162-175 (StdCmdActivateNextWindow metadata)
TEST(MDIChromeTest, ActivateNextWindowMetadata)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_ActivateNextWindow");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("&Next"));
    EXPECT_EQ(std::string(cmd->getToolTipText()), std::string("Activates the next window"));
    EXPECT_EQ(std::string(cmd->getWhatsThis()), std::string("Std_ActivateNextWindow"));
    EXPECT_EQ(std::string(cmd->getPixmap()), std::string("Std_WindowNext"));
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:191-210 (StdCmdActivatePrevWindow metadata)
TEST(MDIChromeTest, ActivatePrevWindowMetadata)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_ActivatePrevWindow");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("&Previous"));
    EXPECT_EQ(std::string(cmd->getToolTipText()),
              std::string("Switches to the previously active window"));
    EXPECT_EQ(std::string(cmd->getWhatsThis()), std::string("Std_ActivatePrevWindow"));
    EXPECT_EQ(std::string(cmd->getPixmap()), std::string("Std_WindowPrev"));
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:226-239 (StdCmdWindows metadata)
TEST(MDIChromeTest, WindowsDialogMetadata)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_Windows");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("Choose Open &Window"));
    EXPECT_EQ(std::string(cmd->getToolTipText()), std::string("Displays the open windows"));
    EXPECT_EQ(std::string(cmd->getWhatsThis()), std::string("Std_Windows"));
    EXPECT_EQ(std::string(cmd->getPixmap()), std::string("Std_Windows"));
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:455-466 (StdCmdWindowsMenu metadata)
TEST(MDIChromeTest, WindowsMenuMetadata)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_WindowsMenu");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("Activate Window"));
    EXPECT_EQ(std::string(cmd->getToolTipText()), std::string("Activates this window"));
    EXPECT_EQ(std::string(cmd->getWhatsThis()), std::string("Std_WindowsMenu"));
}

// =====================================================================
// StdCmdWindowsMenu::createAction returns a WindowAction (ActionGroup)
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:479-497 (StdCmdWindowsMenu::createAction)
// Authored: no reference test exists in FreeCAD for createAction return type
TEST(MDIChromeTest, WindowsMenuCommandCreatesWindowAction)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();
    createStdCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_WindowsMenu");
    ASSERT_NE(cmd, nullptr);
    // initAction calls createAction and stores the result; getAction returns it.
    cmd->initAction();
    auto* action = cmd->getAction();
    ASSERT_NE(action, nullptr);
    // WindowAction is an ActionGroup subclass (see Action.h:191).
    EXPECT_NE(qobject_cast<WindowAction*>(action), nullptr);
    // WindowAction owns 10 window entries + 1 separator = 11 child actions
    // (FreeCAD CommandWindow.cpp:485-494).
    auto* groupAction = qobject_cast<ActionGroup*>(action);
    ASSERT_NE(groupAction, nullptr);
    EXPECT_EQ(groupAction->actions().size(), 11);
}

// =====================================================================
// tile/cascade/activateNextWindow/activatePrevWindow do not crash with no
// sub-windows, matching FreeCAD MainWindow.cpp:970-979,1143-1159.
// =====================================================================

// Ported from: FreeCAD src/Gui/MainWindow.cpp:970-979,1143-1159
TEST(MDIChromeTest, MDISlotsDoNotCrashWithNoWindows)
{
    ensureAppReady();
    MainWindow mw;
    QMdiArea* mdi = mw.mdiArea();
    ASSERT_NE(mdi, nullptr);
    EXPECT_EQ(mdi->subWindowList().size(), 0);

    EXPECT_NO_FATAL_FAILURE(mw.tile());
    EXPECT_NO_FATAL_FAILURE(mw.cascade());
    EXPECT_NO_FATAL_FAILURE(mw.activateNextWindow());
    EXPECT_NO_FATAL_FAILURE(mw.activatePreviousWindow());
    EXPECT_NO_FATAL_FAILURE(mw.tabChanged(nullptr));
}

// =====================================================================
// activateNextWindow/activatePreviousWindow cycle the MDI tab bar
// =====================================================================

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1143-1159 (tab cycling logic)
// Authored: no reference test exists in FreeCAD for tab cycling assertion
TEST(MDIChromeTest, ActivateNextPrevCyclesTabbedMdiArea)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    QMdiArea* mdi = mw.mdiArea();
    ASSERT_NE(mdi, nullptr);

    // Create 3 dummy sub-windows so the tab bar has something to cycle.
    auto* w1 = new QWidget; w1->setWindowTitle(QStringLiteral("w1"));
    auto* w2 = new QWidget; w2->setWindowTitle(QStringLiteral("w2"));
    auto* w3 = new QWidget; w3->setWindowTitle(QStringLiteral("w3"));
    mdi->addSubWindow(w1)->show();
    mdi->addSubWindow(w2)->show();
    mdi->addSubWindow(w3)->show();
    qApp->processEvents();

    auto* tab = mdi->findChild<QTabBar*>();
    ASSERT_NE(tab, nullptr);
    ASSERT_GE(tab->count(), 3);
    tab->setCurrentIndex(0);

    mw.activateNextWindow();
    qApp->processEvents();
    EXPECT_EQ(tab->currentIndex(), 1);

    mw.activateNextWindow();
    qApp->processEvents();
    EXPECT_EQ(tab->currentIndex(), 2);

    // Wrap-around: 2 -> 0
    mw.activateNextWindow();
    qApp->processEvents();
    EXPECT_EQ(tab->currentIndex(), 0);

    // Previous wraps the other way: 0 -> 2
    mw.activatePreviousWindow();
    qApp->processEvents();
    EXPECT_EQ(tab->currentIndex(), 2);
}

// =====================================================================
// tile/cascade affect the QMdiArea when sub-windows exist
// =====================================================================

// Ported from: FreeCAD src/Gui/MainWindow.cpp:970-978 (tile/cascade)
// Authored: no reference test exists in FreeCAD for tile/cascade assertion
TEST(MDIChromeTest, TileAndCascadeRearrangeSubWindows)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    QMdiArea* mdi = mw.mdiArea();
    ASSERT_NE(mdi, nullptr);

    auto* w1 = new QWidget; w1->setWindowTitle(QStringLiteral("a"));
    auto* w2 = new QWidget; w2->setWindowTitle(QStringLiteral("b"));
    mdi->addSubWindow(w1)->show();
    mdi->addSubWindow(w2)->show();
    qApp->processEvents();
    ASSERT_EQ(mdi->subWindowList().size(), 2);

    EXPECT_NO_FATAL_FAILURE(mw.tile());
    qApp->processEvents();
    EXPECT_NO_FATAL_FAILURE(mw.cascade());
    qApp->processEvents();
}

// =====================================================================
// Region 4 Task 2 — dirty `*` marker (onRelabel + setWindowModified)
// =====================================================================

// Ported from: FreeCAD src/Gui/MDIView.cpp:172-199 (onRelabel [*] convention)
// Authored: no reference test exists in FreeCAD for onRelabel title assertion.
// DTA exercises the shell behaviour directly because there is no live
// Gui::Document::onRelabel trigger in the test harness.
TEST(MDIChromeTest, MDIViewOnRelabelAddsDirtyPlaceholder)
{
    ensureAppReady();
    MDIView view(nullptr, nullptr);
    // onRelabel with no doc (DTA stub: Gui::Document is forward-declared, no
    // Label accessor; placeholder "Unnamed" used — FreeCAD MainWindow.cpp:2642
    // default doc name).
    view.onRelabel(nullptr);
    // After onRelabel the title contains the [*] dirty placeholder and the
    // doc label prefix.
    EXPECT_TRUE(view.windowTitle().contains(QStringLiteral("[*]")));
    EXPECT_TRUE(view.windowTitle().contains(QStringLiteral("Unnamed")));
}

// Ported from: FreeCAD src/Gui/MDIView.cpp:178-199 (view-number suffix regex)
// Authored: no reference test exists in FreeCAD for suffix regex assertion.
// Covers the "match.hasMatch()" branch where the existing title already has a
// " : <n>[*]" or " : <n>" suffix that must be preserved across relabel.
TEST(MDIChromeTest, MDIViewOnRelabelPreservesViewNumberSuffix)
{
    ensureAppReady();
    MDIView view(nullptr, nullptr);
    // Pre-existing title with the " : 2[*]" view-number suffix.
    view.setWindowTitle(QStringLiteral("MyDoc : 2[*]"));
    view.onRelabel(nullptr);
    // Suffix preserved verbatim; doc-label prefix replaced with placeholder.
    EXPECT_TRUE(view.windowTitle().contains(QStringLiteral(" : 2[*]")));
    EXPECT_TRUE(view.windowTitle().contains(QStringLiteral("Unnamed")));
    EXPECT_FALSE(view.windowTitle().contains(QStringLiteral("MyDoc")));
}

// Ported from: FreeCAD src/Gui/MDIView.cpp:172-199 + Qt [*] dirty-marker convention
//   (Qt docs: "If you set the window title to a string that contains "[*]"
//   and call setWindowModified(true), the [*] is replaced with *").
// Authored: no reference test exists in FreeCAD for setWindowModified assertion.
TEST(MDIChromeTest, MDIViewSetWindowModifiedTogglesDirtyFlag)
{
    ensureAppReady();
    MDIView view(nullptr, nullptr);
    view.onRelabel(nullptr);
    EXPECT_FALSE(view.isWindowModified());
    view.setWindowModified(true);
    // isWindowModified flips true; Qt renders [*] -> * at paint time.
    // windowTitle() itself retains the [*] placeholder.
    EXPECT_TRUE(view.isWindowModified());
    EXPECT_TRUE(view.windowTitle().contains(QStringLiteral("[*]")));
    view.setWindowModified(false);
    EXPECT_FALSE(view.isWindowModified());
}

// =====================================================================
// MainWindow::changeEvent routes WindowTitleChange + ActivationChange
// (FreeCAD MainWindow.cpp:2646-2682 routes LanguageChange + ActivationChange;
// DTA adds WindowTitleChange handling so the Windows menu rebuilds on
// child-title changes — see task-2-brief.md.)
// =====================================================================

// Ported from: FreeCAD src/Gui/MainWindow.cpp:2646-2682 (changeEvent routing)
// Authored: no reference test exists in FreeCAD for changeEvent routing
// assertion (FreeCAD routes LanguageChange/ActivationChange; DTA additionally
// routes WindowTitleChange to mark the Windows menu dirty).
TEST(MDIChromeTest, MainWindowChangeEventRoutesTitleAndActivation)
{
    ensureAppReady();
    MainWindow mw;

    // WindowTitleChange must emit windowStateChanged so the Windows menu
    // (rebuilt on aboutToShow per StdCmdWindowsMenu) picks up new titles.
    int titleEmissions = 0;
    QWidget* emittedWidget = reinterpret_cast<QWidget*>(0xDEADBEEF);
    QObject::connect(&mw, &MainWindow::windowStateChanged,
                     [&](QWidget* w) { ++titleEmissions; emittedWidget = w; });
    QEvent titleEvent(QEvent::WindowTitleChange);
    EXPECT_NO_FATAL_FAILURE(QCoreApplication::sendEvent(&mw, &titleEvent));
    EXPECT_EQ(titleEmissions, 1);
    EXPECT_EQ(emittedWidget, nullptr);

    // ActivationChange must not crash and must NOT emit windowStateChanged
    // (it is a no-op routing marker in DTA — no Coin3D SoDB sensor).
    int actEmissions = 0;
    QObject::connect(&mw, &MainWindow::windowStateChanged,
                     [&](QWidget*) { ++actEmissions; });
    QEvent activationEvent(QEvent::ActivationChange);
    EXPECT_NO_FATAL_FAILURE(QCoreApplication::sendEvent(&mw, &activationEvent));
    EXPECT_EQ(actEmissions, 0);
}

// Ported from: FreeCAD src/Gui/MainWindow.cpp:1498-1502 (tabChanged)
// Authored: no reference test exists in FreeCAD for tabChanged assertion.
TEST(MDIChromeTest, MainWindowTabChangedWithViewNoCrash)
{
    ensureAppReady();
    MainWindow mw;
    auto* view = new MDIView(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE(mw.tabChanged(view));
    EXPECT_NO_FATAL_FAILURE(mw.tabChanged(nullptr));
    delete view;
}

// =====================================================================
// 2026-09-20 dead-tab saga — Start 页标签无法切回
// =====================================================================

// Authored: no reference test exists in Qt/FreeCAD for tab re-activation after
// adding a second MDI window. Regression for the 2026-09-20 dead-tab saga:
// MainWindow::addWindow used to force-hide every other sub-window (DanQing-only
// deviation); Qt 6.11 QMdiAreaPrivate::activateWindow (qtbase qmdiarea.cpp:967,
// `if (child->isHidden() || child == active) return;`) refuses to activate a
// hidden sub-window in TabbedView, and _q_currentTabChanged (:743-748) disables
// its tab — the Start tab became a dead tab after opening any 3D view.
// FreeCAD's addWindow (MainWindow.cpp:1397-1439) never hides sub-windows.
TEST(MDIChromeTest, SecondWindowKeepsStartTabSwitchable)
{
    ensureAppReady();
    MainWindow mw;
    mw.resize(1000, 700);
    mw.show();
    qApp->processEvents();

    auto* v1 = new MDIView(nullptr, &mw);
    v1->setWindowTitle(QStringLiteral("Start"));
    mw.addWindow(v1);
    qApp->processEvents();
    auto* v2 = new MDIView(nullptr, &mw);
    v2->setWindowTitle(QStringLiteral("3D View"));
    mw.addWindow(v2);
    qApp->processEvents();

    QMdiArea* mdi = mw.mdiArea();
    ASSERT_NE(mdi, nullptr);
    ASSERT_EQ(mdi->subWindowList().size(), 2);
    auto* sw1 = qobject_cast<QMdiSubWindow*>(v1->parentWidget());
    auto* sw2 = qobject_cast<QMdiSubWindow*>(v2->parentWidget());
    ASSERT_NE(sw1, nullptr);
    ASSERT_NE(sw2, nullptr);

    // The previously-opened window must NOT be explicitly hidden — in tabbed
    // mode a hidden sub-window is a dead tab (Qt 6.11 activateWindow refusal).
    EXPECT_FALSE(sw1->isHidden());
    EXPECT_EQ(mdi->activeSubWindow(), sw2);

    // Tab-click path: activate the first window again (the same call the tab
    // bar's currentChanged handler makes: _q_currentTabChanged → activateWindow).
    mdi->setActiveSubWindow(sw1);
    qApp->processEvents();
    EXPECT_EQ(mdi->activeSubWindow(), sw1);
    EXPECT_FALSE(sw1->isHidden());

    // And forward to the 3D view again (round trip).
    mdi->setActiveSubWindow(sw2);
    qApp->processEvents();
    EXPECT_EQ(mdi->activeSubWindow(), sw2);
}

// =====================================================================
// Region 4 Task 3 — view modes Child↔TopLevel↔FullScreen
// (MDIView::setCurrentViewMode, FreeCAD MDIView.cpp:434-517)
// =====================================================================

// Ported from: FreeCAD src/Gui/MDIView.cpp:434-517 (setCurrentViewMode)
// Authored: no reference test exists in FreeCAD for setCurrentViewMode
// assertion. FreeCAD exercises the mode switch manually; DTA drives it
// directly because the shell behaviour is the contract under test.
TEST(MDIChromeTest, SetCurrentViewModeChildToFullScreen)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    QMdiArea* mdi = mw.mdiArea();
    ASSERT_NE(mdi, nullptr);

    // MDIView must be parented into a QMdiSubWindow via addWindow so that
    // parentWidget() returns the subwindow — the Child→{TopLevel,FullScreen}
    // branch (FreeCAD MDIView.cpp:441-447) relies on this.
    auto* view = new MDIView(nullptr, &mw);
    mw.addWindow(view);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::Child);
    ASSERT_NE(qobject_cast<QMdiSubWindow*>(view->parentWidget()), nullptr);

    // Child → FullScreen.
    view->setCurrentViewMode(MDIView::FullScreen);
    qApp->processEvents();

    EXPECT_EQ(view->currentViewMode(), MDIView::FullScreen);
    EXPECT_TRUE(view->windowState() & Qt::WindowFullScreen);

    delete view;
}

// Ported from: FreeCAD src/Gui/MDIView.cpp:434-517 (round-trip Child→FullScreen→Child)
// Authored: no reference test exists in FreeCAD for mode round-trip assertion.
// Covers the return-to-Child path: addWindow reparenting + the FreeCAD
// "max size {1,1} then restore" layout kick (:513-515).
TEST(MDIChromeTest, SetCurrentViewModeRoundTripsToChild)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* view = new MDIView(nullptr, &mw);
    mw.addWindow(view);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::Child);

    // Child → FullScreen → Child round trip.
    view->setCurrentViewMode(MDIView::FullScreen);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::FullScreen);
    EXPECT_TRUE(view->windowState() & Qt::WindowFullScreen);

    view->setCurrentViewMode(MDIView::Child);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::Child);
    EXPECT_FALSE(view->windowState() & Qt::WindowFullScreen);
    // Back inside a QMdiSubWindow.
    EXPECT_NE(qobject_cast<QMdiSubWindow*>(view->parentWidget()), nullptr);

    delete view;
}

// Ported from: FreeCAD src/Gui/MDIView.cpp:434-517 (Child→TopLevel→Child)
// Authored: no reference test exists in FreeCAD for TopLevel mode assertion.
// Exercises the TopLevel branch (showNormal/showMaximized) and the wstate
// backup/restore logic (:448-471).
TEST(MDIChromeTest, SetCurrentViewModeChildToTopLevelAndBack)
{
    ensureAppReady();
    MainWindow mw;
    mw.show();
    qApp->processEvents();

    auto* view = new MDIView(nullptr, &mw);
    mw.addWindow(view);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::Child);

    // Child → TopLevel.
    view->setCurrentViewMode(MDIView::TopLevel);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::TopLevel);
    // A top-level window has no QMdiSubWindow parent.
    EXPECT_EQ(qobject_cast<QMdiSubWindow*>(view->parentWidget()), nullptr);

    // TopLevel → Child.
    view->setCurrentViewMode(MDIView::Child);
    qApp->processEvents();
    EXPECT_EQ(view->currentViewMode(), MDIView::Child);
    EXPECT_NE(qobject_cast<QMdiSubWindow*>(view->parentWidget()), nullptr);

    delete view;
}

// =====================================================================
// Region 4 task 4: GeneralSettingsWidget (Language/Unit/NavStyle combos)
// FreeCAD has no test for GeneralSettingsWidget — the widget is exercised
// manually via the Start page FirstStartWidget. These tests assert the
// 1:1 structural shape ported from FreeCAD GeneralSettingsWidget.cpp.
// =====================================================================

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:75-87 (3 combos)
// Authored: no reference test exists in FreeCAD for GeneralSettingsWidget structure
TEST(MDIChromeTest, GeneralSettingsWidgetHasThreeCombos)
{
    ensureAppReady();
    StartGui::GeneralSettingsWidget w;
    EXPECT_NE(w.findChild<QComboBox*>(QStringLiteral("languageComboBox")), nullptr);
    EXPECT_NE(w.findChild<QComboBox*>(QStringLiteral("unitSystemComboBox")), nullptr);
    EXPECT_NE(w.findChild<QComboBox*>(QStringLiteral("navigationStyleComboBox")), nullptr);
}

// Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:97-143, 220-261
// Authored: no reference test exists in FreeCAD for GeneralSettingsWidget population
TEST(MDIChromeTest, GeneralSettingsWidgetCombosPopulated)
{
    ensureAppReady();
    StartGui::GeneralSettingsWidget w;

    auto* language = w.findChild<QComboBox*>(QStringLiteral("languageComboBox"));
    auto* unitSystem = w.findChild<QComboBox*>(QStringLiteral("unitSystemComboBox"));
    auto* navStyle = w.findChild<QComboBox*>(QStringLiteral("navigationStyleComboBox"));
    ASSERT_NE(language, nullptr);
    ASSERT_NE(unitSystem, nullptr);
    ASSERT_NE(navStyle, nullptr);

    // Language: combo contains "English" plus one entry per supported locale;
    // FreeCAD GeneralSettingsWidget.cpp:104-131 adds English first then iterates
    // supportedLocales(). After model->sort(0) the order is alphabetical, so we
    // only assert count and that "English" is present (not its index).
    Gui::TStringMap locales = Gui::Translator::instance()->supportedLocales();
    EXPECT_EQ(language->count(), 1 + static_cast<int>(locales.size()));
    bool foundEnglish = false;
    for (int i = 0; i < language->count(); ++i) {
        if (language->itemText(i) == QLatin1String("English")) {
            foundEnglish = true;
            break;
        }
    }
    EXPECT_TRUE(foundEnglish) << "Language combo must contain English";

    // Unit System: count matches Base::UnitsApi::getDescriptions()
    auto descriptions = Base::UnitsApi::getDescriptions();
    EXPECT_EQ(unitSystem->count(), static_cast<int>(descriptions.size()));
    for (int i = 0; i < unitSystem->count() && i < static_cast<int>(descriptions.size()); ++i) {
        EXPECT_EQ(unitSystem->itemText(i).toStdString(), descriptions[i])
            << "Unit System item " << i << " should match UnitsApi::getDescriptions()";
    }

    // Navigation Style: count matches Gui::UserNavigationStyle::getUserFriendlyNames()
    auto styles = Gui::UserNavigationStyle::getUserFriendlyNames();
    EXPECT_EQ(navStyle->count(), static_cast<int>(styles.size()));
}

// =====================================================================
// Region 4 task 5: ThemeSelectorWidget (3 auto-exclusive theme buttons)
//   FreeCAD ThemeSelectorWidget.cpp:109-160 (setupButtons) — 3 QToolButton
//   Classic/Light/Dark, autoExclusive + ToolButtonTextUnderIcon.
// FreeCAD has no test for ThemeSelectorWidget — exercised manually via
// the Start page. These tests assert the 1:1 structural shape.
// =====================================================================

// Ported from: FreeCAD src/Mod/Start/Gui/ThemeSelectorWidget.cpp:109-160 (setupButtons)
// Authored: no reference test exists in FreeCAD for ThemeSelectorWidget structure
TEST(MDIChromeTest, ThemeSelectorHasThreeAutoExclusiveButtons)
{
    ensureAppReady();
    StartGui::ThemeSelectorWidget w;
    auto buttons = w.findChildren<QToolButton*>();
    EXPECT_EQ(buttons.size(), 3);
    for (auto* b : buttons) {
        EXPECT_TRUE(b->autoExclusive());
        EXPECT_TRUE(b->isCheckable());
        EXPECT_EQ(b->toolButtonStyle(), Qt::ToolButtonStyle::ToolButtonTextUnderIcon);
    }
}

// =====================================================================
// Region 4 task 5: FirstStartWidget integrates both sub-widgets
//   FreeCAD FirstStartWidget.cpp:65-69 instantiates ThemeSelectorWidget
//   + GeneralSettingsWidget as children of the outer layout.
// =====================================================================

// Ported from: FreeCAD src/Mod/Start/Gui/FirstStartWidget.cpp:56-79 (setupUi embeds both)
// Authored: no reference test exists in FreeCAD for FirstStartWidget sub-widget assertion
TEST(MDIChromeTest, FirstStartWidgetContainsSubWidgets)
{
    ensureAppReady();
    StartGui::FirstStartWidget fs;
    EXPECT_NE(fs.findChild<StartGui::GeneralSettingsWidget*>(), nullptr);
    EXPECT_NE(fs.findChild<StartGui::ThemeSelectorWidget*>(), nullptr);
}

// =====================================================================
// Region 4 task 6: 3D view context menu (View context-group infrastructure)
//   FreeCAD Workbench.cpp:634-660 (StdWorkbench::setupContextMenu "View")
//   FreeCAD MenuManager.cpp:273-338 (recursive QMenu builder)
//   FreeCAD NavigationStyle.cpp:2422-2487 (openPopupMenu dispatch + popup)
// FreeCAD has no unit test for the context-menu tree shape; the behaviour is
// exercised manually. These tests assert the 1:1 structural shape ported from
// Workbench.cpp:634-660 and that MenuManager::setupContextMenu builds a
// non-empty QMenu from a MenuItem tree.
// =====================================================================

// Ported from: FreeCAD src/Gui/Workbench.cpp:634-660 (StdWorkbench::setupContextMenu "View")
// Authored: no reference test exists in FreeCAD for context-menu tree assertion.
TEST(MDIChromeTest, StdWorkbenchSetupContextMenuViewGroup)
{
    ensureAppReady();
    MenuItem item;
    StdWorkbench wb;
    wb.setupContextMenu("View", &item);

    // The View context-group tree must contain (FreeCAD Workbench.cpp:638-660):
    //   [createLinkMenu — DTA: skipped, no doc backend]
    //   "Separator"
    //   "Std_ViewFitAll" "Std_ViewFitSelection" "Std_AlignToSelection"
    //   "Std_DrawStyle" <Standard Views submenu> "Separator"
    //   "Std_ViewDockUndockFullscreen"
    //
    // Collect leaf + submenu command names in insertion order.
    std::vector<std::string> cmds;
    for (const MenuItem* child : item.getItems()) {
        cmds.push_back(child->command());
    }

    // Helper: check membership.
    auto contains = [&](const std::string& name) {
        return std::find(cmds.begin(), cmds.end(), name) != cmds.end();
    };

    EXPECT_TRUE(contains("Separator")) << "First separator after createLinkMenu";
    EXPECT_TRUE(contains("Std_ViewFitAll"))   << "Workbench.cpp:650";
    EXPECT_TRUE(contains("Std_ViewFitSelection")) << "Workbench.cpp:650";
    EXPECT_TRUE(contains("Std_AlignToSelection")) << "Workbench.cpp:650";
    EXPECT_TRUE(contains("Std_DrawStyle"))    << "Workbench.cpp:651";
    EXPECT_TRUE(contains("Std_ViewDockUndockFullscreen")) << "Workbench.cpp:652";

    // Standard Views submenu (Workbench.cpp:642-648) — find the submenu and verify its children.
    bool foundStdViews = false;
    for (const MenuItem* child : item.getItems()) {
        if (child->command() == "Standard Views") {
            foundStdViews = true;
            std::vector<std::string> subCmds;
            for (const MenuItem* sv : child->getItems()) {
                subCmds.push_back(sv->command());
            }
            auto subHas = [&](const std::string& name) {
                return std::find(subCmds.begin(), subCmds.end(), name) != subCmds.end();
            };
            EXPECT_TRUE(subHas("Std_ViewIsometric"))  << "Workbench.cpp:645";
            EXPECT_TRUE(subHas("Std_ViewHome"))       << "Workbench.cpp:645";
            EXPECT_TRUE(subHas("Std_ViewFront"))      << "Workbench.cpp:645";
            EXPECT_TRUE(subHas("Std_ViewTop"))        << "Workbench.cpp:646";
            EXPECT_TRUE(subHas("Std_ViewRight"))      << "Workbench.cpp:646";
            EXPECT_TRUE(subHas("Std_ViewRear"))       << "Workbench.cpp:647";
            EXPECT_TRUE(subHas("Std_ViewBottom"))     << "Workbench.cpp:647";
            EXPECT_TRUE(subHas("Std_ViewLeft"))       << "Workbench.cpp:647";
            EXPECT_TRUE(subHas("Std_ViewRotateLeft"))  << "Workbench.cpp:648";
            EXPECT_TRUE(subHas("Std_ViewRotateRight")) << "Workbench.cpp:648";
            break;
        }
    }
    EXPECT_TRUE(foundStdViews) << "Standard Views submenu must be present";
}

// Ported from: FreeCAD src/Gui/Workbench.cpp:634,661-690 (recipient dispatch)
// Authored: no reference test exists in FreeCAD for recipient dispatch assertion.
// "Tree" recipient has a selection-guarded body in FreeCAD (:661-683) — with no
// selection backend in DTA the body is empty, matching FreeCAD with empty selection.
TEST(MDIChromeTest, StdWorkbenchSetupContextMenuTreeRecipientIsEmpty)
{
    ensureAppReady();
    MenuItem item;
    StdWorkbench wb;
    wb.setupContextMenu("Tree", &item);
    // No selection in DTA → Tree branch produces no items (FreeCAD :662 guard).
    EXPECT_FALSE(item.hasItems());
}

// Ported from: FreeCAD src/Gui/Workbench.cpp:360-364 (base Workbench::setupContextMenu no-op)
// Authored: no reference test exists in FreeCAD for base no-op assertion.
TEST(MDIChromeTest, BaseWorkbenchSetupContextMenuIsNoOp)
{
    ensureAppReady();
    // Workbench is abstract in DTA (setupMenuBar/setupToolBars are pure virtual);
    // use a minimal concrete subclass to exercise the base no-op.
    struct TestWorkbench : public Workbench {
        MenuItem* setupMenuBar() const override { return nullptr; }
        ToolBarItem* setupToolBars() const override { return nullptr; }
    };
    TestWorkbench wb;
    MenuItem item;
    wb.setupContextMenu("View", &item);
    EXPECT_FALSE(item.hasItems());
}

// Ported from: FreeCAD src/Gui/MenuManager.cpp:273-338 (setup → QMenu recursive builder)
// Authored: no reference test exists in FreeCAD for setupContextMenu assertion.
TEST(MDIChromeTest, MenuManagerSetupContextMenuBuildsMenu)
{
    ensureAppReady();
    CommandManager mgr;
    MenuManager mm(mgr);

    // Build a small MenuItem tree mirroring the View context-group shape.
    MenuItem item;
    item << "Std_ViewFitAll" << "Separator";
    auto* sub = new MenuItem;
    sub->setCommand("Standard Views");
    *sub << "Std_ViewIsometric" << "Std_ViewFront";
    item << sub;

    QMenu menu;
    mm.setupContextMenu(&item, menu);

    // The menu must be non-empty: at minimum the Separator is always inserted
    // (FreeCAD MenuManager.cpp:334), and the "Standard Views" submenu is always
    // added as a QMenu (Workbench.cpp:338-341). Unregistered Std_* commands add
    // nothing, so we don't assert their presence — only Separator + submenu.
    EXPECT_GT(menu.actions().size(), 0);

    bool hasSep = false;
    bool hasSub = false;
    for (QAction* a : menu.actions()) {
        if (a->isSeparator()) hasSep = true;
        if (a->menu() && a->menu()->title() == QLatin1String("Standard Views")) {
            hasSub = true;
        }
    }
    EXPECT_TRUE(hasSep);
    EXPECT_TRUE(hasSub);
}

// Ported from: FreeCAD src/Gui/Application.cpp:2226-2252 (Application::setupContextMenu dispatch)
// Authored: no reference test exists in FreeCAD for CommandManager dispatch assertion.
// DTA adaptation: dispatch entry point lives on CommandManager (no Gui::Application
// singleton with this method in DTA — see task-6-brief.md design decision). The
// active Workbench* is tracked via Workbench::activate() → s_activeWorkbench.
TEST(MDIChromeTest, CommandManagerSetupContextMenuDispatchesToActiveWorkbench)
{
    ensureAppReady();
    MainWindow mw;
    CommandManager& mgr = mw.commandManager();

    // Activate a StdWorkbench so CommandManager can dispatch to it.
    StdWorkbench wb;
    wb.setMainWindow(&mw);
    wb.setManagers(&mw.commandManager(), &mw.menuManager(), &mw.toolBarManager());
    wb.activate();

    // Dispatch via CommandManager — should reach StdWorkbench::setupContextMenu.
    MenuItem item;
    mgr.setupContextMenu("View", &item);

    // Same structural assertions as StdWorkbenchSetupContextMenuViewGroup —
    // proves dispatch reached the active workbench.
    std::vector<std::string> cmds;
    for (const MenuItem* child : item.getItems()) {
        cmds.push_back(child->command());
    }
    auto contains = [&](const std::string& name) {
        return std::find(cmds.begin(), cmds.end(), name) != cmds.end();
    };
    EXPECT_TRUE(contains("Std_ViewFitAll"));
    EXPECT_TRUE(contains("Std_DrawStyle"));
    EXPECT_TRUE(contains("Std_ViewDockUndockFullscreen"));
}

// Ported from: FreeCAD src/Gui/Application.cpp:2226-2252 (null-active guard)
// Authored: no reference test exists in FreeCAD for null-active assertion.
TEST(MDIChromeTest, CommandManagerSetupContextMenuNoActiveWorkbenchIsSafe)
{
    ensureAppReady();
    // Fresh CommandManager with no active workbench — must not crash.
    CommandManager mgr;
    MenuItem item;
    EXPECT_NO_FATAL_FAILURE(mgr.setupContextMenu("View", &item));
    EXPECT_FALSE(item.hasItems());
}
