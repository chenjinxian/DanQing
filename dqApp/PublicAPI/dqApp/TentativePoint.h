// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — TentativePoint (snap preview point)
// Ported from: itwinjs-core core/frontend/src/TentativePoint.ts
#pragma once

#include "Export.h"
#include "ToolAdmin.h"  // BeButtonEvent (faithful TentativePoint.onButtonEvent/process take BeButtonEvent)
#include <dqGeom/Point3d.h>

namespace dqApp {

// ---------------------------------------------------------------------------
// TentativePoint — manages the tentative (preview) snap point
// Ported from: itwinjs-core TentativePoint.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT TentativePoint {
public:
    TentativePoint() = default;
    virtual ~TentativePoint() = default;

    // Called after Application startup.
    // Ported from: itwinjs-core TentativePoint.onInitialized()
    virtual void onInitialized() {}

    // Whether the tentative point is active.
    // Ported from: itwinjs-core TentativePoint.isActive
    bool isActive() const noexcept { return m_active; }

    // Get the tentative point location.
    // Ported from: itwinjs-core TentativePoint.getPoint()
    dqGeom::Point3d const& getPoint() const noexcept { return m_point; }

    // Clear the tentative point.
    // Ported from: itwinjs-core TentativePoint.clear()
    void clear() { m_active = false; }

    // --- Button dispatch hooks (called from ToolAdmin.sendButtonEvent) -------
    // Step 3 no-op stubs — Task 15 may expand them to the full
    // removeTentative / synchSnapMode / setCurrSnap / tpHits = undefined logic
    // (TentativePoint.ts:103-121). No-op here is faithful to the case where
    // there is no active tentative point to clear.
    // Ported from: itwinjs-core TentativePoint.process (TentativePoint.ts:204-211 —
    //               early-return when a viewTool is in dynamic update).
    virtual void process(BeButtonEvent const&) {}
    // Ported from: itwinjs-core TentativePoint.onButtonEvent (TentativePoint.ts:103-121).
    virtual void onButtonEvent(BeButtonEvent const&) {}

private:
    bool m_active = false;
    dqGeom::Point3d m_point = dqGeom::Point3d::FromZero();
};

}  // namespace dqApp
