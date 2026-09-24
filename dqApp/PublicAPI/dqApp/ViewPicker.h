// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewPicker (display-test-app view selection)
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/ViewPicker.ts
#pragma once

#include "Export.h"
#include "IModelConnection.h"   // IModelConnection::ViewSpec (ViewList entry base)
#include "ViewState.h"          // ViewState (getView/clone return)

#include <dqBase/RefCounted.h>
#include <dqBase/SortedArray.h>
#include <dqBase/DqId.h>

#include <optional>
#include <string>
#include <unordered_map>

namespace dqApp {

class SpatialViewState;

// ---------------------------------------------------------------------------
// manufactureSpatialView — create a spatial view initialized to show the project
// extents from top view, with the display-test-app display overrides applied
// (backgroundMap + lighting + SmoothShade + white background + sky on). Model and
// category selectors are empty.
// Ported from: itwinjs-core ViewPicker.ts:149-169 (ViewList.manufactureSpatialView).
// NOTE: in the reference this is a private ViewList method, reached via the view-load
// failure catch (ViewPicker.ts:39-44); DanQing exposes it as a free function (both
// ViewList::getView and direct callers use it).
// ---------------------------------------------------------------------------
dqBase::RefPtr<SpatialViewState> manufactureSpatialView(IModelConnection* iModel);

// ---------------------------------------------------------------------------
// ViewSpec — a view-list entry, with the display-test-app's isPrivate extension.
// Ported from: itwinjs-core ViewPicker.ts:10-12
//              (interface ViewSpec extends IModelConnection.ViewSpec { isPrivate }).
// ---------------------------------------------------------------------------
struct ViewSpec : IModelConnection::ViewSpec {
    bool isPrivate = false;
};

// ---------------------------------------------------------------------------
// ViewList — the sorted, cached list of an iModel's views backing the view picker.
// Ported from: itwinjs-core ViewPicker.ts:14-170 (class ViewList extends
//              SortedArray<ViewSpec>).
//
// The reference is async throughout (backend RPC); DanQing is synchronous — every
// async step short-circuits for a closed connection (a BlankConnection is always
// closed, IModelConnection.ts:781), which is the only connection kind today.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewList : public dqBase::SortedArray<ViewSpec> {
public:
    // ← ViewPicker.ts:32 get defaultViewId()
    dqBase::DqId defaultViewId() const { return m_defaultViewId; }

    // ← ViewPicker.ts:34-51 getView — cached load, load-failure → manufactureSpatialView,
    //    and every caller receives a CLONE so the cached view keeps its initial
    //    (persistent) state.
    dqBase::RefPtr<ViewState> getView(dqBase::DqId id, IModelConnection* iModel);

    // ← ViewPicker.ts:53-55 getDefaultView
    dqBase::RefPtr<ViewState> getDefaultView(IModelConnection* iModel);

    // ← ViewPicker.ts:57-61 static async create (synchronous here — the only async
    //    steps were the backend queries, short-circuited for a closed connection).
    static ViewList create(IModelConnection* iModel,
                           std::optional<std::string> const& viewName = std::nullopt);

    // ← ViewPicker.ts:63-67 clear() override. NB: hides (not overrides) the base's
    //    non-virtual clear() — all ViewList users clear through the ViewList
    //    interface, as in the reference.
    void clear();

    // ← ViewPicker.ts:69-144 populate
    void populate(IModelConnection* iModel,
                  std::optional<std::string> const& viewName = std::nullopt);

private:
    // ← ViewPicker.ts:18-30 private constructor (sort: isPrivate, then name, then id).
    ViewList();

    dqBase::DqId m_defaultViewId;  // ← _defaultViewId = Id64.invalid (:15)
    // ← _views (:16) — TS Map<Id64String, ViewState> → std::unordered_map (the §8.3
    //    TS-Map mapping); iteration order is never observed (get/set only).
    std::unordered_map<dqBase::DqId, dqBase::RefPtr<ViewState>> m_views;
};

}  // namespace dqApp
