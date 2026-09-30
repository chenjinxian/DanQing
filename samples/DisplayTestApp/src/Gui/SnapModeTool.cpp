// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — SetActiveSnapMode tool 实现（M-M(6) 接线级）。
// Ported from: display-test-app App.ts:486-489（setActiveSnapMode →
//              setActiveSnapModes([snap])）+ AccuSnap 子类写通道。
#include "SnapModeTool.h"

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>

#include <cstdio>
#include <string>

namespace Gui {

std::optional<dqApp::SnapMode> SetActiveSnapModeTool::parseMode(std::string const& name)
{
    // SnapMode 枚举名 1:1（HitDetail.ts:22-32）。
    if (name == "Nearest") return dqApp::SnapMode::Nearest;
    if (name == "NearestKeypoint") return dqApp::SnapMode::NearestKeypoint;
    if (name == "MidPoint") return dqApp::SnapMode::MidPoint;
    if (name == "Center") return dqApp::SnapMode::Center;
    if (name == "Origin") return dqApp::SnapMode::Origin;
    if (name == "Bisector") return dqApp::SnapMode::Bisector;
    if (name == "Intersection") return dqApp::SnapMode::Intersection;
    if (name == "PerpendicularPoint") return dqApp::SnapMode::PerpendicularPoint;
    if (name == "TangentPoint") return dqApp::SnapMode::TangentPoint;
    return std::nullopt;
}

bool SetActiveSnapModeTool::run()
{
    // 无实参 → 恢复默认 NearestKeypoint（App.ts:76 _activeSnaps 初值）。
    auto const mode = m_mode.value_or(dqApp::SnapMode::NearestKeypoint);
    dqApp::Application::Get().GetAccuSnap().setActiveSnapMode(mode);
    int n = 0;
    auto const* active = dqApp::Application::Get().GetAccuSnap().getActiveSnapModes(n);
    char buf[96];
    std::snprintf(buf, sizeof(buf), "[SNAP] active snap mode = 0x%x",
                  n > 0 ? static_cast<unsigned>(*active) : 0u);
    // 输出走 NotificationManager（M-L(3) 接通的输出通道——测试经
    // OnMessageOutput 断言）。
    dqApp::NotifyMessageDetails details;
    details.briefMessage = buf;
    dqApp::Application::Get().GetNotificationManager().OutputMessage(details);
    return true;
}

bool SetActiveSnapModeTool::parseAndRun(std::vector<std::string> const& args)
{
    if (!args.empty()) {
        m_mode = parseMode(args[0]);
        if (!m_mode) {
            dqApp::NotifyMessageDetails details;
            details.briefMessage = "[SNAP] unknown snap mode '" + args[0]
                + "' (Nearest/NearestKeypoint/MidPoint/Center/Origin/Bisector/"
                  "Intersection/PerpendicularPoint/TangentPoint)";
            dqApp::Application::Get().GetNotificationManager().OutputMessage(details);
            return false;
        }
    }
    return run();
}

}  // namespace Gui
