// Ported from: Authored — no reference unit test exists in FreeCAD for command-to-onMsg
// dispatch routing (FreeCAD tests run the full Gui::Application + real view stack). DTA
// verifies the dispatch contract with a mock MDIView instead.
//
// Invocation note: FreeCAD's CommandManager::runCommand(Command* pCmd, int iMsg) calls
// pCmd->activated(iMsg) through a Command* base pointer — the override is protected in the
// derived class (DEF_STD_CMD_A) but public in Command itself. The Command& casts below
// mirror that exact dispatch path.
#include <gtest/gtest.h>
#include "QtTestFixtures.h"

#include <App/Application.h>

#include "Gui/MainWindow.h"
#include "Gui/Application.h"
#include "Gui/CommandView.h"
#include "Gui/CommandDoc.h"
#include "Gui/CommandWindow.h"  // declares createWindowCommands() registry helper
#include "Gui/CreateStdCommands.h"  // declares createStdCommands() master registry
#include "MockMDIView.h"

using namespace Gui;

// Ported from: FreeCAD StdCmdViewFitAll::activated → sendMsgToActiveView("ViewFit")
TEST(CommandWiringTest, ViewFitAllDispatchesViewFit)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    auto* view = new MockMDIView(nullptr, &mw);
    mw.addWindow(view);

    StdCmdViewFitAll cmd;
    static_cast<Command&>(cmd).activated(0);  // FreeCAD: runCommand → pCmd->activated(iMsg)
    ASSERT_EQ(view->receivedMessages.size(), 1u);
    EXPECT_EQ(view->receivedMessages[0], "ViewFit");
}

// Ported from: FreeCAD StdCmdViewFront::activated → sendMsgToActiveView("ViewFront")
TEST(CommandWiringTest, ViewFrontDispatchesViewFront)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    auto* view = new MockMDIView(nullptr, &mw);
    mw.addWindow(view);

    StdCmdViewFront cmd;
    static_cast<Command&>(cmd).activated(0);
    ASSERT_EQ(view->receivedMessages.size(), 1u);
    EXPECT_EQ(view->receivedMessages[0], "ViewFront");
}

// Ported from: FreeCAD StdOrthographicCamera::activated → sendMsgToActiveView("OrthographicCamera")
TEST(CommandWiringTest, OrthographicCameraDispatchesMessage)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    auto* view = new MockMDIView(nullptr, &mw);
    mw.addWindow(view);

    StdOrthographicCamera cmd;
    static_cast<Command&>(cmd).activated(0);
    ASSERT_FALSE(view->receivedMessages.empty());
    EXPECT_EQ(view->receivedMessages[0], "OrthographicCamera");
}

// Ported from: FreeCAD isActive() → sendHasMsgToActiveView contract
TEST(CommandWiringTest, IsActiveFollowsSendHasMsg)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    auto* view = new MockMDIView(nullptr, &mw);
    view->supportedMessages.insert("ViewFit");
    mw.addWindow(view);

    StdCmdViewFitAll cmd;
    Command& cmdBase = cmd;
    EXPECT_TRUE(cmdBase.isActive());  // active view supports "ViewFit"

    view->supportedMessages.clear();
    EXPECT_FALSE(cmdBase.isActive());  // active view no longer supports it
}

// Ported from: FreeCAD command disabled when no active view
TEST(CommandWiringTest, ViewCommandInactiveWithNoView)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    StdCmdViewFitAll cmd;
    Command& cmdBase = cmd;
    EXPECT_FALSE(cmdBase.isActive());  // no active view → sendHasMsg returns false
}

// Ported from: FreeCAD StdCmdNew::activated → Application::newDocument (new view/document).
TEST(CommandWiringTest, StdCmdNewCallsNewDocument)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    int factoryCalls = 0;
    Application::Instance()->setNewViewFactory([&mw, &factoryCalls]() -> MDIView* {
        ++factoryCalls;
        return new MockMDIView(nullptr, &mw);
    });

    StdCmdNew cmd;
    // FreeCAD: CommandManager::runCommand → pCmd->activated(iMsg) through a Command* base
    // pointer (the override is protected in the derived class via DEF_STD_CMD).
    static_cast<Command&>(cmd).activated(0);

    EXPECT_EQ(factoryCalls, 1);             // newDocument invoked the factory
    EXPECT_NE(mw.activeWindow(), nullptr);  // and the factory's view was added + made active

    // Cleanup: reset factory so later tests don't get this lambda.
    Application::Instance()->setNewViewFactory(Application::NewViewFactory{});
}

// Ported from: FreeCAD StdCmdTileWindows::isActive (src/Gui/CommandWindow.cpp:66-69) and
//              StdCmdTileWindows::activated (src/Gui/CommandWindow.cpp:60-64).
// Characterization test: the Window-domain commands were wired by the MDI-chrome session
// (DTA CommandWindow.cpp:25-55) to route through MainWindow MDI slots. This confirms the
// registry-dispatch path (CommandManager::getCommandByName -> Command* base interface, the
// same path the menu/toolbar system uses). The StdCmdTileWindows class is macro-declared
// (DEF_STD_CMD_A) inside CommandWindow.cpp so it is not visible to the test translation unit;
// dispatching through Command* keeps the public base contract (activated/isActive are public
// on Command, protected on the derived class).
// Authored: no reference unit test exists in FreeCAD for StdCmdTileWindows::isActive
//           dispatch; FreeCAD exercises it only via full GUI runs. DTA test verifies the
//           registry-routed Command* path against the already-wired behavior.
TEST(CommandWiringTest, StdCmdTileWindowsIsActiveOnlyWithWindows)
{
    ensureAppReady();
    CommandManager mgr;
    createWindowCommands(mgr);
    Command* tile = mgr.getCommandByName("Std_TileWindows");
    ASSERT_NE(tile, nullptr);

    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    EXPECT_FALSE(tile->isActive());  // no MDI windows yet

    mw.addWindow(new MockMDIView(nullptr, &mw));
    EXPECT_TRUE(tile->isActive());   // window present
}

// Ported from: FreeCAD StdCmdTileWindows::activated → MainWindow::tile
//              (src/Gui/CommandWindow.cpp:60-64; DTA MainWindow.cpp:270-275).
// Characterization test: QMdiArea::tileSubWindows() rearranges geometry but preserves the
// sub-window count. Same registry-dispatch rationale as the test above.
// Authored: no reference unit test exists in FreeCAD for StdCmdTileWindows::activated
//           window-count preservation; DTA test verifies the already-wired behavior.
TEST(CommandWiringTest, StdCmdTileWindowsPreservesWindowCount)
{
    ensureAppReady();
    CommandManager mgr;
    createWindowCommands(mgr);
    Command* tile = mgr.getCommandByName("Std_TileWindows");
    ASSERT_NE(tile, nullptr);

    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    mw.addWindow(new MockMDIView(nullptr, &mw));
    mw.addWindow(new MockMDIView(nullptr, &mw));
    ASSERT_EQ(mw.mdiArea()->subWindowList().size(), 2);

    tile->activated(0);  // FreeCAD: CommandManager::runCommand → pCmd->activated(iMsg)
    EXPECT_EQ(mw.mdiArea()->subWindowList().size(), 2);  // tile rearranges, does not close
}

// Ported from: Authored — no reference unit test exists in FreeCAD for the "deferred command"
// concept. FreeCAD gates commands via onHasMsg on the active view; DTA instead marks commands
// whose backend lands in Step 3 (tool/dialog/view wiring) or Step 4 (file/element I/O) as
// explicitly inactive (isActive() → false) with a deferred-TODO annotation in activated().
// This test verifies the defer sweep via the same registry path the menu/toolbar system uses
// (CommandManager::getCommandByName → Command* base interface). The commands looked up are
// macro-declared (DEF_STD_CMD_A / DEF_STD_CMD) inside their respective Command*.cpp files
// and therefore not visible to the test translation unit by class name — registry lookup
// through the public Command* base is the only way to exercise them here.
TEST(CommandWiringTest, DeferredCommandsAreInactive)
{
    ensureAppReady();
    CommandManager mgr;
    createStdCommands(mgr);  // registers all domains (File/Edit/View/Std/Tools/Structure/Macro/Window)

    // Active view present — the ONLY reason these are inactive is "backend deferred".
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    mw.addWindow(new MockMDIView(nullptr, &mw));

    // View domain — Step 3 (view backend)
    Gui::Command* dim = mgr.getCommandByName("Std_ViewDimetric");
    ASSERT_NE(dim, nullptr);
    EXPECT_FALSE(dim->isActive());  // deferred → disabled

    // Visibly-enabled-but-dead View sweep (final-review I1): Std_ViewDrawStyle,
    // Std_ViewZoomIn, and the visibility cluster were pre-sweep stubs (activated { /* TODO */ }
    // + isActive true). They now follow the same defer pattern as Std_ViewDimetric.
    Gui::Command* ds = mgr.getCommandByName("Std_DrawStyle");
    ASSERT_NE(ds, nullptr);
    EXPECT_FALSE(ds->isActive());  // deferred → disabled

    Gui::Command* zi = mgr.getCommandByName("Std_ViewZoomIn");
    ASSERT_NE(zi, nullptr);
    EXPECT_FALSE(zi->isActive());  // deferred → disabled

    Gui::Command* tv = mgr.getCommandByName("Std_ToggleVisibility");
    ASSERT_NE(tv, nullptr);
    EXPECT_FALSE(tv->isActive());  // deferred → disabled

    // File / Edit domain — Step 4 (file/element I/O)
    Gui::Command* opn = mgr.getCommandByName("Std_Open");
    ASSERT_NE(opn, nullptr);
    EXPECT_FALSE(opn->isActive());

    Gui::Command* sel = mgr.getCommandByName("Std_SelectAll");
    ASSERT_NE(sel, nullptr);
    EXPECT_FALSE(sel->isActive());

    Gui::Command* sav = mgr.getCommandByName("Std_Save");
    ASSERT_NE(sav, nullptr);
    EXPECT_FALSE(sav->isActive());

    // Tools / Structure / Macro domain — Step 3+ (tool/dialog backend)
    Gui::Command* dlp = mgr.getCommandByName("Std_DlgParameter");
    ASSERT_NE(dlp, nullptr);
    EXPECT_FALSE(dlp->isActive());

    Gui::Command* part = mgr.getCommandByName("Std_Part");
    ASSERT_NE(part, nullptr);
    EXPECT_FALSE(part->isActive());

    Gui::Command* mac = mgr.getCommandByName("Std_DlgMacroExecute");
    ASSERT_NE(mac, nullptr);
    EXPECT_FALSE(mac->isActive());
}
