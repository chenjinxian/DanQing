// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewportSync implementation
// Ported from: itwinjs-core core/frontend/src/ViewportSync.ts
#include "dqApp/ViewportSync.h"
#include "dqApp/Viewport.h"

#include <dqBase/DqEvent.h>

namespace dqApp {

// ---------------------------------------------------------------------------
// connectViewports — bidirectional synchronization
// Ported from: itwinjs-core ViewportSync.ts connectViewports()
// ---------------------------------------------------------------------------
void connectViewports(
    std::vector<Viewport*> const& viewports,
    SynchronizeViewportsFn syncFn)
{
    if (viewports.size() < 2 || !syncFn)
        return;

    // Use a shared echo flag to prevent recursive synchronization.
    auto echo = std::make_shared<bool>(false);

    // Subscribe to OnChangeView on each viewport.
    // When any viewport changes, update all others.
    for (auto* vp : viewports) {
        if (!vp) continue;

        // Capture the viewport pointer and all other viewports.
        vp->OnChangeView.AddListener([vp, &viewports, syncFn, echo](ViewState*) {
            if (*echo) return;  // prevent recursion
            *echo = true;

            for (auto* target : viewports) {
                if (target && target != vp) {
                    syncFn(*vp, *target);
                }
            }

            *echo = false;
        });
    }
}

// ---------------------------------------------------------------------------
// synchronizeViewportFrusta — sync camera frustums
// Ported from: itwinjs-core ViewportSync.ts synchronizeViewportFrusta()
// ---------------------------------------------------------------------------
SynchronizeViewportsFn synchronizeViewportFrusta()
{
    return [](Viewport& /*source*/, Viewport& target) {
        // Copy the frustum pose from source to target.
        // In itwinjs-core, this uses view.savePose() / target.applyPose().
        // For now, we just trigger a redraw on the target.
        // Full implementation would copy the camera position, rotation, and extents.
        target.RequestRedraw();
    };
}

// ---------------------------------------------------------------------------
// connectViewportFrusta — convenience wrapper
// Ported from: itwinjs-core ViewportSync.ts connectViewportFrusta()
// ---------------------------------------------------------------------------
void connectViewportFrusta(std::vector<Viewport*> const& viewports)
{
    connectViewports(viewports, synchronizeViewportFrusta());
}

// ---------------------------------------------------------------------------
// synchronizeViewportViews — sync full ViewState
// Ported from: itwinjs-core ViewportSync.ts synchronizeViewportViews()
// ---------------------------------------------------------------------------
SynchronizeViewportsFn synchronizeViewportViews()
{
    return [](Viewport& source, Viewport& target) {
        auto* sourceView = source.GetView();
        if (sourceView) {
            auto clonedView = sourceView->Clone();
            if (clonedView) {
                target.ChangeView(std::move(clonedView));
            }
        }
    };
}

// ---------------------------------------------------------------------------
// connectViewportViews — convenience wrapper
// Ported from: itwinjs-core ViewportSync.ts connectViewportViews()
// ---------------------------------------------------------------------------
void connectViewportViews(std::vector<Viewport*> const& viewports)
{
    connectViewports(viewports, synchronizeViewportViews());
}

// ---------------------------------------------------------------------------
// TwoWayViewportSync
// Ported from: itwinjs-core ViewportSync.ts TwoWayViewportSync
// ---------------------------------------------------------------------------
TwoWayViewportSync::TwoWayViewportSync(Viewport* first, Viewport* second)
    : m_first(first)
    , m_second(second)
{
    if (m_first) {
        m_first->OnChangeView.AddListener([this](ViewState*) {
            if (m_echo) return;
            m_echo = true;
            if (m_first && m_second) {
                syncViewports(*m_first, *m_second);
            }
            m_echo = false;
        });
    }

    if (m_second) {
        m_second->OnChangeView.AddListener([this](ViewState*) {
            if (m_echo) return;
            m_echo = true;
            if (m_first && m_second) {
                syncViewports(*m_second, *m_first);
            }
            m_echo = false;
        });
    }
}

TwoWayViewportSync::~TwoWayViewportSync()
{
    disconnect();
}

void TwoWayViewportSync::disconnect()
{
    // The event connections are automatically cleaned up when the
    // Viewport is destroyed. For explicit disconnection, we would
    // need to store the disconnect tokens.
}

void TwoWayViewportSync::syncViewports(Viewport& source, Viewport& target)
{
    // Clone source ViewState and apply to target.
    auto* sourceView = source.GetView();
    if (sourceView) {
        auto clonedView = sourceView->Clone();
        if (clonedView) {
            target.ChangeView(std::move(clonedView));
        }
    }
}

// ---------------------------------------------------------------------------
// TwoWayViewportFrustumSync
// Ported from: itwinjs-core ViewportSync.ts TwoWayViewportFrustumSync
// ---------------------------------------------------------------------------
void TwoWayViewportFrustumSync::syncViewports(Viewport& /*source*/, Viewport& target)
{
    // Frustum-only synchronization — just trigger a redraw.
    // Full implementation would copy the camera frustum.
    target.RequestRedraw();
}

}  // namespace dqApp
