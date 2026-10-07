// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — ZoomToSelectedElements 工具（M-N(2)）。
// Ported from: itwinjs-core display-test-app Viewer.ts:42-47
//              zoomToSelectedElements（`vp.iModel.selectionSet.elements` →
//              `vp.zoomToElements(elems, {animateFrustumChange:true,...})`）
//              + Viewer.ts:49-89 ZoomToSelectedElementsTool（toolId/
//              parseAndRun 的 margin/padding 实参面——接线子集：核心 run
//              路径；margin/padding 需 MarginOptions 引擎面，登记后续）。
// 引擎通道：Viewport.zoomToElements（:2301-2310）= elements.getPlacements
// (ids) → zoomToPlacements（:2245-2270 = 有效 placement 的世界域并集 →
// lookAtVolume + synchWithView）。DanQing 离线对应物：placements.json
// 回放表（DumpIModelConnection::getPlacements——M-N(2) 采集面）+
// LookAtVolume + InvalidateController（DumpOpenHelper frameToWorldContent
// 同款取景模式）。
// 世界域计算 = Placement 语义（origin + YPR×elementAlignedBox——
// Placement3d：bbox 八角经 YawPitchRollAngles.toMatrix3d 旋转后平移）。
#pragma once

#include <dqApp/ToolAdmin.h>

#include <optional>
#include <vector>

namespace dqApp {
class Viewport;
class DumpIModelConnection;
}

namespace Gui {

class ZoomToSelectedElementsTool final : public dqApp::InteractiveTool
{
public:
    const char* getToolId() const override { return "ZoomToSelectedElements"; }
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 4; }
    std::string englishKeyin() const override { return "dta zoom selected"; }

    bool run() override;
    bool parseAndRun(std::vector<std::string> const& args) override;

    // 核心：选中元素集 → 域并集（zoomToPlacements 的 placements→volume 步
    // ——静态化供测试直接驱动）。空选集/全无 placement → 空域。
    // viewRotation 非空 = Viewport.ts:2266-2269 的视空间逐角变换（union-
    // (rotate)，返回视轴对齐域）；空 = 世界域（测试静态直驱语义，审计 S-5）。
    static std::optional<dqGeom::Range3d> computeSelectedVolume(
        dqApp::DumpIModelConnection& connection,
        std::vector<uint64_t> const& selectedElementIds,
        dqGeom::Matrix3d const* viewRotation = nullptr);
};

}  // namespace Gui
