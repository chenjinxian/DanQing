// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Macro 播放器实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/MacroTools.ts
#include "MacroTool.h"

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ToolAdmin.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Gui {

// 读文件全文（readExternalFile 的本地等价——EQUIVALENCE 见头文件）。
static std::string readMacroFile(std::string const& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Ported from: MacroTool.run (:13-48).
bool MacroTool::run()
{
    std::string const macroString = readMacroFile(m_macroFile);
    // :15-16 剔除 \r。
    std::string macroStr2;
    macroStr2.reserve(macroString.size());
    for (char c : macroString)
        if (c != '\r')
            macroStr2.push_back(c);

    // :17-21 \n 分行 + 空行剔除（参考 forEach+splice 的同语义——重建等价）。
    std::vector<std::string> commands;
    {
        std::istringstream stream(macroStr2);
        std::string line;
        while (std::getline(stream, line))
            if (!line.empty())
                commands.push_back(line);
    }

    auto& notify = dqApp::Application::Get().GetNotificationManager();
    if (commands.empty()) {
        // :23-24 "File not found or no content"。
        notify.OutputMessage(dqApp::NotifyMessageDetails(
            dqApp::OutputMessagePriority::Info,
            "File not found or no content"));
        return true;
    }

    // :26-47 逐行 parseAndRun 三分支告警（MismatchedQuotes 为 DanQing 域扩——
    // 参考无此值，落入 default 无告警[同参考 default]）。
    for (auto const& cmd : commands) {
        std::string message;
        switch (dqApp::Application::Get().GetToolAdmin()
                     .GetRegistry()
                     .parseAndRun(cmd)) {
            case dqApp::ParseAndRunResult::ToolNotFound:
                message = "Cannot find a key-in that matches: " + cmd;
                break;
            case dqApp::ParseAndRunResult::BadArgumentCount:
                message = "Incorrect number of arguments for: " + cmd;
                break;
            case dqApp::ParseAndRunResult::FailedToRun:
                message = "Key-in failed to run: " + cmd;
                break;
            default:
                break;
        }
        // :44-45 openMessageBox MediumAlert → OutputMessage（EQUIVALENCE 头文件）。
        if (!message.empty())
            notify.OutputMessage(dqApp::NotifyMessageDetails(
                dqApp::OutputMessagePriority::Info, message));
    }
    return true;
}

// Ported from: MacroTool.parseAndRun (:51-54).
bool MacroTool::parseAndRun(std::vector<std::string> const& args)
{
    if (args.empty())
        return false;
    m_macroFile = args[0];
    return run();
}

}  // namespace Gui
