// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Rendering Style 14 预设表
// Ported from: itwinjs-core test-apps/display-test-app ViewAttributes.ts
//              renderingStyleViewFlags (:31-42) + renderingStyles (:44-268)
//              + applyRenderingStyle (:283-286)
//
// M-Q Q-b。EQUIVALENCE / 裁决（逐条；验证法 = RenderingStylesTest 表完整性锁）：
//  - Atmosphere 项的 atmosphere.display（:260-266）——DanQing EnvironmentProps
//    无 atmosphere 面（大气层装饰未移植）——sky/ground display 两段照落，
//    atmosphere 段不移植（➖ 登记，计划文档 E2）。
//  - AO/Thematic 项的视觉面——viewflags 位与 ao/thematic props 数据面 1:1
//    写入；对应渲染 pass 未移植（视觉无效果——计划文档 E2 登记）。
//  - Sun-dappled 的 shadows viewflag（solarShadows 字段未解析——数据面
//    viewflags 位照落，视觉无效——计划文档 E3 登记）。
#pragma once

#include <dqCommon/DisplayStyleSettings.h>

#include <cstddef>
#include <string>
#include <vector>

namespace dqApp {
class Viewport;
}

#ifndef BEGIN_DQ_GUI_NS
#define BEGIN_DQ_GUI_NS namespace Gui {
#define END_DQ_GUI_NS }
#endif

BEGIN_DQ_GUI_NS

// RenderingStyle = DisplayStyle3dSettingsProps + name
// （ViewAttributes.ts:27-29 interface RenderingStyle）。
struct RenderingStyle {
    std::string name;
    dqCommon::DisplayStyle3dSettingsProps props;
};

// 14 预设（ViewAttributes.ts:44-268 逐字段 1:1——顺序即参考数组序）。
std::vector<RenderingStyle> const& renderingStyles();

// applyRenderingStyle（ViewAttributes.ts:283-286）——None no-op，其余
// vp.overrideDisplayStyle(style)。返回是否应用（None=false）。
bool applyRenderingStyle(class dqApp::Viewport& vp, size_t index);

END_DQ_GUI_NS
