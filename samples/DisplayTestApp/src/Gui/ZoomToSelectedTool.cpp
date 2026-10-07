// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — ZoomToSelectedElements 工具实现（M-N(2)）。
#include "ZoomToSelectedTool.h"

#include "../DumpOpenHelper.h"
#include "MainWindow.h"
#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/tile/DumpIModelConnection.h>

#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/YawPitchRollAngles.h>

#include <cstdio>
#include <string>

namespace Gui {

namespace {

// Placement 世界域：elementAligned bbox 八角经 YPR 旋转 + origin 平移。
// Ported from: itwinjs-core Placement3d 语义（zoomToPlacements :2246-2270
// 的 placement.getRange() 消费——八角展开是 ElementAlignedBox3d 变换的
// 直接表达）。
void extendWorldRange(dqApp::DumpIModelConnection::PlacementInfo const& p,
                      dqGeom::Range3d& out)
{
    // Placement.isValid（Placement.ts:87/:144——`!bbox.isNull &&
    // max(origin.maxAbs(), bbox.maxAbs()) < Constant.circumferenceOfEarth`；
    // Viewport.ts:2246-2248 的 filter(x => x.isValid) 面。2026-10-07 审计
    // S-6：无效 placement 参与并集会撑大取景域——过滤。
    constexpr double kCircumferenceOfEarth = 40075.0 * 1000.0;  // Constant.ts:26——40075 km
    {
        bool bboxNull = true;
        double bboxMaxAbs = 0.0;
        for (int i = 0; i < 3; ++i) {
            if (p.bboxHigh[i] > p.bboxLow[i])
                bboxNull = false;
            bboxMaxAbs = std::max(bboxMaxAbs, std::max(std::abs(p.bboxLow[i]),
                                                       std::abs(p.bboxHigh[i])));
        }
        double originMaxAbs = std::max({std::abs(p.origin[0]),
                                        std::abs(p.origin[1]),
                                        std::abs(p.origin[2])});
        if (bboxNull || std::max(originMaxAbs, bboxMaxAbs) >= kCircumferenceOfEarth)
            return;
    }
    auto const rot = dqGeom::YawPitchRollAngles::CreateDegrees(
                         p.angles[0], p.angles[1], p.angles[2])
                         .ToMatrix3d();
    for (int corner = 0; corner < 8; ++corner) {
        double const local[3] = {
            (corner & 1) ? p.bboxHigh[0] : p.bboxLow[0],
            (corner & 2) ? p.bboxHigh[1] : p.bboxLow[1],
            (corner & 4) ? p.bboxHigh[2] : p.bboxLow[2],
        };
        double const world[3] = {
            rot.coffs[0] * local[0] + rot.coffs[1] * local[1] + rot.coffs[2] * local[2]
                + p.origin[0],
            rot.coffs[3] * local[0] + rot.coffs[4] * local[1] + rot.coffs[5] * local[2]
                + p.origin[1],
            rot.coffs[6] * local[0] + rot.coffs[7] * local[1] + rot.coffs[8] * local[2]
                + p.origin[2],
        };
        out.ExtendPoint(dqGeom::Point3d::From(world[0], world[1], world[2]));
    }
}

}  // namespace

std::optional<dqGeom::Range3d> ZoomToSelectedElementsTool::computeSelectedVolume(
    dqApp::DumpIModelConnection& connection,
    std::vector<uint64_t> const& selectedElementIds)
{
    if (selectedElementIds.empty())
        return std::nullopt;
    dqGeom::Range3d volume = dqGeom::Range3d::CreateNull();
    for (uint64_t id : selectedElementIds) {
        auto const* placement =
            connection.findPlacement(dqBase::DqId(id));
        if (placement)
            extendWorldRange(*placement, volume);
    }
    if (volume.isNull())
        return std::nullopt;
    return volume;
}

bool ZoomToSelectedElementsTool::run()
{
    auto* mw = MainWindow::getInstance();
    auto* view3d = mw ? qobject_cast<View3DInventor*>(mw->activeWindow()) : nullptr;
    auto* vp = view3d ? view3d->getUeViewport() : nullptr;
    if (!vp)
        return true;

    // Viewer.ts:43-45：`vp.iModel.selectionSet.elements` 空选集 → no-op。
    auto* imodel = vp->GetIModel();
    if (!imodel || imodel->GetSelectionSet().isEmpty())
        return true;   // Viewer.ts:43-45——空选集静默 no-op（2026-10-07 审计 B6：参考无通知面）

    // 数据源：placement 回放表（getPlacements 的离线对应物）。非 dump
    // 连接 / 表未采集 → 报告（参考 getPlacements RPC 失败语义的宿主面）。
    auto* opened = dta::findOpenedDump(view3d);
    if (!opened || !opened->connection
        || opened->connection->getPlacements().empty())
        return true;   // getPlacements RPC 失败/缺席 → 静默 no-op（Viewer.ts:42-46 同款）

    std::vector<uint64_t> ids;
    for (uint32_t id : imodel->GetSelectionSet().GetElements())
        ids.push_back(id);
    auto volume = computeSelectedVolume(*opened->connection, ids);
    if (!volume)
        return true;

    // zoomToPlacements → lookAtVolume(volume, viewRect.aspect) +
    // synchWithView（DumpOpenHelper frameToWorldContent 同款取景模式——
    // LookAtVolume 在 ViewState3d）。
    auto* view3dState =
        vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
    if (!view3dState)
        return true;
    double const aspect = vp->viewRect().aspect();
    view3dState->LookAtVolume(*volume, &aspect);
    // Viewport.ts:2274-2275 —— lookAtViewAlignedVolume 尾随 synchWithView：
    // ScreenViewport.synchWithView（:3591-3596）saveViewUndo + Invalidate-
    // Controller（2026-10-07 审计 B6：原裸 InvalidateController——不存 undo
    // 条目；动画面 animateFrustumChange 未移植——终态 1:1）。
    vp->synchWithView();
    return true;
}

bool ZoomToSelectedElementsTool::parseAndRun(std::vector<std::string> const&)
{
    // margin/padding 实参面（Viewer.ts:57-83）依赖 MarginOptions 引擎面
    // （lookAtVolume 的 marginPercent——lookAtVolume 当前无该参），登记
    // 后续；keyin 无参直接 run。
    return run();
}

}  // namespace Gui
