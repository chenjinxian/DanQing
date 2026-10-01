// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — ChangeGridSettingsTool 实现（M-O(1) I5）。
// Ported from: display-test-app Grid.ts:16-93（run 的逐参 if 门 +
// parseAndRun 的 s/r/g/o/l 参数面——o 的 0..4 枚举映射 1:1）。
#include "GridSettingsTool.h"

#include <dqApp/Application.h>
#include <dqApp/ParseArgs.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/GridOrientationType.h>

namespace Gui {

bool ChangeGridSettingsTool::run()
{
    // Grid.ts:17-18——无活动视口即 false。
    auto* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    if (!vp || !vp->GetView())
        return false;
    auto* v3 = vp->GetView()->AsViewState3d();
    if (!v3)
        return false;

    // Grid.ts:21-31——spacing/ratio/gridsPerRef/orientation 逐参写
    //（ratio 分支：y = 当前 x * ratio，:25）。DanQing 的写通道是合并 setter
    // setGridSettings(orientation, spacing, gridsPerRef)——先读现值再并参
    //（离散 getter：getGridOrientation/getGridsPerRef/getGridSpacing）。
    bool changed = false;
    dqCommon::GridOrientationType orientation = v3->getGridOrientation();
    dqGeom::Point2d spacing = v3->getGridSpacing();
    int32_t gridsPerRef = v3->getGridsPerRef();
    if (m_spacing) {
        spacing = dqGeom::Point2d{*m_spacing, *m_spacing};
        changed = true;
    }
    if (m_ratio) {
        spacing = dqGeom::Point2d{spacing.x, spacing.x * *m_ratio};
        changed = true;
    }
    if (m_gridsPerRef) {
        gridsPerRef = *m_gridsPerRef;
        changed = true;
    }
    if (m_orientation) {
        orientation = *m_orientation;
        changed = true;
    }
    if (changed)
        v3->setGridSettings(orientation, spacing, gridsPerRef);

    // Grid.ts:33-34——gridLock（ToolAdmin 公有字段；参考消费者 AccuDraw 锁定
    // 坐标未移植，字段随本工具先立）。
    if (m_lock)
        dqApp::Application::Get().GetToolAdmin().setGridLock(*m_lock);

    // Grid.ts:36——清缓存的 grid 装饰。
    vp->InvalidateScene();
    return true;
}

bool ChangeGridSettingsTool::parseAndRun(std::vector<std::string> const& args)
{
    // Grid.ts:46-93——s/r/g/o/l（parseArgs 名值对；o 的 0..4 →
    // GridOrientationType 逐项映射 :68-85；l 走 getBoolean）。
    auto const opts = dqApp::parseArgs(args);
    if (auto v = opts.getFloat("s"))
        m_spacing = v;
    if (auto v = opts.getFloat("r"))
        m_ratio = v;
    if (auto v = opts.getInteger("g"))
        m_gridsPerRef = v;
    if (auto v = opts.getInteger("o")) {
        switch (*v) {
        case 0: m_orientation = dqCommon::GridOrientationType::View; break;
        case 1: m_orientation = dqCommon::GridOrientationType::WorldXY; break;
        case 2: m_orientation = dqCommon::GridOrientationType::WorldYZ; break;
        case 3: m_orientation = dqCommon::GridOrientationType::WorldXZ; break;
        case 4: m_orientation = dqCommon::GridOrientationType::AuxCoord; break;
        default: break;   // 参考越界值不映射（switch 无命中即 undefined）
        }
    }
    if (auto v = opts.getBoolean("l"))
        m_lock = v;
    return run();
}

}  // namespace Gui
