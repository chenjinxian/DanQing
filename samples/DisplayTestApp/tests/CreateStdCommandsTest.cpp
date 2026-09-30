// Ported from: Authored — no reference unit test exists in FreeCAD for CreateStdCommands
// standalone; test scenarios derived from FreeCAD CommandStd.cpp CreateStdCommands() and
// the individual domain registration functions.
// M-L(2)：Structure/Tools/Macro 域与 Help 链接命令族已删（分析报告 §3.1/§3.5
// 按域量化——注册 ~110、真功能 ~25），对应域测试随删；保留域的断言按删后
// 实态更新（File=3 / View=14 / Std=1(AboutQt) / Window=6，合计 24）。
#include <gtest/gtest.h>

#include <QApplication>
#include <string>

#include "QtTestFixtures.h"

// Qt application fixture — uses the same singleton as CommandTest.cpp
extern QtApp& qtApp();

#include "Command.h"
#include "CreateStdCommands.h"
#include "CommandStd.h"

using namespace Gui;

// =====================================================================
// createStdCommands() master registration tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
// Verifies that createStdCommands() registers the surviving commands (24 total).
TEST(CreateStdCommandsTest, RegistersAllStdCommands)
{
    qtApp();
    CommandManager mgr;
    createStdCommands(mgr);

    const auto& all = mgr.getAllCommands();
    EXPECT_EQ(all.size(), 24u);

    // Spot-check key commands from each surviving domain
    EXPECT_NE(mgr.getCommandByName("Std_New"), nullptr);            // File
    EXPECT_NE(mgr.getCommandByName("Std_Import"), nullptr);         // File
    EXPECT_NE(mgr.getCommandByName("Std_Quit"), nullptr);           // File
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);     // View
    EXPECT_NE(mgr.getCommandByName("Std_ViewFront"), nullptr);      // View
    EXPECT_NE(mgr.getCommandByName("Std_AboutQt"), nullptr);        // Tools
    EXPECT_NE(mgr.getCommandByName("Std_TileWindows"), nullptr);    // Window
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
    EXPECT_EQ(all.size(), 24u);
}

// =====================================================================
// Domain-specific registration tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
// M-L(2)：Std 域仅存 Std_AboutQt（原 Help 域唯一真功能项，归 Tools）。
TEST(CreateStdCommandsTest, StdDomainCommandsRegistered)
{
    qtApp();
    CommandManager mgr;
    createStdDomainCommands(mgr);

    EXPECT_NE(mgr.getCommandByName("Std_AboutQt"), nullptr);

    // 1 command total
    EXPECT_EQ(mgr.getAllCommands().size(), 1u);
}

// =====================================================================
// Metadata spot-check tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
TEST(CreateStdCommandsTest, StdDomainCommandsHaveCorrectMetadata)
{
    qtApp();
    CommandManager mgr;
    createStdDomainCommands(mgr);

    auto* cmd = mgr.getCommandByName("Std_AboutQt");
    ASSERT_NE(cmd, nullptr);
    // M-L(2)：原 Help 域唯一真功能项归 Tools（Help 菜单整体删除）
    EXPECT_STREQ(cmd->getGroupName(), "Tools");
    EXPECT_EQ(std::string(cmd->getMenuText()), std::string("About &Qt"));
}
