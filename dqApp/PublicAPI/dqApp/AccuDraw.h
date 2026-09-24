// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — AccuDraw (precision drawing aid)
// Ported from: itwinjs-core core/frontend/src/AccuDraw.ts
#pragma once

#include "Export.h"
#include "ToolAdmin.h"  // BeButtonEvent (faithful AccuDraw.onPreButtonEvent/onPostButtonEvent take BeButtonEvent)
#include <dqBase/DqEvent.h>

namespace dqApp {

class Viewport;
struct ButtonEvent;

// ---------------------------------------------------------------------------
// AccuDraw — precision drawing aid (compass, axis locking, distance input)
// Ported from: itwinjs-core AccuDraw.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT AccuDraw {
public:
    AccuDraw() = default;
    virtual ~AccuDraw() = default;

    // Called after Application startup.
    // Ported from: itwinjs-core AccuDraw.onInitialized()
    virtual void onInitialized() {}

    // Whether AccuDraw is currently enabled.
    // Ported from: itwinjs-core AccuDraw.isEnabled
    bool isEnabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // Whether AccuDraw is currently active (visible compass).
    // Ported from: itwinjs-core AccuDraw.isActive
    bool isActive() const noexcept { return m_active; }

    // Process a button event for AccuDraw adjustments.
    // Ported from: itwinjs-core AccuDraw.buttonEvent()
    virtual void buttonEvent(ButtonEvent const& event) { (void)event; }

    // Process a motion event.
    // Ported from: itwinjs-core AccuDraw.motionEvent()
    virtual void motionEvent(ButtonEvent const& event) { (void)event; }

    // --- Button dispatch hooks (called from ToolAdmin.sendButtonEvent) -------
    // Step 3 no-op stubs — Task 15 may expand to the full AccuDraw state-machine
    // (AccuDraw.ts:3234-3270 onPreButtonEvent / onPostButtonEvent). Returning
    // false matches the reference's isEnabled == false early-return path
    // (AccuDraw.ts:3242, 3262-3263).
    // Ported from: itwinjs-core AccuDraw.onPreButtonEvent (AccuDraw.ts:3234-3258)
    virtual bool onPreButtonEvent(BeButtonEvent const&) { return false; }
    // Ported from: itwinjs-core AccuDraw.onPostButtonEvent (AccuDraw.ts:3261-3270)
    virtual bool onPostButtonEvent(BeButtonEvent const&) { return false; }

    // Process a motion event for AccuDraw adjustments. Called by ToolAdmin
    // during onMotion dispatch (ToolAdmin.ts:1128). The reference body is the
    // no-op base (AccuDraw.ts:3231); subclasses override to update the compass.
    // Ported from: itwinjs-core AccuDraw.onMotion (AccuDraw.ts:3231).
    virtual void onMotion(BeButtonEvent const&) {}

private:
    bool m_enabled = false;
    bool m_active = false;
};

}  // namespace dqApp
