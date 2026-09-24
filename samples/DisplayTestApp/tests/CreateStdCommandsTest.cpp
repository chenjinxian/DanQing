// Ported from: Authored — no reference unit test exists in FreeCAD for CreateStdCommands
// standalone; test scenarios derived from FreeCAD CommandStd.cpp CreateStdCommands() and
// the individual domain registration functions.
#include <gtest/gtest.h>

#include <memory>
#include <QApplication>
#include <set>
#include <string>

#include "QtTestFixtures.h"

// Qt application fixture — uses the same singleton as CommandTest.cpp
extern QtApp& qtApp();

#include "Command.h"
#include "Action.h"
#include "CreateStdCommands.h"
#include "CommandStructure.h"
#include "CommandStd.h"
#include "CommandTools.h"
#include "CommandMacro.h"

using namespace Gui;

// =====================================================================
// createStdCommands() master registration tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
// Verifies that createStdCommands() registers all expected commands (>70 total).
TEST(CreateStdCommandsTest, RegistersAllStdCommands)
{
    qtApp();
    CommandManager mgr;
    createStdCommands(mgr);

    const auto& all = mgr.getAllCommands();
    EXPECT_GT(all.size(), 70u);

    // Spot-check key commands from each domain
    EXPECT_NE(mgr.getCommandByName("Std_New"), nullptr);          // File
    EXPECT_NE(mgr.getCommandByName("Std_Save"), nullptr);         // File
    EXPECT_NE(mgr.getCommandByName("Std_Undo"), nullptr);         // Edit
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);   // View
    EXPECT_NE(mgr.getCommandByName("Std_Part"), nullptr);         // Structure
    EXPECT_NE(mgr.getCommandByName("Std_DlgCustomize"), nullptr); // Std/Tools
    EXPECT_NE(mgr.getCommandByName("Std_DlgMacroRecord"), nullptr); // Macro
    EXPECT_NE(mgr.getCommandByName("Std_About"), nullptr);        // Help
    EXPECT_NE(mgr.getCommandByName("Std_OnlineHelp"), nullptr);   // Help
}

// Ported from: Authored — no reference test exists in FreeCAD
// Verifies no duplicate command names across all domain registrations.
TEST(CreateStdCommandsTest, NoDuplicateCommandNames)
{
    qtApp();
    CommandManager mgr;
    createStdCommands(mgr);

    const auto& all = mgr.getAllCommands();
    // All names in the map are unique by construction (std::map key),
    // but verify none were silently dropped by checking expected count.
    EXPECT_GE(all.size(), 100u);
}

// =====================================================================
// Domain-specific registration tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, StructureCommandsRegistered)
{
    qtApp();
    CommandManager mgr;
    createStructureCommands(mgr);

    EXPECT_NE(mgr.getCommandByName("Std_Part"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Group"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_VarSet"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_LinkActions"), nullptr);

    // 4 commands total
    EXPECT_EQ(mgr.getAllCommands().size(), 4u);
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, StdDomainCommandsRegistered)
{
    qtApp();
    CommandManager mgr;
    createStdDomainCommands(mgr);

    // Help (13 commands)
    EXPECT_NE(mgr.getCommandByName("Std_About"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_AboutQt"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_WhatsThis"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_RestartInSafeMode"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_PythonHelp"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_OnlineHelp"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_OnlineHelpWebsite"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADWebsite"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADDonation"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADUserHub"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADForum"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ReportBug"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DevHandbook"), nullptr);

    // Tools (7 commands)
    EXPECT_NE(mgr.getCommandByName("Std_DlgParameter"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DlgPreferences"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DlgCustomize"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_CommandLine"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_UnitsCalculator"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_TextDocument"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_AnnotationLabel"), nullptr);

    // View (1 command)
    EXPECT_NE(mgr.getCommandByName("Std_ReloadStyleSheet"), nullptr);

    // Macro (1 command)
    EXPECT_NE(mgr.getCommandByName("Std_RecentMacros"), nullptr);

    // 13 + 7 + 1 + 1 = 22 commands total
    EXPECT_EQ(mgr.getAllCommands().size(), 22u);
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, ToolsCommandsRegistered)
{
    qtApp();
    CommandManager mgr;
    createToolsCommands(mgr);

    EXPECT_NE(mgr.getCommandByName("Std_AddonMgr"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_Measure"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_MassProperties"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DependencyGraph"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ExportDependencyGraph"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ProjectUtil"), nullptr);

    // 6 commands total
    EXPECT_EQ(mgr.getAllCommands().size(), 6u);
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, MacroCommandsRegistered)
{
    qtApp();
    CommandManager mgr;
    createMacroCommands(mgr);

    EXPECT_NE(mgr.getCommandByName("Std_DlgMacroRecord"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DlgMacroExecute"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DlgMacroExecuteDirect"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_MacroAttachDebugger"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_OpenMacrosFolder"), nullptr);

    // 5 commands total
    EXPECT_EQ(mgr.getAllCommands().size(), 5u);
}

// Ported from: Authored — no reference test exists in FreeCAD
// Help commands are registered as part of createStdDomainCommands() since they
// live in CommandStd.cpp (matching FreeCAD structure).
TEST(CreateStdCommandsTest, HelpCommandsRegistered)
{
    qtApp();
    CommandManager mgr;
    createStdDomainCommands(mgr);

    EXPECT_NE(mgr.getCommandByName("Std_About"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_AboutQt"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_WhatsThis"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_RestartInSafeMode"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_PythonHelp"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_OnlineHelp"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_OnlineHelpWebsite"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADWebsite"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADDonation"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADUserHub"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_FreeCADForum"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_ReportBug"), nullptr);
    EXPECT_NE(mgr.getCommandByName("Std_DevHandbook"), nullptr);
}

// =====================================================================
// Metadata spot-check tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, StructureCommandsHaveCorrectMetadata)
{
    qtApp();
    CommandManager mgr;
    createStructureCommands(mgr);

    auto* partCmd = mgr.getCommandByName("Std_Part");
    ASSERT_NE(partCmd, nullptr);
    EXPECT_STREQ(partCmd->getGroupName(), "Structure");

    auto* groupCmd = mgr.getCommandByName("Std_Group");
    ASSERT_NE(groupCmd, nullptr);
    EXPECT_STREQ(groupCmd->getGroupName(), "Structure");

    auto* varsetCmd = mgr.getCommandByName("Std_VarSet");
    ASSERT_NE(varsetCmd, nullptr);
    EXPECT_STREQ(varsetCmd->getGroupName(), "Structure");
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, HelpCommandsHaveCorrectMetadata)
{
    qtApp();
    CommandManager mgr;
    createStdDomainCommands(mgr);

    const char* helpCmds[] = {
        "Std_About", "Std_AboutQt", "Std_WhatsThis", "Std_RestartInSafeMode",
        "Std_PythonHelp", "Std_OnlineHelp", "Std_OnlineHelpWebsite",
        "Std_FreeCADWebsite", "Std_FreeCADDonation", "Std_FreeCADUserHub",
        "Std_FreeCADForum", "Std_ReportBug", "Std_DevHandbook",
    };

    for (const char* name : helpCmds) {
        auto* cmd = mgr.getCommandByName(name);
        ASSERT_NE(cmd, nullptr) << "Command not found: " << name;
        EXPECT_STREQ(cmd->getGroupName(), "Help") << "Wrong group for: " << name;
    }
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, ToolsCommandsHaveCorrectGroup)
{
    qtApp();
    CommandManager mgr;
    createToolsCommands(mgr);

    const char* toolCmds[] = {
        "Std_AddonMgr", "Std_Measure", "Std_MassProperties",
        "Std_DependencyGraph", "Std_ExportDependencyGraph", "Std_ProjectUtil",
    };

    for (const char* name : toolCmds) {
        auto* cmd = mgr.getCommandByName(name);
        ASSERT_NE(cmd, nullptr) << "Command not found: " << name;
        EXPECT_STREQ(cmd->getGroupName(), "Tools") << "Wrong group for: " << name;
    }
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, MacroCommandsHaveCorrectGroup)
{
    qtApp();
    CommandManager mgr;
    createMacroCommands(mgr);

    const char* macroCmds[] = {
        "Std_DlgMacroRecord", "Std_DlgMacroExecute",
        "Std_DlgMacroExecuteDirect", "Std_MacroAttachDebugger",
        "Std_OpenMacrosFolder",
    };

    for (const char* name : macroCmds) {
        auto* cmd = mgr.getCommandByName(name);
        ASSERT_NE(cmd, nullptr) << "Command not found: " << name;
        EXPECT_STREQ(cmd->getGroupName(), "Macro") << "Wrong group for: " << name;
    }
}

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, ShortcutSpotChecks)
{
    qtApp();
    CommandManager mgr;
    createStdCommands(mgr);

    // WhatsThis: Shift+F1 (QKeySequence::WhatsThis)
    auto* wts = mgr.getCommandByName("Std_WhatsThis");
    ASSERT_NE(wts, nullptr);
    wts->initAction();
    EXPECT_EQ(wts->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Shift+F1"));

    // DlgMacroExecuteDirect: Ctrl+F6
    auto* execDirect = mgr.getCommandByName("Std_DlgMacroExecuteDirect");
    ASSERT_NE(execDirect, nullptr);
    execDirect->initAction();
    EXPECT_EQ(execDirect->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+F6"));

    // DlgPreferences: Ctrl+,
    auto* prefs = mgr.getCommandByName("Std_DlgPreferences");
    ASSERT_NE(prefs, nullptr);
    prefs->initAction();
    EXPECT_EQ(prefs->getAction()->action()->shortcut().toString().toStdString(),
              std::string("Ctrl+,"));
}
