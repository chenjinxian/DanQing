// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Viewport synchronization utilities
// Ported from: itwinjs-core core/frontend/src/ViewportSync.ts
//
// Provides functions to synchronize the view state between multiple viewports.
// Useful for split-screen views, mini-map overlays, and multi-viewport layouts.
#pragma once

#include "Export.h"

#include <functional>
#include <vector>

namespace dqApp {

class Viewport;
class ViewState;

// Synchronization function type — called when any viewport in the group changes.
// The function receives the source viewport and should return a function that
// applies the synchronized state to all other viewports.
using SynchronizeViewportsFn = std::function<void(Viewport& source, Viewport& target)>;

// Connect multiple viewports for bidirectional synchronization.
// When any viewport's view changes, the sync function is called to update all others.
// Uses an echo flag to prevent recursive synchronization.
// Returns a disconnect function.
// Ported from: itwinjs-core ViewportSync.ts connectViewports()
DQ_APP_EXPORT void connectViewports(
    std::vector<Viewport*> const& viewports,
    SynchronizeViewportsFn syncFn);

// Synchronize frustum poses between viewports.
// When the source viewport's camera changes, the target viewport's camera
// is updated to match.
// Ported from: itwinjs-core ViewportSync.ts synchronizeViewportFrusta()
DQ_APP_EXPORT SynchronizeViewportsFn synchronizeViewportFrusta();

// Connect viewports for frustum synchronization.
// Convenience wrapper that calls connectViewports with synchronizeViewportFrusta().
// Ported from: itwinjs-core ViewportSync.ts connectViewportFrusta()
DQ_APP_EXPORT void connectViewportFrusta(std::vector<Viewport*> const& viewports);

// Synchronize full ViewState between viewports.
// When the source viewport's view changes, the target viewport's ViewState
// is replaced with a clone of the source's ViewState.
// Ported from: itwinjs-core ViewportSync.ts synchronizeViewportViews()
DQ_APP_EXPORT SynchronizeViewportsFn synchronizeViewportViews();

// Connect viewports for full ViewState synchronization.
// Convenience wrapper that calls connectViewports with synchronizeViewportViews().
// Ported from: itwinjs-core ViewportSync.ts connectViewportViews()
DQ_APP_EXPORT void connectViewportViews(std::vector<Viewport*> const& viewports);

// Two-way viewport synchronization for exactly two viewports.
// Ported from: itwinjs-core ViewportSync.ts TwoWayViewportSync
class DQ_APP_EXPORT TwoWayViewportSync {
public:
    TwoWayViewportSync(Viewport* first, Viewport* second);
    virtual ~TwoWayViewportSync();

    // Disconnect the synchronization.
    void disconnect();

protected:
    // Override to customize the synchronization behavior.
    virtual void syncViewports(Viewport& source, Viewport& target);

private:
    Viewport* m_first;
    Viewport* m_second;
    bool m_echo = false;
};

// Two-way frustum synchronization for exactly two viewports.
// Ported from: itwinjs-core ViewportSync.ts TwoWayViewportFrustumSync
class DQ_APP_EXPORT TwoWayViewportFrustumSync : public TwoWayViewportSync {
public:
    TwoWayViewportFrustumSync(Viewport* first, Viewport* second)
        : TwoWayViewportSync(first, second) {}

protected:
    void syncViewports(Viewport& source, Viewport& target) override;
};

}  // namespace dqApp
