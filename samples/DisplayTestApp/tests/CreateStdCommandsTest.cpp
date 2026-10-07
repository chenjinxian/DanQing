// Ported from: Authored — no reference unit test exists in FreeCAD for CreateStdCommands
// standalone; test scenarios derived from FreeCAD CommandStd.cpp CreateStdCommands() and
// the individual domain registration functions.
// M-L(2)：Structure/Tools/Macro 域与 Help 链接命令族已删（分析报告 §3.1/§3.5
// 按域量化——注册 ~110、真功能 ~25），对应域测试随删；保留域的断言按删后
// 实态更新。2026-10-07 删除侧：Std 域（Std_AboutQt——DTA 无 About 对话框）与
// Std_Windows 存根再清（File=3 / View=14 / Window=5，合计 22）。
#include <gtest/gtest.h>

#include <QApplication>
#include <string>

#include "QtTestFixtures.h"

// Qt application fixture — uses the same singleton as CommandTest.cpp
extern QtApp& qtApp();

#include "Command.h"
#include "CreateStdCommands.h"

using namespace Gui;

// =====================================================================
// createStdCommands() master registration tests
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD
// Verifies that createStdCommands() registers the surviving commands (22 total).
TEST(CreateStdCommandsTest, RegistersAllStdCommands)
{
    qtApp();
    CommandManager mgr;
    createStdCommands(mgr);

    const auto& all = mgr.getAllCommands();
    EXPECT_EQ(all.size(), 22u);

    // Spot-check key commands from each surviving domain
    EXPECT_NE(mgr.getCommandByName("Std_New"), nullptr);            // File
    EXPECT_NE(mgr.getCommandByName("Std_Import"), nullptr);         // File
    EXPECT_NE(mgr.getCommandByName("Std_Quit"), nullptr);           // File
    EXPECT_NE(mgr.getCommandByName("Std_ViewFitAll"), nullptr);     // View
    EXPECT_NE(mgr.getCommandByName("Std_ViewFront"), nullptr);      // View
    // Std_AboutQt 2026-10-07 删除侧已删（CommandStd.cpp/.h 整删——DTA 无 About）
    EXPECT_EQ(mgr.getCommandByName("Std_AboutQt"), nullptr);
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
    EXPECT_EQ(all.size(), 22u);
}

// Std 域注册/元数据测试已删（2026-10-07 删除侧——createStdDomainCommands
// 随 CommandStd.cpp/.h 整删，DTA 无 About 对应对话框）。
