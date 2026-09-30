// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — SetActiveSnapMode tool（M-M(6) 接线级）。
// Ported from: itwinjs-core display-test-app App.ts:486-489
//              `public static setActiveSnapMode(snap: SnapMode): void {
//                 this.setActiveSnapModes([snap]); }` + AccuSnap 子类的
//              setActiveSnapModes/setSnapModeOverride 面（App.ts:76-107——
//              DisplayTestAppAccuSnap 持 _activeSnaps 数组，keyin 无对应物：
//              DTA 的 snap 模式面经 UI 设置与 DrawingAidTestTool 快捷键，
//              本工具以 keyin 形态对齐同一引擎通道）。
// 语义：设置 AccuSnap 的活跃 snap 模式（单模式——参考 setActiveSnapMode 的
// [snap] 单元素数组形态）；无参数 = 恢复默认 NearestKeypoint（App.ts:76
// _activeSnaps 初值）。clear() 联动（App.ts:107 setSnapModeOverride 尾部
// `IModelApp.accuSnap.clear()`——挂起的 snap 状态失效）。
#pragma once

#include <dqApp/AccuSnap.h>
#include <dqApp/ToolAdmin.h>

#include <optional>
#include <string>
#include <vector>

namespace Gui {

class SetActiveSnapModeTool final : public dqApp::InteractiveTool
{
public:
    const char* getToolId() const override { return "SetActiveSnapMode"; }
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 1; }
    // SVTTools.json "tools.SetActiveSnapMode.keyin" 形态（dta 命名空间——
    // 与 SaveImage/RecordFps 同族）。
    std::string englishKeyin() const override { return "dta snapmode"; }

    bool run() override;
    bool parseAndRun(std::vector<std::string> const& args) override;

    // 模式名解析（keyin 实参 → SnapMode 位）。参考 SnapMode 枚举名
    // （HitDetail.ts:22-32）1:1；未知名 → nullopt（run 报错——参考
    // parseArgs 的 unknown-value 行为）。
    static std::optional<dqApp::SnapMode> parseMode(std::string const& name);

private:
    std::optional<dqApp::SnapMode> m_mode;
};

}  // namespace Gui
