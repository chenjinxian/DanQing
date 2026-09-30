// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Viewport synchronization implementation
// Ported from: itwinjs-core core/frontend/src/ViewportSync.ts
#include "dqApp/ViewportSync.h"
#include "dqApp/Viewport.h"
#include "dqApp/ViewState.h"
#include "dqApp/ViewPose.h"

#include <dqBase/DqEvent.h>

#include <memory>

namespace dqApp {

// ---------------------------------------------------------------------------
// connectViewports — bidirectional synchronization
// Ported from: itwinjs-core connectViewports (ViewportSync.ts:52-100).
//
// C++ lifetime adaptation: the reference's closures over `echo`/`viewports` are
// GC-held; here the shared state lives in a shared_ptr captured by value by both
// the per-viewport listeners and the returned disconnect closure (stack locals
// would dangle the listeners after this function returns).
// ---------------------------------------------------------------------------
DisconnectViewportsFn connectViewports(
    std::vector<Viewport*> const& viewports,
    SynchronizeViewportsFactory syncFactory)
{
    // 偏差登记（M-L(3) 终审 Minor-⑦a）：size<2 早退——参考无此门（对 0/1 个
    // 视口同样挂 listener 并做一次首视口 synchronize；行为上等价于 no-op，
    // 差异仅在 syncFactory 的捕获副作用会多跑一次）。当前无观察面；如需
    // 1:1 可去掉早退门（保持断言面不变）。
    if (viewports.size() < 2 || !syncFactory)
        return [] {};  // TS returns a no-op closure (no listeners were added)

    struct SyncState {
        bool echo = false;
        std::vector<Viewport*> viewports;
        SynchronizeViewportsFactory factory;
    };
    auto state = std::make_shared<SyncState>();
    state->viewports = viewports;
    state->factory = std::move(syncFactory);

    auto const synchronize = [state](Viewport* source) {
        if (state->echo)
            return;

        // Ignore onViewChanged events resulting from synchronization.
        state->echo = true;
        SynchronizeViewportsFn const doSync = state->factory(*source);
        for (Viewport* vp : state->viewports)
            if (vp != nullptr && vp != source)
                doSync(*source, *vp);
        state->echo = false;
    };

    std::vector<DisconnectViewportsFn> disconnect;
    Viewport* firstViewport = nullptr;
    for (Viewport* vp : viewports) {
        if (!vp)
            continue;
        if (!firstViewport)
            firstViewport = vp;

        // TS: disconnect.push(vp.onViewChanged.addListener(() => synchronize(vp)))
        // — DqEvent's AddListener returns the removal token; wrap it as the
        // VoidFunction the reference stores.
        dqBase::DqEventDisconnect token = vp->onViewChanged.AddListener(
            [vp, synchronize](Viewport*) { synchronize(vp); });
        disconnect.push_back([token]() { token(); });
    }

    if (firstViewport)
        synchronize(firstViewport);

    return [disconnect]() {
        for (auto const& f : disconnect)
            f();
    };
}

// ---------------------------------------------------------------------------
// synchronizeViewportFrusta — sync camera frustums
// Ported from: itwinjs-core synchronizeViewportFrusta (ViewportSync.ts:114-122):
//   const pose = source.view.savePose();
//   return (_source, target) => {
//     const view = target.view.applyPose(pose);
//     target.applyViewState(view);
//   };
// DanQing applyViewState equivalent (Viewport.ts:2362-2367: updateChangeFlags +
// setView + synchWithView): the pose is applied to the target's view in place, so
// the propagation is synchWithView (viewport-from-view rebuild, the same step
// FitViewTool.doFit drives after mutating the view) + a redraw request.
// ---------------------------------------------------------------------------
SynchronizeViewportsFn synchronizeViewportFrusta(Viewport& source)
{
    // savePose/applyPose live on ViewState3d (the reference's view.is3d() forms —
    // ViewSync.ts:115/:119); DanQing keeps them on the 3d layer (ViewState.h).
    ViewState3d* sourceView = source.GetView() ? source.GetView()->AsViewState3d() : nullptr;
    if (!sourceView)
        return [](Viewport&, Viewport&) {};

    // unique_ptr is non-copyable (lambda captures copy) — share the pose.
    std::shared_ptr<ViewPose> const pose = sourceView->savePose();

    return [pose](Viewport& /*source*/, Viewport& target) {
        ViewState3d* targetView = target.GetView() ? target.GetView()->AsViewState3d() : nullptr;
        if (!targetView || !pose)
            return;
        targetView->applyPose(*pose);
        // 偏差登记（M-L(3) 终审 Minor-⑦b）：synchWithView() 未传
        // noSaveInUndo——每次 frusta 同步往目标视口的 view-undo 栈压一条
        // （参考 applyViewState = setView + synchWithView，不经 undo 保存；
        // Viewport.ts:2362-2367）。同步风暴会稀释 undo 栈——消除需给
        // synchWithView 传 ViewChangeOptions{noSaveInUndo=true}，随该参数面
        // 的既有消费锁（DtaToolsWiring.Sync×2）一起复核。
        target.synchWithView();
        target.RequestRedraw();
    };
}

// ---------------------------------------------------------------------------
// synchronizeViewportViews — sync full ViewState
// Ported from: itwinjs-core synchronizeViewportViews (ViewportSync.ts:104-110):
//   (_source, target) => target.applyViewState(source.view.clone(target.iModel))
// DanQing applyViewState equivalent = ChangeView (the full view-swap path: attach/
// detach + doSetupFromView + InvalidateController — the ported half of the
// reference's setView + synchWithView).
// ---------------------------------------------------------------------------
SynchronizeViewportsFn synchronizeViewportViews(Viewport& source)
{
    return [&source](Viewport& /*source*/, Viewport& target) {
        ViewState* sourceView = source.GetView();
        if (sourceView) {
            auto clonedView = sourceView->Clone();
            if (clonedView) {
                target.ChangeView(std::move(clonedView));
            }
        }
    };
}

// ---------------------------------------------------------------------------
// connectViewportFrusta / connectViewportViews — convenience wrappers
// Ported from: itwinjs-core (ViewportSync.ts:126-136).
// ---------------------------------------------------------------------------
DisconnectViewportsFn connectViewportFrusta(std::vector<Viewport*> const& viewports)
{
    return connectViewports(viewports, synchronizeViewportFrusta);
}

DisconnectViewportsFn connectViewportViews(std::vector<Viewport*> const& viewports)
{
    return connectViewports(viewports, synchronizeViewportViews);
}

// ---------------------------------------------------------------------------
// TwoWayViewportSync
// Ported from: itwinjs-core TwoWayViewportSync (ViewportSync.ts:140-190).
// ---------------------------------------------------------------------------
TwoWayViewportSync::~TwoWayViewportSync()
{
    disconnect();
}

// Ported from: itwinjs-core TwoWayViewportSync.connect (ViewportSync.ts:167-174):
//   this.disconnect();
//   this.connectViewports(viewport1, viewport2);        // initial synchronization
//   this._disconnect.push(connectViewports([v1, v2],    // ongoing synchronization
//     () => (source, target) => this.syncViewports(source, target)));
void TwoWayViewportSync::connect(Viewport* viewport1, Viewport* viewport2)
{
    disconnect();

    m_first = viewport1;
    m_second = viewport2;

    if (m_first && m_second)
        syncViewports(*m_first, *m_second);

    if (m_first) {
        dqBase::DqEventDisconnect token = m_first->onViewChanged.AddListener(
            [this](Viewport*) {
                if (m_echo)
                    return;
                m_echo = true;
                if (m_first && m_second)
                    syncViewports(*m_first, *m_second);
                m_echo = false;
            });
        m_disconnect.push_back([token]() { token(); });
    }

    if (m_second) {
        dqBase::DqEventDisconnect token = m_second->onViewChanged.AddListener(
            [this](Viewport*) {
                if (m_echo)
                    return;
                m_echo = true;
                if (m_first && m_second)
                    syncViewports(*m_second, *m_first);
                m_echo = false;
            });
        m_disconnect.push_back([token]() { token(); });
    }
}

// Ported from: itwinjs-core TwoWayViewportSync.disconnect (ViewportSync.ts:181-185).
void TwoWayViewportSync::disconnect()
{
    for (auto const& f : m_disconnect)
        f();
    m_disconnect.clear();
    m_first = nullptr;
    m_second = nullptr;
}

// Ported from: itwinjs-core TwoWayViewportSync.syncViewports (ViewportSync.ts:159-165):
// target.applyViewState(source.view.clone(target.iModel)).
void TwoWayViewportSync::syncViewports(Viewport& source, Viewport& target)
{
    ViewState* sourceView = source.GetView();
    if (sourceView) {
        auto clonedView = sourceView->Clone();
        if (clonedView) {
            target.ChangeView(std::move(clonedView));
        }
    }
}

// ---------------------------------------------------------------------------
// TwoWayViewportFrustumSync
// Ported from: itwinjs-core TwoWayViewportFrustumSync (ViewportSync.ts:194-214):
//   syncViewports: pose = source.view.savePose(); view = target.view.applyPose(pose);
//                  target.applyViewState(view).
// ---------------------------------------------------------------------------
void TwoWayViewportFrustumSync::syncViewports(Viewport& source, Viewport& target)
{
    ViewState3d* sourceView = source.GetView() ? source.GetView()->AsViewState3d() : nullptr;
    ViewState3d* targetView = target.GetView() ? target.GetView()->AsViewState3d() : nullptr;
    if (!sourceView || !targetView)
        return;

    std::unique_ptr<ViewPose> const pose = sourceView->savePose();
    targetView->applyPose(*pose);
    target.synchWithView();
    target.RequestRedraw();
}

}  // namespace dqApp
