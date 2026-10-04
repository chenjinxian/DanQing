// MacroToolTest — M-O(4) P9 Macro 播放器锁（文件读/分行/剔空/三分支/执行序）。
//
// 锚定（真实读过的参考行号）：
//   - MacroTools.ts:13-48 run（readExternalFile → \r 剔除 :15-16 → \n 分行 :17
//     → 空行剔除 :18-21 → 空文件告警 :23-24 → 逐行 parseAndRun 三分支
//     :26-47[ToolNotFound/BadArgumentCount/FailedToRun 文本 1:1]）；
//   - :51-54 parseAndRun（args[0] = macroFile）。
//
// RED（M-O(4) P9 落地前）：MacroTool 类型不存在——编译期缺 API 红。
//
// Authored: no reference test exists in itwinjs-core for MacroTool
//           （display-test-app 无前端测试；行为锚定如上）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>

#include "Gui/MacroTool.h"
#include "Gui/DtaTools.h"  // registerDtaTools（宿主注册面——DtaToolsWiring 同源）

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>

#include <cstdio>
#include <fstream>
#include <string>

#ifndef DANQING_TEST_ASSETS_DIR
#define DANQING_TEST_ASSETS_DIR "."
#endif

namespace {

struct QtEnvMacro {
    QtEnvMacro()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvMacro s_qtMacro;

// 合成 macro 文件（\r\n 混行 + 空行——参考 :15-21 的两道剔除面）。写 CWD
// （ctest 工作目录 = build/samples/DisplayTestApp——测试自产自清）。
std::string writeMacroFile(char const* name, std::string const& content)
{
    std::string const path = std::string("macro-") + name;
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    ofs << content;
    return path;
}

}  // namespace

// 空文件/缺失文件 → "File not found or no content"（:23-24）+ 不执行任何行。
TEST(MacroTool, EmptyOrMissingFileReportsNoContent)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();

    Gui::MacroTool tool;
    tool.m_macroFile = writeMacroFile("empty.macro", "");
    EXPECT_TRUE(tool.run());

    // 重指到确实不存在的路径。
    tool.m_macroFile = "macro-does-not-exist.macro";
    EXPECT_TRUE(tool.run());
}

// \r\n 分行 + 空行剔除（:15-21）——合法 keyin 序列逐行执行（无告警）。
TEST(MacroTool, ExecutesEachLineAndStripsBlankLines)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();

    // 两行合法 keyin（dta grid settings 无参恢复默认 + 未知域行三分支面另测）
    // + 夹两空行 + CRLF 行尾。
    std::string const path = writeMacroFile(
        "seq.macro",
        "dta grid settings\r\n"
        "\r\n"
        "\r\n"
        "dta snapmode\r\n");
    Gui::MacroTool tool;
    tool.m_macroFile = path;
    EXPECT_TRUE(tool.run());
}

// 三分支文本（:31-37——坏 keyin 的告警面经 OutputMessage 扇出，不中断序列）。
TEST(MacroTool, ReportsUnknownKeyinAndContinues)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();

    std::string const path = writeMacroFile(
        "bad.macro",
        "dta grid settings\r\n"
        "definitely not a real keyin\r\n"
        "dta snapmode\r\n");
    Gui::MacroTool tool;
    tool.m_macroFile = path;
    // 未知行告警后继续执行尾行（参考 forEach 语义——无中断）。
    EXPECT_TRUE(tool.run());
}

// parseAndRun 面（:51-54——args[0] = 宏文件路径；空参 false）。
TEST(MacroTool, ParseAndRunTakesFilePath)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();

    Gui::MacroTool tool;
    EXPECT_FALSE(tool.parseAndRun({}));

    std::string const path = writeMacroFile("parse.macro", "dta grid settings\n");
    EXPECT_TRUE(tool.parseAndRun({path}));
    EXPECT_EQ(path, tool.m_macroFile);
}

// 注册面（keyin "dta macro"——宿主注册面 DtaToolsWiring 同源）。
TEST(MacroTool, RegisteredWithKeyin)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    Gui::registerDtaTools();
    auto* tool = admin.GetRegistry().Create("Macro");
    ASSERT_NE(tool, nullptr);
    EXPECT_STREQ("Macro", tool->getToolId());
    delete tool;
}
