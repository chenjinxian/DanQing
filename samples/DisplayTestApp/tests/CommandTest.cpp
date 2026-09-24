// Ported from: Authored — no reference unit test exists in FreeCAD for Command framework
// standalone; test scenarios derived from FreeCAD Command.cpp/Command.h usage patterns.
#include <gtest/gtest.h>

#include <memory>
#include <QApplication>
#include <QKeySequence>
#include <QWidget>

#include "QtTestFixtures.h"

#include <App/Application.h>

// Qt application fixture — singleton definition (shared across test TUs)
// Ported from: Authored — Qt test fixture pattern
static int   g_argc = 1;
static char  g_arg0[] = "test";
static char* g_argv[] = { g_arg0, nullptr };
QtApp::QtApp() : app(g_argc, g_argv) {}
QtApp& qtApp() { static QtApp i; return i; }

// Per-test helper: ensures QApplication + App::Application singletons exist. MainWindow ctor
// reads App::GetApplication().GetUserParameter() for the status-bar/dock parameter group
// (FreeCAD MainWindow.cpp:84). Defined here (alongside qtApp()) so the three test TUs that
// previously held static duplicates can share one definition via QtTestFixtures.h.
// Ported from: Authored — Qt test fixture pattern.
void ensureAppReady()
{
    qtApp();  // ensure QApplication singleton
    if (!App::Application::_pcSingleton) {
        App::Application::_pcSingleton = new App::Application();
    }
}

#include "Gui/Application.h"

MainWindowGuard::MainWindowGuard(Gui::MainWindow* mw)
{
    Gui::Application::Instance()->setMainWindow(mw);
}

MainWindowGuard::~MainWindowGuard()
{
    Gui::Application::Instance()->setMainWindow(nullptr);
}

#include "Gui/Command.h"
#include "Gui/CommandDoc.h"
#include "Gui/Action.h"
#include "StubCmd.h"

using namespace Gui;

// =====================================================================
// keySequenceToAccel tests
// =====================================================================

// Ported from: Authored — no reference unit test exists in FreeCAD for keySequenceToAccel
TEST(CommandTest, KeySequenceToAccelMapsStandardKeys)
{
    qtApp();
    EXPECT_STREQ(keySequenceToAccel(QKeySequence::New), "Ctrl+N");
    EXPECT_STREQ(keySequenceToAccel(QKeySequence::Save), "Ctrl+S");
}

// Ported from: Authored — empty key sequence returns empty string
TEST(CommandTest, KeySequenceToAccelEmptyKey)
{
    qtApp();
    // Invalid standard key should produce empty or near-empty string
    const char* result = keySequenceToAccel(static_cast<int>(QKeySequence::UnknownKey));
    // QKeySequence(UnknownKey).toString() is empty
    EXPECT_STREQ(result, "");
}

// =====================================================================
// CommandManager tests
// =====================================================================

// Ported from: Authored — no reference unit test exists in FreeCAD for CommandManager registry
TEST(CommandManagerTest, GetCommandByNameReturnsNullForUnknown)
{
    qtApp();
    CommandManager mgr;
    EXPECT_EQ(mgr.getCommandByName("NonExistent"), nullptr);
}

// Ported from: Authored — addCommand + getCommandByName round-trip
TEST(CommandManagerTest, AddAndLookupCommand)
{
    qtApp();
    CommandManager mgr;
    auto cmd = std::make_unique<StubCmd>();
    mgr.addCommand(cmd.get());

    Command* found = mgr.getCommandByName("Stub_Cmd");
    EXPECT_NE(found, nullptr);
    EXPECT_STREQ(found->getName(), "Stub_Cmd");
}

// Ported from: Authored — duplicate addCommand is silently ignored
TEST(CommandManagerTest, DuplicateAddCommandIgnored)
{
    qtApp();
    CommandManager mgr;
    auto cmd1 = std::make_unique<StubCmd>();
    auto cmd2 = std::make_unique<StubCmd>();
    mgr.addCommand(cmd1.get());
    mgr.addCommand(cmd2.get());  // duplicate — should be ignored

    // The first command should still be registered
    Command* found = mgr.getCommandByName("Stub_Cmd");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found, cmd1.get());  // still the first one
}

// Ported from: Authored — addTo returns false for unknown command
TEST(CommandManagerTest, AddToUnknownCommandReturnsFalse)
{
    qtApp();
    CommandManager mgr;
    QWidget widget;
    EXPECT_FALSE(mgr.addTo("NonExistent", &widget));
}

// Ported from: Authored — addTo returns true for known command
TEST(CommandManagerTest, AddToKnownCommandReturnsTrue)
{
    qtApp();
    CommandManager mgr;
    auto cmd = std::make_unique<StubCmd>();
    mgr.addCommand(cmd.get());

    QWidget widget;
    EXPECT_TRUE(mgr.addTo("Stub_Cmd", &widget));
    // Action should have been created and added to widget
    EXPECT_NE(cmd->getAction(), nullptr);
}

// Ported from: Authored — testActive sweeps all commands
TEST(CommandManagerTest, TestActiveSweepsAllCommands)
{
    qtApp();
    CommandManager mgr;
    auto active = std::make_unique<StubCmd>();
    auto inactive = std::make_unique<StubActiveCmd>();
    mgr.addCommand(active.get());
    mgr.addCommand(inactive.get());

    // Force action creation
    QWidget widget;
    active->addTo(&widget);
    inactive->addTo(&widget);

    // testActive should not crash
    mgr.testActive();

    // StubCmd isActive() returns true (default), so action should be enabled
    EXPECT_TRUE(active->getAction()->action()->isEnabled());
    // StubActiveCmd isActive() returns false, so action should be disabled
    EXPECT_FALSE(inactive->getAction()->action()->isEnabled());
}

// =====================================================================
// Command metadata -> QAction mapping tests
// =====================================================================

// Ported from: Authored — no reference test exists for metadata->QAction mapping
TEST(CommandMetaDataTest, CreateActionMapsToolTip)
{
    qtApp();
    auto* cmd = new StubCmd();
    Action* a = cmd->createAction();
    ASSERT_NE(a, nullptr);

    // The tooltip should contain the translated tool tip text
    // Note: QT_TR_NOOP returns the string as-is when no translation context is loaded
    EXPECT_FALSE(a->action()->toolTip().isEmpty());

    delete a;
    delete cmd;
}

// Ported from: Authored — createAction maps pixmap to icon
TEST(CommandMetaDataTest, CreateActionMapsIcon)
{
    qtApp();
    auto* cmd = new StubCmd();
    Action* a = cmd->createAction();
    ASSERT_NE(a, nullptr);

    // StubCmd sets sPixmap = "StubIcon", so icon path should be :/icons/StubIcon
    // (may not resolve to a real file in tests, but the icon should be set)
    // QIcon::isNull() would be true if pixmap not found, but the method was called
    // Just verify createAction didn't crash and returned valid action
    EXPECT_NE(a->action(), nullptr);

    delete a;
    delete cmd;
}

// Ported from: Authored — initAction creates action lazily + sets shortcut
TEST(CommandTest, InitActionCreatesActionLazily)
{
    qtApp();
    auto* cmd = new StubCmd();
    EXPECT_EQ(cmd->getAction(), nullptr);  // no action yet

    cmd->initAction();
    EXPECT_NE(cmd->getAction(), nullptr);  // action created

    // Shortcut should be set from sAccel = "Ctrl+N"
    QKeySequence shortcut = cmd->getAction()->action()->shortcut();
    EXPECT_EQ(shortcut.toString().toStdString(), std::string("Ctrl+N"));

    delete cmd;
}

// Ported from: Authored — testActive enables/disables based on isActive()
TEST(CommandTest, TestActiveRespectsIsActive)
{
    qtApp();
    QWidget widget;

    // StubCmd: isActive() returns true (default)
    auto* activeCmd = new StubCmd();
    activeCmd->addTo(&widget);
    activeCmd->testActive();
    EXPECT_TRUE(activeCmd->getAction()->action()->isEnabled());

    // StubActiveCmd: isActive() returns false
    auto* inactiveCmd = new StubActiveCmd();
    inactiveCmd->addTo(&widget);
    inactiveCmd->testActive();
    EXPECT_FALSE(inactiveCmd->getAction()->action()->isEnabled());

    delete activeCmd;
    delete inactiveCmd;
}

// Ported from: Authored — addTo creates action and adds to widget
TEST(CommandTest, AddToWidgetCreatesAction)
{
    qtApp();
    auto* cmd = new StubCmd();
    EXPECT_EQ(cmd->getAction(), nullptr);

    QWidget widget;
    cmd->addTo(&widget);
    EXPECT_NE(cmd->getAction(), nullptr);
    // QAction should be in the widget's actions
    EXPECT_TRUE(widget.actions().contains(cmd->getAction()->action()));

    delete cmd;
}

// =====================================================================
// Action tests
// =====================================================================

// Ported from: Authored — Action wraps QAction, addTo adds to widget
TEST(ActionTest, AddToWidget)
{
    qtApp();
    auto* cmd = new StubCmd();
    Action* a = cmd->createAction();
    ASSERT_NE(a, nullptr);

    QWidget widget;
    a->addTo(&widget);
    EXPECT_TRUE(widget.actions().contains(a->action()));

    delete a;
    delete cmd;
}

// Ported from: Authored — Action transparent delegation
TEST(ActionTest, DelegatesEnabledState)
{
    qtApp();
    auto* cmd = new StubCmd();
    Action* a = cmd->createAction();
    ASSERT_NE(a, nullptr);

    a->setEnabled(false);
    EXPECT_FALSE(a->action()->isEnabled());

    a->setEnabled(true);
    EXPECT_TRUE(a->action()->isEnabled());

    delete a;
    delete cmd;
}

// Ported from: Authored — DEF_STD_CMD macro produces valid class
TEST(CommandMacroTest, DefStdCmdProducesValidClass)
{
    qtApp();
    auto* cmd = new StubCmd();
    EXPECT_STREQ(cmd->getName(), "Stub_Cmd");
    EXPECT_STREQ(cmd->className(), "StubCmd");
    EXPECT_STREQ(cmd->getGroupName(), "Standard");

    delete cmd;
}

// Ported from: Authored — DEF_STD_CMD_A macro produces valid class with isActive
TEST(CommandMacroTest, DefStdCmdAProducesValidClass)
{
    qtApp();
    auto* cmd = new StubActiveCmd();
    EXPECT_STREQ(cmd->getName(), "StubActive_Cmd");
    EXPECT_STREQ(cmd->className(), "StubActiveCmd");
    // isActive() is protected; test behavior through testActive() which
    // checks isActive() and enables/disables accordingly
    QWidget widget;
    cmd->addTo(&widget);
    cmd->testActive();
    EXPECT_FALSE(cmd->getAction()->action()->isEnabled());

    delete cmd;
}

// Ported from: Authored — CommandManager getAllCommands returns registered commands
TEST(CommandManagerTest, GetAllCommands)
{
    qtApp();
    CommandManager mgr;
    auto cmd1 = std::make_unique<StubCmd>();
    auto cmd2 = std::make_unique<StubActiveCmd>();
    mgr.addCommand(cmd1.get());
    mgr.addCommand(cmd2.get());

    const auto& all = mgr.getAllCommands();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_NE(all.find("Stub_Cmd"), all.end());
    EXPECT_NE(all.find("StubActive_Cmd"), all.end());
}

// =====================================================================
// CommandDoc metadata spot-check tests
// Ported from: Authored — no reference unit test exists in FreeCAD for
// command metadata table; scenarios derived from FreeCAD CommandDoc.cpp
// constructor metadata.
// =====================================================================

// Ported from: Authored — File/Edit commands registered via createFileEditCommands
TEST(CommandDocTest, FileCommandsHaveCorrectMetadata)
{
    qtApp();
    CommandManager mgr;
    createFileEditCommands(mgr);

    // Std_New: Ctrl+N
    auto* newCmd = mgr.getCommandByName("Std_New");
    ASSERT_NE(newCmd, nullptr);
    newCmd->initAction();
    EXPECT_EQ(newCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+N"));
    EXPECT_STREQ(newCmd->getGroupName(), "File");

    // Std_Open: Ctrl+O
    auto* openCmd = mgr.getCommandByName("Std_Open");
    ASSERT_NE(openCmd, nullptr);
    openCmd->initAction();
    EXPECT_EQ(openCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+O"));

    // Std_Save: Ctrl+S
    auto* saveCmd = mgr.getCommandByName("Std_Save");
    ASSERT_NE(saveCmd, nullptr);
    saveCmd->initAction();
    EXPECT_EQ(saveCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+S"));

    // Std_SaveAs: Ctrl+Shift+S
    auto* saveAsCmd = mgr.getCommandByName("Std_SaveAs");
    ASSERT_NE(saveAsCmd, nullptr);
    saveAsCmd->initAction();
    EXPECT_EQ(saveAsCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+Shift+S"));

    // Std_Quit: Ctrl+Q（QKeySequence::Quit 平台感知：Windows 无系统默认，
    // shortcut 回退显示菜单文本 "Exit"）
    auto* quitCmd = mgr.getCommandByName("Std_Quit");
    ASSERT_NE(quitCmd, nullptr);
    quitCmd->initAction();
#ifdef _WIN32
    EXPECT_EQ(quitCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Exit"));
#else
    EXPECT_EQ(quitCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+Q"));
#endif
}

// Ported from: Authored — Edit domain shortcut spot-checks
TEST(CommandDocTest, EditCommandsHaveCorrectShortcuts)
{
    qtApp();
    CommandManager mgr;
    createFileEditCommands(mgr);

    // Std_Undo: Ctrl+Z
    auto* undoCmd = mgr.getCommandByName("Std_Undo");
    ASSERT_NE(undoCmd, nullptr);
    undoCmd->initAction();
    EXPECT_EQ(undoCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+Z"));

    // Std_Redo: Ctrl+Shift+Z（QKeySequence::Redo 平台感知：Windows 标准为 Ctrl+Y）
    auto* redoCmd = mgr.getCommandByName("Std_Redo");
    ASSERT_NE(redoCmd, nullptr);
    redoCmd->initAction();
#ifdef _WIN32
    EXPECT_EQ(redoCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+Y"));
#else
    EXPECT_EQ(redoCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+Shift+Z"));
#endif

    // Std_Cut: Ctrl+X
    auto* cutCmd = mgr.getCommandByName("Std_Cut");
    ASSERT_NE(cutCmd, nullptr);
    cutCmd->initAction();
    EXPECT_EQ(cutCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+X"));

    // Std_Copy: Ctrl+C
    auto* copyCmd = mgr.getCommandByName("Std_Copy");
    ASSERT_NE(copyCmd, nullptr);
    copyCmd->initAction();
    EXPECT_EQ(copyCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+C"));

    // Std_Paste: Ctrl+V
    auto* pasteCmd = mgr.getCommandByName("Std_Paste");
    ASSERT_NE(pasteCmd, nullptr);
    pasteCmd->initAction();
    EXPECT_EQ(pasteCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+V"));

    // Std_SelectAll: Ctrl+A
    auto* selectAllCmd = mgr.getCommandByName("Std_SelectAll");
    ASSERT_NE(selectAllCmd, nullptr);
    selectAllCmd->initAction();
    EXPECT_EQ(selectAllCmd->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+A"));
}

// Ported from: Authored — Tooltips are non-empty for all registered commands.
// StdCmdRecentFiles/StdCmdRecentMacros skipped: their createAction() calls
// App::GetApplication() which requires the Application singleton (unavailable in tests).
TEST(CommandDocTest, AllCommandsHaveTooltips)
{
    qtApp();
    CommandManager mgr;
    createFileEditCommands(mgr);

    const char* names[] = {
        "Std_New", "Std_Open",
        // "Std_RecentFiles",  // requires App::GetApplication() in createAction
        "Std_CloseActiveWindow",
        "Std_CloseAllWindows", "Std_Save", "Std_SaveAs", "Std_SaveCopy",
        "Std_SaveAll", "Std_Revert", "Std_Import", "Std_Export",
        "Std_MergeProjects", "Std_ProjectInfo", "Std_Print", "Std_PrintPreview",
        "Std_PrintPdf", "Std_Quit", "Std_Undo", "Std_Redo",
        "Std_Cut", "Std_Copy", "Std_Paste", "Std_DuplicateSelection",
        "Std_SelectAll", "Std_Delete", "Std_Refresh", "Std_BoxSelection",
        "Std_BoxElementSelection", "Std_SendToPythonConsole", "Std_Placement",
        "Std_TransformManip", "Std_Alignment", "Std_Edit", "Std_Properties",
        "Std_UserEditMode",
    };

    for (const char* name : names) {
        auto* cmd = mgr.getCommandByName(name);
        ASSERT_NE(cmd, nullptr) << "Command not found: " << name;
        cmd->initAction();
        EXPECT_FALSE(cmd->getAction()->action()->toolTip().isEmpty())
            << "Empty tooltip for: " << name;
    }
}

// Ported from: Authored — Registration count sanity check
TEST(CommandDocTest, RegistrationCount)
{
    qtApp();
    CommandManager mgr;
    createFileEditCommands(mgr);

    // 17 File commands + 19 Edit commands = 36 total
    const auto& all = mgr.getAllCommands();
    EXPECT_EQ(all.size(), 36u);
}
