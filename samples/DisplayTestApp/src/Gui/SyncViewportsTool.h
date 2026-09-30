// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — viewport synchronization tools
// Ported from: itwinjs-core display-test-app SyncViewportsTool.ts
// (SyncViewportsTool — connect/disconnect two or more viewports with
// connectViewports; SyncViewportFrustaTool — the TwoWayViewportFrustumSync-shaped
// frustum variant; both key-in driven).
#pragma once

#include <QObject>
#include <QString>

#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewportSync.h>

#include <dqBase/DqEvent.h>

namespace Gui {

// "view" | "frusta" sync flavor (SyncViewportsTool.syncType, :33).
enum class SyncType { View, Frusta };

// Connect or disconnect two or more viewports. Ported from: SyncViewportsTool
// (SyncViewportsTool.ts:28-99) — toolId "SyncViewports" (frusta subclass
// "SyncFrusta"), minArgs 0 / maxArgs undefined (INT_MAX), keyins
// "dta viewport sync" / "dta frustum sync" (SVTTools.json).
class SyncViewportsTool : public dqApp::InteractiveTool
{
public:
    explicit SyncViewportsTool(SyncType type = SyncType::View) : m_syncType(type) {}

    const char* getToolId() const override
    {
        return SyncType::Frusta == m_syncType ? "SyncFrusta" : "SyncViewports";
    }
    // Ported from: SyncViewportsTool.maxArgs = undefined (SyncViewportsTool.ts:36).
    int maxArgs() const override { return INT32_MAX; }
    // SVTTools.json "tools.SyncViewports.keyin" / "tools.SyncFrusta.keyin".
    std::string englishKeyin() const override
    {
        return SyncType::Frusta == m_syncType ? "dta frustum sync" : "dta viewport sync";
    }

    // Ported from: SyncViewportsTool.run (SyncViewportsTool.ts:43-56) — fewer than
    // two viewports disconnects; connecting an already-connected set disconnects;
    // otherwise connect.
    bool run(dqApp::Viewport* const* vps, int count);
    bool run() override { return run(nullptr, 0); }
    // Ported from: SyncViewportsTool.parseAndRun (SyncViewportsTool.ts:58-73) —
    // 0 args → run(); 1 arg "all" → all viewports; >= 2 args → viewport ids.
    bool parseAndRun(std::vector<std::string> const& args) override;

protected:
    // Ported from: SyncViewportsTool.syncType — the frusta subclass overrides to
    // "frustum" (SyncViewportsTool.ts:90-95); the C++ port carries it as a member.
    virtual dqApp::SynchronizeViewportsFactory syncFactory() const;

private:
    // Ported from: SyncViewportsTool.State (SyncViewportsTool.ts:17-27) — the
    // connected viewport-id set (sorted) + its disconnect closure.
    struct State {
        std::vector<int> viewportIds;
        dqApp::DisconnectViewportsFn disconnect;
        // Ported from: SyncViewportsTool.connect's onDisposed hooks
        // (SyncViewportsTool.ts:85-86 — `vps.map(x => x.onDisposed.addOnce(disconnect))`).
        // DanQing surface: ViewManager.OnViewClose (raised inside DropViewport,
        // while the viewport's members are still alive) — a Qt destroyed hook
        // fires from ~QObject, AFTER the viewport's DqEvent members are gone, so
        // the disconnect tokens would dangle (MSVC "erase iterator outside
        // range" 实锤).
        dqBase::DqEventScope closeHooks;
        bool equals(dqApp::Viewport* const* vps, int count) const;
    };
    static State& state();
    // Ported from: SyncViewportsTool.connect/disconnect (SyncViewportsTool.ts:75-99)
    // — the onDisposed hooks are the Qt destroyed signals (reference:
    // x.onDisposed.addOnce(disconnect), :86).
    static void connect(dqApp::Viewport* const* vps, int count, dqApp::SynchronizeViewportsFactory factory);
    static void disconnect();

    SyncType m_syncType;
};

// Connect or disconnect two viewports using frustum synchronization.
// Ported from: SyncViewportFrustaTool (SyncViewportsTool.ts:91-99).
class SyncViewportFrustaTool final : public SyncViewportsTool
{
public:
    SyncViewportFrustaTool() : SyncViewportsTool(SyncType::Frusta) {}
};

}  // namespace Gui
