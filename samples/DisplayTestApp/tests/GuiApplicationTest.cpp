// Ported from: Authored — no reference test exists in FreeCAD for MainWindow::activeWindow
// (FreeCAD tracks active view via Gui::Application singleton + viewActivated signal; DTA
//  exposes the same concept on MainWindow for the dispatch facade.)
#include <gtest/gtest.h>
#include "QtTestFixtures.h"

#include <App/Application.h>

#include "Gui/MainWindow.h"
#include "Gui/MDIView.h"
#include "Gui/Application.h"
#include "MockMDIView.h"

using namespace Gui;

// Ported from: Authored — no reference test exists in FreeCAD for activeWindow with no views
TEST(GuiApplicationTest, ActiveWindowReturnsNullWithNoViews)
{
    ensureAppReady();
    MainWindow mw;
    EXPECT_EQ(mw.activeWindow(), nullptr);
}

// Ported from: Authored — no reference test exists in FreeCAD for addWindow-activates-view
TEST(GuiApplicationTest, AddWindowMakesItActive)
{
    ensureAppReady();
    MainWindow mw;
    auto* view = new MockMDIView(nullptr, &mw);
    mw.addWindow(view);
    EXPECT_EQ(mw.activeWindow(), view);
}

// Ported from: FreeCAD src/Gui/Application.cpp:1469-1481 (sendMsg/sendHasMsg contract)
TEST(GuiApplicationTest, SendMsgReturnsFalseWithNoActiveView)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    EXPECT_FALSE(Application::Instance()->sendMsgToActiveView("ViewFit"));
    EXPECT_FALSE(Application::Instance()->sendHasMsgToActiveView("ViewFit"));
}

// Ported from: FreeCAD src/Gui/Application.cpp:1469-1475
TEST(GuiApplicationTest, SendMsgRoutesToActiveViewOnMsg)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    auto* view = new MockMDIView(nullptr, &mw);
    view->handlesMessages = true;
    mw.addWindow(view);

    bool ok = Application::Instance()->sendMsgToActiveView("ViewFront");
    EXPECT_TRUE(ok);
    ASSERT_EQ(view->receivedMessages.size(), 1u);
    EXPECT_EQ(view->receivedMessages[0], "ViewFront");
}

// Ported from: FreeCAD src/Gui/Application.cpp:1477-1481
TEST(GuiApplicationTest, SendHasMsgMirrorsViewOnHasMsg)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    auto* view = new MockMDIView(nullptr, &mw);
    view->supportedMessages.insert("ViewFit");
    mw.addWindow(view);

    EXPECT_TRUE(Application::Instance()->sendHasMsgToActiveView("ViewFit"));
    EXPECT_FALSE(Application::Instance()->sendHasMsgToActiveView("ViewFront"));
}

// Ported from: FreeCAD Application::activeView delegates to MainWindow::activeWindow
TEST(GuiApplicationTest, ActiveViewDelegatesToMainWindow)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    EXPECT_EQ(Application::Instance()->activeView(), nullptr);
    auto* view = new MockMDIView(nullptr, &mw);
    mw.addWindow(view);
    EXPECT_EQ(Application::Instance()->activeView(), view);
}

// Ported from: FreeCAD Application::newDocument — DTA uses an injectable view factory so the
// command layer (compiled into the test target) never references View3DInventor (real dqApp).
TEST(GuiApplicationTest, NewDocumentUsesFactoryAndAddsWindow)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    Application::Instance()->setNewViewFactory([&mw]() -> MDIView* {
        return new MockMDIView(nullptr, &mw);
    });

    MDIView* v = Application::Instance()->newDocument();
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(mw.activeWindow(), v);  // addWindow was called → it is active
}

// Ported from: Authored — newDocument with no factory is a graceful no-op
TEST(GuiApplicationTest, NewDocumentNoopWithoutFactory)
{
    ensureAppReady();
    MainWindow mw;
    MainWindowGuard mwGuard(&mw);
    Application::Instance()->setNewViewFactory({});
    EXPECT_EQ(Application::Instance()->newDocument(), nullptr);
}

// Ported from: Authored — getGuiApplication returns the singleton
TEST(GuiApplicationTest, GetGuiApplicationReturnsSingleton)
{
    ensureAppReady();
    EXPECT_EQ(getGuiApplication(), Application::Instance());
}
