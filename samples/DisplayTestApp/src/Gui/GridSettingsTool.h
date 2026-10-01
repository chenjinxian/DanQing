// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — ChangeGridSettingsTool（M-O(1) I5 接线级）。
// Ported from: itwinjs-core display-test-app Grid.ts:11-94
//              （toolId "GridSettings"、keyin "dta grid settings"
//              [SVTTools.json]、minArgs 0 / maxArgs 4、s/r/g/o/l 参数面、
//              run 末尾 invalidateScene 清缓存的 grid 装饰）。
// 引擎面：ViewState3d setGridSettings/getGridSpacing/getGridsPerRef/
// getGridOrientation（ViewState.ts:953-1006 的 DanQing 移植，GridSettingsTest
// 全绿）+ ToolAdmin.gridLock（本项随工具移植的字段——参考消费者 AccuDraw
// 锁定坐标未移植，字段先立）。
#pragma once

#include <dqApp/ToolAdmin.h>
#include <dqCommon/GridOrientationType.h>

#include <optional>
#include <string>
#include <vector>

namespace Gui {

class ChangeGridSettingsTool final : public dqApp::InteractiveTool
{
public:
    const char* getToolId() const override { return "GridSettings"; }
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 4; }
    // SVTTools.json "tools.GridSettings.keyin"。
    std::string englishKeyin() const override { return "dta grid settings"; }

    bool run() override;
    bool parseAndRun(std::vector<std::string> const& args) override;

private:
    // Grid.ts run(spacing, ratio, gridsPerRef, orientation, lock) 的实参态
    //（undefined = 未指定即不动该字段——参考逐参 if 门）。
    std::optional<double> m_spacing;
    std::optional<double> m_ratio;
    std::optional<int> m_gridsPerRef;
    std::optional<dqCommon::GridOrientationType> m_orientation;
    std::optional<bool> m_lock;
};

}  // namespace Gui
