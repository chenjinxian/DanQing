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
    if (!imodel || imodel->GetSelectionSet().isEmpty()) {
        dqApp::NotifyMessageDetails details;
        details.briefMessage = "[ZOOM] no selected elements";
        dqApp::Application::Get().GetNotificationManager().OutputMessage(details);
        return true;
    }

    // 数据源：placement 回放表（getPlacements 的离线对应物）。非 dump
    // 连接 / 表未采集 → 报告（参考 getPlacements RPC 失败语义的宿主面）。
    auto* opened = dta::findOpenedDump(view3d);
    if (!opened || !opened->connection
        || opened->connection->getPlacements().empty()) {
        dqApp::NotifyMessageDetails details;
        details.briefMessage = "[ZOOM] placement data plane not captured for this model";
        dqApp::Application::Get().GetNotificationManager().OutputMessage(details);
        return true;
    }

    std::vector<uint64_t> ids;
    for (uint32_t id : imodel->GetSelectionSet().GetElements())
        ids.push_back(id);
    auto volume = computeSelectedVolume(*opened->connection, ids);
    if (!volume) {
        dqApp::NotifyMessageDetails details;
        details.briefMessage = "[ZOOM] selected elements have no placements";
        dqApp::Application::Get().GetNotificationManager().OutputMessage(details);
        return true;
    }

    // zoomToPlacements → lookAtVolume(volume, viewRect.aspect) +
    // synchWithView（DumpOpenHelper frameToWorldContent 同款取景模式——
    // LookAtVolume 在 ViewState3d）。
    auto* view3dState =
        vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
    if (!view3dState)
        return true;
    double const aspect = vp->viewRect().aspect();
    view3dState->LookAtVolume(*volume, &aspect);
    vp->InvalidateController();

    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "[ZOOM] volume=(%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f) elements=%zu",
                  volume->low.x, volume->low.y, volume->low.z,
                  volume->high.x, volume->high.y, volume->high.z, ids.size());
    dqApp::NotifyMessageDetails details;
    details.briefMessage = buf;
    dqApp::Application::Get().GetNotificationManager().OutputMessage(details);
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
