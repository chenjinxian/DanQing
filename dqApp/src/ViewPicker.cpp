// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewPicker implementation
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/ViewPicker.ts
#include "dqApp/ViewPicker.h"
#include "dqApp/IModelConnection.h"
#include "dqApp/ViewState.h"
#include <dqBase/DqCompare.h>   // compareBooleans / compareStrings (core-bentley Compare.ts)
#include <dqCommon/ColorDef.h>
#include <dqCommon/RenderMode.h>
#include <dqCommon/ViewFlags.h>

namespace dqApp {

// Ported from: itwinjs-core ViewPicker.ts:149-169 (ViewList.manufactureSpatialView).
dqBase::RefPtr<SpatialViewState> manufactureSpatialView(IModelConnection* iModel)
{
    // ViewPicker.ts:150  const ext = iModel.projectExtents;
    auto const& ext = iModel->GetProjectExtents();

    // ViewPicker.ts:153  createBlank(iModel, ext.low, ext.high.minus(ext.low), undefined)
    auto blankView = SpatialViewState::CreateBlank(iModel, ext.low, ext.Diagonal());

    // ViewPicker.ts:156-161  style.viewFlags = style.viewFlags.copy({
    //   backgroundMap: true, lighting: true, renderMode: RenderMode.SmoothShade })
    auto& style = blankView->GetDisplayStyle();
    dqCommon::ViewFlagsProperties props = style.getViewFlags().Properties();
    props.backgroundMap = true;
    props.lighting = true;
    props.renderMode = dqCommon::RenderMode::SmoothShade;
    // Grid is OFF — DTA-faithful (ViewPicker.ts:156-161 does not set viewFlags.grid,
    // so it stays at its default false). Unblocker: DanQing now ports the background-
    // map depth-range branch of ViewingSpace.adjustZPlanes (ViewingSpace.ts:191-204),
    // so with backgroundMap on the sky's frustum stays deep (delta.z ≥ the grid-on
    // case) even with grid off — eliminating the white-sky coupling that previously
    // forced grid on. Verified at the frustum-math level (BackgroundMapGeometryTest.
    // BackgroundMapOnGridOffYieldsDeepFrustum); live DTA visual confirmation pending.
    // ACS triad is OFF here — DTA-faithful (ViewPicker.ts:156-161 does not set
    // viewFlags.acsTriad, so it stays at its default false). The user toggles it on
    // via the "ACS" toolbar action (DisplayTestApp main.cpp). The dqRender Polyline
    // thick-line path it exercises (arrow outlines + X/Y label glyphs) renders — TWO
    // fixes were required: (1) per-frame LUT lifecycle (OpenGLDriver::destroyTexture
    // → OpenGLState::invalidateTexture before glDeleteTextures, so a recycled GL name
    // no longer short-circuits bindTexture); (2) the Polyline program was receiving
    // ZERO u_proj/u_viewportTransformation/u_frustum (their ProgramUniform bindings
    // are registered but not invoked on the draw path) → gl_Position = u_proj·q = 0
    // → 0 fragments while Surface fills (u_mvp) rendered — fixed by uploading them
    // via params in the Polyline dispatch (SceneCompositorImpl drawPass).
    // props.acsTriad = true;
    style.setViewFlags(dqCommon::ViewFlags(props));

    // ViewPicker.ts:163  style.backgroundColor = ColorDef.white;
    // ColorDef.white is tbgr 0x00FFFFFF (alpha byte 0) — ColorDef stores 0xTTBBGGRR
    // (ColorDef.ts:655 white = new ColorDef(ColorByName.white), ColorByName.white =
    // 0xFFFFFF). Not 0xFFFFFFFF.
    style.setBackgroundColor(dqCommon::ColorDef::white.getTbgr());

    // ViewPicker.ts:166  style.environment = style.environment.withDisplay({ sky: true });
    style.toggleSkyBox(true);

    return blankView;
}

// ---------------------------------------------------------------------------
// ViewList (ViewPicker.ts:14-170)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core ViewPicker.ts:18-30 — private constructor with the
// UI sort comparator: isPrivate first (non-private sort first), then name, then id.
ViewList::ViewList()
    : dqBase::SortedArray<ViewSpec>([](ViewSpec const& lhs, ViewSpec const& rhs) {
        // Every entry has a unique Id, but we want to sort in the UI based on other
        // criteria first.
        int cmp = dqBase::compareBooleans(lhs.isPrivate, rhs.isPrivate);
        if (0 == cmp) {
            cmp = dqBase::compareStrings(lhs.name, rhs.name);
            if (0 == cmp)
                // ← compareStrings(lhs.id, rhs.id): the reference compares Id64Strings
                // lexicographically; DqId::ToString() reproduces that ordering.
                cmp = dqBase::compareStrings(lhs.id.ToString(), rhs.id.ToString());
        }
        return cmp;
    })
{
}

// Ported from: itwinjs-core ViewPicker.ts:63-67 (clear override).
void ViewList::clear()
{
    dqBase::SortedArray<ViewSpec>::clear();  // super.clear()
    m_defaultViewId = dqBase::DqId();        // Id64.invalid
    m_views.clear();
}

// Ported from: itwinjs-core ViewPicker.ts:34-51 (getView).
dqBase::RefPtr<ViewState> ViewList::getView(dqBase::DqId id, IModelConnection* iModel)
{
    dqBase::RefPtr<ViewState> view;
    auto it = m_views.find(id);
    if (it != m_views.end()) {
        view = it->second;
    } else {
        // ViewPicker.ts:37-44 — try to load the view; on failure replace with a
        // default spatial view. A blank connection's load always fails (no backend),
        // so this is the catch branch (ViewPicker.ts:39-44):
        //   "The view probably refers to a nonexistent display style or model/category
        //    selector... Or, we've opened a blank connection and `id` is intentionally
        //    invalid. The viewport's title bar will display 'UNNAMED' instead."
        view = iModel->GetViews().load(id);
        if (!view.IsValid())
            view = manufactureSpatialView(iModel);
        m_views.emplace(id, view);
    }

    // ViewPicker.ts:49-50 — NB: We clone so that if user switches back to this view,
    // it is shown in its initial (persistent) state.
    return view->Clone();
}

// Ported from: itwinjs-core ViewPicker.ts:53-55 (getDefaultView).
dqBase::RefPtr<ViewState> ViewList::getDefaultView(IModelConnection* iModel)
{
    return getView(m_defaultViewId, iModel);
}

// Ported from: itwinjs-core ViewPicker.ts:57-61 (static create). Synchronous: the
// reference's awaits are backend queries, all short-circuited for a closed connection.
ViewList ViewList::create(IModelConnection* iModel, std::optional<std::string> const& viewName)
{
    ViewList viewList;
    viewList.populate(iModel, viewName);
    return viewList;
}

// Ported from: itwinjs-core ViewPicker.ts:69-144 (populate).
void ViewList::populate(IModelConnection* iModel, std::optional<std::string> const& viewName)
{
    clear();  // ViewPicker.ts:70

    // ViewPicker.ts:72-75 — query all non-private views. They sort first in list.
    // (Blank connection: getViewList short-circuits to [] — IModelConnection.ts:1490-1491.)
    auto specs = iModel->GetViews().getViewList({false});
    for (auto const& spec : specs) {
        ViewSpec entry;
        static_cast<IModelConnection::ViewSpec&>(entry) = spec;
        entry.isPrivate = false;
        insert(entry);
    }

    // ViewPicker.ts:77-88 — query private views. They sort to end of list.
    // NB: the reference compares the private query's length against the NON-private
    // count (getViewList(wantPrivate:true) returns all views, private included) — the
    // quirk is ported verbatim.
    auto const nSpecs = specs.size();
    specs = iModel->GetViews().getViewList({true});
    if (specs.size() > nSpecs) {
        for (auto const& spec : specs) {
            ViewSpec entry;
            static_cast<IModelConnection::ViewSpec&>(entry) = spec;
            entry.isPrivate = false;
            if (findEqual(entry) == nullptr) {
                entry.isPrivate = true;
                insert(entry);
            }
        }
    }

    // ViewPicker.ts:90-97 — match the requested view name.
    if (viewName) {
        for (auto const& spec : m_array) {
            if (spec.name == *viewName) {
                m_defaultViewId = spec.id;
                break;
            }
        }
    }

    // ViewPicker.ts:99-125 — backfill names of unnamed views. The reference runs an
    // ECSQL query (createQueryReader joining bis.ViewDefinition2d to bis.Document) and
    // uses the element id for any view the query misses (:123-124). A blank connection
    // never has views, so the block is a no-op here; DanQing has no ECSQL layer, so only
    // the reference's "remaining → use the id" fallback is ported.
    // TODO: port the ECSQL name backfill with the data layer (out of engine scope).
    for (auto& spec : m_array) {
        if (spec.name.empty())
            spec.name = spec.id.ToString();
    }

    // ViewPicker.ts:127-137 — default view: the first entry, then the iModel's stored
    // default view id if it appears in the list.
    if (!m_defaultViewId.isValid() && !m_array.empty()) {
        m_defaultViewId = m_array[0].id;
        auto const defaultViewId = iModel->GetViews().queryDefaultViewId();
        for (auto const& spec : m_array) {
            if (spec.id == defaultViewId) {
                m_defaultViewId = defaultViewId;
                break;
            }
        }
    }

    // ViewPicker.ts:139-140 — no views found (e.g. blank connection): insert the
    // synthetic entry so getView(invalid) routes to manufactureSpatialView.
    if (!m_defaultViewId.isValid()) {
        ViewSpec synth;
        synth.id = dqBase::DqId();  // Id64.invalid
        synth.name = "Spatial View";
        // ← SpatialViewState.classFullName = schemaName:className = "BisCore:" +
        //   "SpatialViewDefinition" (EntityState.ts:74 + :21, SpatialViewState.ts:36).
        synth.className = "BisCore:SpatialViewDefinition";
        synth.isPrivate = false;
        insert(synth);
    }

    // ViewPicker.ts:143 — ensure default view is selected and loaded.
    (void)getView(m_defaultViewId, iModel);
}

}  // namespace dqApp
