// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — viewport synchronization tools implementation
// Ported from: itwinjs-core display-test-app SyncViewportsTool.ts
#include "SyncViewportsTool.h"

#include <QMetaObject>

#include <algorithm>

#include <dqApp/Application.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

namespace Gui {

namespace {
dqApp::Viewport* viewportById(int id)
{
    auto& viewMgr = dqApp::Application::Get().GetViewManager();
    for (size_t i = 0; i < viewMgr.GetViewportCount(); ++i) {
        if (auto* vp = viewMgr.GetViewport(i);
            vp && vp->GetViewportId() == id)
            return vp;
    }
    return nullptr;
}
}  // namespace

SyncViewportsTool::State& SyncViewportsTool::state()
{
    // Ported from: SyncViewportsTool._state (SyncViewportsTool.ts:38) — tool-class
    // static.
    static State s_state;
    return s_state;
}

bool SyncViewportsTool::State::equals(dqApp::Viewport* const* vps, int count) const
{
    if (static_cast<int>(viewportIds.size()) != count)
        return false;
    std::vector<int> ids;
    ids.reserve(viewportIds.size());
    for (int i = 0; i < count; ++i)
        ids.push_back(vps[i]->GetViewportId());
    std::sort(ids.begin(), ids.end());
    return ids == viewportIds;
}

bool SyncViewportsTool::run(dqApp::Viewport* const* vps, int count)
{
    // Ported from: SyncViewportsTool.run (SyncViewportsTool.ts:43-56).
    if (nullptr == vps || count < 2) {
        disconnect();
    } else {
        if (state().equals(vps, count))
            disconnect();
        else
            connect(vps, count, syncFactory());
    }
    return true;
}

bool SyncViewportsTool::parseAndRun(std::vector<std::string> const& args)
{
    // Ported from: SyncViewportsTool.parseAndRun (SyncViewportsTool.ts:58-73).
    if (args.empty())
        return run();

    if (1 == args.size()) {
        if (0 == QString::fromStdString(args[0]).compare(QLatin1String("all"), Qt::CaseInsensitive)) {
            auto& viewMgr = dqApp::Application::Get().GetViewManager();
            std::vector<dqApp::Viewport*> all;
            for (size_t i = 0; i < viewMgr.GetViewportCount(); ++i)
                if (auto* vp = viewMgr.GetViewport(i))
                    all.push_back(vp);
            return run(all.data(), static_cast<int>(all.size()));
        }
        return false;
    }

    std::vector<dqApp::Viewport*> vps;
    for (std::string const& arg : args) {
        bool ok = false;
        int const vpId = QString::fromStdString(arg).toInt(&ok, 10);
        if (!ok)
            return false;
        dqApp::Viewport* vp = viewportById(vpId);
        if (!vp)
            return false;
        vps.push_back(vp);
    }

    return run(vps.data(), static_cast<int>(vps.size()));
}

dqApp::SynchronizeViewportsFactory SyncViewportsTool::syncFactory() const
{
    // Ported from: SyncViewportsTool.connect —
    //   const connect = "view" === syncType ? connectViewportViews : connectViewportFrusta
    // (SyncViewportsTool.ts:81-82).
    if (SyncType::Frusta == m_syncType)
        return &dqApp::synchronizeViewportFrusta;
    return &dqApp::synchronizeViewportViews;
}

void SyncViewportsTool::connect(dqApp::Viewport* const* vps, int count,
                                dqApp::SynchronizeViewportsFactory factory)
{
    // Ported from: SyncViewportsTool.connect (SyncViewportsTool.ts:75-87).
    disconnect();
    state().disconnect = dqApp::connectViewports(
        std::vector<dqApp::Viewport*>(vps, vps + count), factory);
    state().viewportIds.clear();
    for (int i = 0; i < count; ++i) {
        state().viewportIds.push_back(vps[i]->GetViewportId());
        // Ported from: x.onDisposed.addOnce(() => this.disconnect())
        // (SyncViewportsTool.ts:85-86) — the DanQing surface is OnViewClose
        // (see State::closeHooks for the timing rationale).
        state().closeHooks.add(dqApp::Application::Get().GetViewManager().OnViewClose.AddListener(
            [vp = vps[i]](dqApp::Viewport* closing) {
                if (closing == vp)
                    SyncViewportsTool::disconnect();
            }));
    }
    std::sort(state().viewportIds.begin(), state().viewportIds.end());
}

void SyncViewportsTool::disconnect()
{
    // Ported from: SyncViewportsTool.disconnect (SyncViewportsTool.ts:89-98) —
    // state.disconnect() + _state = undefined + _removeListeners teardown.
    if (state().disconnect)
        state().disconnect();
    state().disconnect = nullptr;
    state().viewportIds.clear();
    state().closeHooks.DisconnectAll();
}

}  // namespace Gui
