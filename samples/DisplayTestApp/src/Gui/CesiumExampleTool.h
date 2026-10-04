// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Cesium 陈列馆入口工具（keyin "dta cesium example"）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/EmptyExample.ts
//              （:14-48 的 start 入口 + onModelClose 清理；参考经 Start 页
//               "Cesium" 卡片启动——DanQing 以 keyin 挂载[EQUIVALENCE：卡片
//               启停面与 keyin 同语义]）。
#pragma once

#include <dqApp/ToolAdmin.h>

namespace Gui {

class CesiumExampleTool final : public dqApp::InteractiveTool
{
public:
    const char* getToolId() const override { return "CesiumExample"; }
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 0; }
    std::string englishKeyin() const override { return "dta cesium example"; }

    // Ported from: EmptyExample.ts:14-48（start 装饰器 + onModelClose 时 stop；
    // DanQing 形态 = toggle——已挂则摘）。
    bool run() override;
};

}  // namespace Gui
