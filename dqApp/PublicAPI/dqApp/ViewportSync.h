// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Viewport synchronization utilities
// Ported from: itwinjs-core core/frontend/src/ViewportSync.ts
//
// Provides functions to synchronize the view state between multiple viewports.
// Useful for split-screen views, mini-map overlays, and multi-viewport layouts.
//
// M-L(3) 接线清偿（§11.10 ported-but-uncalled）：本单元原先的三处半成品按参考
// 源归位——①connectViewports 订阅 onViewChanged（Viewport.ts:310，doSetupFromView
// 尾部派发——ViewportSync.ts:56-58 只订阅它）并返回 disconnect 闭包
// （ViewportSync.ts:52-100）；②synchronizeViewportFrusta 实装 savePose/applyPose
// （原为 RequestRedraw 占位——ViewportSync.ts:102-112）；③TwoWayViewportSync.
// disconnect 实装（原为注释占位）。事件签名的 SynchronizeViewports 工厂形参
// （TS sync(changedViewport) => SynchronizeViewports，ViewportSync.ts:47）按
// 参考归位。
#pragma once

#include "Export.h"

#include <functional>
#include <vector>

namespace dqApp {

class Viewport;
class ViewState;

// A function used by connectViewports that can synchronize the state of a target
// Viewport with changes in the state of a source Viewport.
// Ported from: itwinjs-core SynchronizeViewports (ViewportSync.ts:14-19).
using SynchronizeViewportsFn = std::function<void(Viewport& source, Viewport& target)>;

// A function invoked once per source-viewport change to obtain the per-target
// synchronization function. Ported from: itwinjs-core the `sync` parameter of
// connectViewports (ViewportSync.ts:47 — `(changedViewport: Viewport) => SynchronizeViewports`).
using SynchronizeViewportsFactory = std::function<SynchronizeViewportsFn(Viewport& source)>;

// Function that severs a connection formed by connectViewports.
// Ported from: itwinjs-core the VoidFunction returned by connectViewports
//              (ViewportSync.ts:52/100).
using DisconnectViewportsFn = std::function<void()>;

// Forms a connection between two or more viewports such that a change in any one
// of them is reflected in all of the others. When the connection is first formed,
// all viewports are synchronized to the current state of the FIRST viewport.
// Returns a function that severs the connection.
// Lifetime contract (the reference's GC hides it — here it is explicit): the
// caller MUST sever the connection (or arrange to — e.g. ViewManager.OnViewClose)
// before any member viewport is destroyed; a destroyed viewport inside an
// still-connected group leaves a dangling entry in the sync set.
// Ported from: itwinjs-core connectViewports (ViewportSync.ts:52-100).
DQ_APP_EXPORT DisconnectViewportsFn connectViewports(
    std::vector<Viewport*> const& viewports,
    SynchronizeViewportsFactory syncFactory);

// Returns a function that synchronizes every aspect of the target viewport's state
// with the source's (a clone of the source's ViewState is applied).
// Ported from: itwinjs-core synchronizeViewportViews (ViewportSync.ts:104-110).
DQ_APP_EXPORT SynchronizeViewportsFn synchronizeViewportViews(Viewport& source);

// Returns a function that synchronizes the viewed volumes of each viewport (the
// source's view pose, captured at call time, is applied to the target).
// Ported from: itwinjs-core synchronizeViewportFrusta (ViewportSync.ts:114-122).
DQ_APP_EXPORT SynchronizeViewportsFn synchronizeViewportFrusta(Viewport& source);

// Connect viewports for frustum synchronization.
// Ported from: itwinjs-core connectViewportFrusta (ViewportSync.ts:126-129).
DQ_APP_EXPORT DisconnectViewportsFn connectViewportFrusta(std::vector<Viewport*> const& viewports);

// Connect viewports for full ViewState synchronization.
// Ported from: itwinjs-core connectViewportViews (ViewportSync.ts:133-136).
DQ_APP_EXPORT DisconnectViewportsFn connectViewportViews(std::vector<Viewport*> const& viewports);

// Forms a bidirectional connection between two viewports such that the ViewStates
// of each are synchronized with one another. Call connect() to establish the
// connection (the first viewport's state initializes the second), disconnect() to
// sever it. (Two-phase init is the reference shape — the virtual syncViewports
// must dispatch to the derived override, which a base-constructor call would not.)
// Ported from: itwinjs-core TwoWayViewportSync (ViewportSync.ts:140-190).
class DQ_APP_EXPORT TwoWayViewportSync {
public:
    TwoWayViewportSync() = default;
    virtual ~TwoWayViewportSync();

    // Establish the connection between two Viewports: initialize viewport2 with
    // the state of viewport1, then subscribe both viewports for ongoing
    // synchronization. Any prior connection is severed first.
    // Ported from: itwinjs-core TwoWayViewportSync.connect (ViewportSync.ts:167-174).
    void connect(Viewport* viewport1, Viewport* viewport2);

    // Remove the connection between the two views.
    // Ported from: itwinjs-core TwoWayViewportSync.disconnect (ViewportSync.ts:181-185).
    void disconnect();

protected:
    // Sets up the initial connection between two viewports by applying a clone
    // of source's ViewState to target. Virtual so subclasses can customize the
    // initial synchronization (TwoWayViewportFrustumSync overrides with
    // syncViewports). Ported from: TwoWayViewportSync.connectViewports
    // (ViewportSync.ts:152-155——2026-10-07 审计 S-4：原缺扩展点，初始同步
    // 内联在 connect 里走 syncViewports）。
    virtual void connectViewports(Viewport& source, Viewport& target);

    // Invoked each time source changes to update target to match. Default applies
    // a clone of the source's ViewState to the target.
    // Ported from: itwinjs-core TwoWayViewportSync.syncViewports (ViewportSync.ts:159-165).
    virtual void syncViewports(Viewport& source, Viewport& target);

private:
    Viewport* m_first = nullptr;
    Viewport* m_second = nullptr;
    bool m_echo = false;
    // Severance tokens for the per-viewport onViewChanged subscriptions
    // (ViewportSync.ts:55 + :180-184 — the reference stores the disconnect fns
    // returned by each addListener).
    std::vector<DisconnectViewportsFn> m_disconnect;
};

// Forms a bidirectional connection between two viewports such that the frusta of
// each are synchronized with one another (no other aspects of the viewports are
// synchronized).
// Ported from: itwinjs-core TwoWayViewportFrustumSync (ViewportSync.ts:194-214).
class DQ_APP_EXPORT TwoWayViewportFrustumSync : public TwoWayViewportSync {
public:
    TwoWayViewportFrustumSync() = default;

protected:
    // Synchronizes the two viewports by applying the source's frustum to the target.
    // Ported from: itwinjs-core TwoWayViewportFrustumSync.syncViewports
    //              (ViewportSync.ts:197-201).
    void syncViewports(Viewport& source, Viewport& target) override;

    // Sets up the initial connection by applying source's frustum to target
    //（:203-205 override —— connectViewports → syncViewports）。
    void connectViewports(Viewport& source, Viewport& target) override;
};

}  // namespace dqApp
