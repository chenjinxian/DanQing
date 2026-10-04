// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — glTF decoration tool (keyin-driven)
// Ported from: itwinjs-core display-test-app GltfDecoration.ts:103-228
//              (GltfDecorationTool — toolId "AddGltfDecoration",
//              minArgs 0 / maxArgs 6, parseAndRun 参数面 + run 体).
//
// 参数面（:118-124）：`u>` URL（**省略——§8.2 零网络**；DanQing 无 URL 取数，
// 本地文件选择为 queryAsset 的无-url 分支等价物，:136-146）/`i>` 实例数 /
// `s>` 随机缩放 / `c>` 随机逐实例色 / `r>` 随机旋转 / `f>` 强制非实例化 /
// `w>` useViewportRenderMode（DanQing 无 RenderMode 覆盖面——参数消费面保留
// 登记，EQUIVALENCE：参考该参数只影响纹理采样的模态开关，DanQing glTF 装饰
// 路径恒 SmoothShade 语义）。
#pragma once

#include <dqApp/ToolAdmin.h>

#include <dqGeom/Transform.h>  // createGltfInstanceTransform 返回形

#include <memory>
#include <string>
#include <vector>

namespace dqApp {
class Viewport;
}
namespace dqRender {
struct GltfScene;
}

namespace Gui {

class GltfDecorationTool final : public dqApp::InteractiveTool
{
public:
    // Ported from: GltfDecorationTool.toolId (GltfDecoration.ts:104).
    const char* getToolId() const override { return "AddGltfDecoration"; }
    // Ported from: GltfDecorationTool.minArgs / maxArgs (GltfDecoration.ts:105-106).
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 6; }
    // keyin 面——SVTTools.json 无该工具的英文 keyin 键（keyin 可达性经
    // 工具类名注册面，App.ts 扫描）；DanQing 以 "dta gltf" 挂注册。
    std::string englishKeyin() const override { return "dta gltf"; }

    // Ported from: GltfDecorationTool.run (GltfDecoration.ts:149-227).
    bool run() override;
    // Ported from: GltfDecorationTool.parseAndRun (GltfDecoration.ts:116-127).
    bool parseAndRun(std::vector<std::string> const& args) override;

    // Test seam: force-instanced / force-uninstanced builder halves and the
    // transform/instancing factories factored for direct drive.
    // Ported from: GltfDecoration.ts:41-67 (createTransform——随机位置 + 可选
    // 缩放/旋转；maxExtent = projectExtents diagonal 的最小分量 :74-75）。
    static dqGeom::Transform createGltfInstanceTransform(
        double maxExtent, bool wantScale, bool wantRotate);

    // Ported from: GltfDecoration.ts:69-98 (createInstances)+ :180-195 的
    // 实例化装配半边——逐实例烘 createTransform 变换 + c> 七色循环；CPU 展开
    // 形态的 EQUIVALENCE 见实现注。测试缝（像素锁直接驱动——maxExtent 由
    // 调用方按 :74-75 计算或按数据面给定；seed=0 非确定性[参考 Math.random]，
    // 固定种子供确定性布局断言）。
    static std::unique_ptr<dqRender::GltfScene> buildInstancedScene(
        dqRender::GltfScene const& src, int numInstances, double maxExtent,
        bool wantScale, bool wantColor, bool wantRotate, unsigned seed = 0);

    // Parameters set by parseAndRun (tests may set them directly).
    std::string m_url;         // ← _url（无 URL 取数——本地文件选择触发）
    int m_numInstances = 1;    // ← _numInstances（i>）
    bool m_wantScale = false;  // ← _wantScale（s>）
    bool m_wantColor = false;  // ← _wantColor（c>）
    bool m_wantRotate = false; // ← _wantRotate（r>）
    bool m_forceUninstanced = false;  // ← _forceUninstanced（f>）
};

}  // namespace Gui
