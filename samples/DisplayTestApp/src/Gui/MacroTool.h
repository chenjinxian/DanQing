// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Macro 播放器（keyin 序列）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/MacroTools.ts
//              （MacroTool :8-55）
//
// EQUIVALENCE（§11.10）：
//   - readExternalFile（:14 DtaRpcInterface 后端读）→ 本地 std::ifstream
//     （§8.2 零网络——OutputShadersTool/SavedViewsPanel 同款先例）。
//   - openMessageBox MediumAlert（:45）→ NotificationManager.OutputMessage
//     Info 告警（DanQing 无模态框面——错误分流文本 1:1 保留）。
#pragma once

#include <dqApp/ToolAdmin.h>

#include <string>
#include <vector>

namespace Gui {

class MacroTool final : public dqApp::InteractiveTool
{
public:
    // Ported from: MacroTool.toolId (:9).
    const char* getToolId() const override { return "Macro"; }
    // Ported from: MacroTool.minArgs/maxArgs (:10-11).
    int minArgs() const override { return 1; }
    int maxArgs() const override { return 1; }
    // keyin（参考无 locale keyin 键——类名注册面；DanQing 以 dta 域挂载）。
    std::string englishKeyin() const override { return "dta macro"; }

    // Ported from: MacroTool.run (:13-48 —— 读文件 + 分行 + 剔空 + 逐行
    // parseAndRun 三分支告警)。
    bool run() override;
    // Ported from: MacroTool.parseAndRun (:51-54).
    bool parseAndRun(std::vector<std::string> const& args) override;

    std::string m_macroFile;  // ← macroFile（args[0]）
};

}  // namespace Gui
