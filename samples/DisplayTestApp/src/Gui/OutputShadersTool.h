// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — OutputShaders 工具（keyin "dta output shaders"）
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/OutputShadersTool.ts
//              （OutputShadersTool :322-367 + outputShaders :295-320 +
//               skipThisShader :289-293 + makeShadeBat 常量 :10-287）
//
// 参数面（parseAndRun :341-366）：单 token 标志 c（先 compileAllShaders）/
// u/n（isUsed 过滤）/ v/f（vert/frag 过滤）/ g/h（GL/HLSL 过滤——DanQing
// GL-only，h 语义保留[全 isGL=true → 过滤全空]）；`d=目录` 指定输出目录
//（含反斜杠/斜杠补尾）；缺省目录 "d:\temp\shaders\"（:365 逐字）。
//
// EQUIVALENCE（§11.10）：
//   - 写出面：参考 DtaRpcInterface.writeExternalFile（后端文件写）；DanQing
//     §8.2 零网络——本地 ofstream 直写。验证法 = OutputShadersTest 落盘锁
//     （_makeShadeBat/_OutputList.txt/逐 .glsl 文件数对账）。
//   - makeShadeBat：参考常量逐字移植（fxc/HLSL 批处理——纯字符串落盘、
//     工具不执行它；DanQing GL-only 后端无 fxc 域，该文件仅作参考侧工件
//     保真）。GL 源文件头注释（entry.isGL 时不加 "// filename isUsed" 前缀）
//     1:1（:315）。
#pragma once

#include <dqApp/ToolAdmin.h>

#include <string>
#include <vector>

namespace dqRender {
struct DebugShaderFile;
}

namespace Gui {

class OutputShadersTool final : public dqApp::InteractiveTool
{
public:
    // Ported from: OutputShadersTool.toolId/minArgs/maxArgs (:323-325).
    const char* getToolId() const override { return "OutputShaders"; }
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 2; }
    // keyin 面（SVTTools.json "OutputShaders"——DanQing 以 dta 域挂注册）。
    std::string englishKeyin() const override { return "dta output shaders"; }

    // Ported from: OutputShadersTool.run (:327-339).
    bool run() override;
    // Ported from: OutputShadersTool.parseAndRun (:341-366).
    bool parseAndRun(std::vector<std::string> const& args) override;

    // Ported from: skipThisShader (:289-293——六向过滤谓词；公开供锁直驱)。
    static bool skipThisShader(dqRender::DebugShaderFile const& entry,
                               std::string const& usedFlag,
                               std::string const& typeFlag,
                               std::string const& langFlag);

    // 参数面（parseAndRun 设置；测试可直驱）。
    bool m_compile = false;        // ← compile（token 含 c）
    std::string m_usedFlag;        // ← usedFlag（u/n）
    std::string m_typeFlag;        // ← typeFlag（v/f）
    std::string m_langFlag;        // ← langFlag（g/h）
    std::string m_outputDir;       // ← outputDir（d=；缺省 d:\temp\shaders\）
};

}  // namespace Gui
