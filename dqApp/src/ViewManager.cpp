// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewManager implementation
// Ported from: itwinjs-core core/frontend/src/ViewManager.ts
#include "dqApp/ViewManager.h"
#include "dqApp/AcsTriadDecorator.h"
#include "dqApp/Application.h"
#include "dqApp/Viewport.h"
#include "dqApp/Decorator.h"

#include <dqRender/tile/TileAdmin.h>
#include <dqRender/tile/Tile.h>

#include <algorithm>
#include <chrono>

namespace dqApp {

ViewManager::ViewManager()
{
    // ← itwinjs-core ViewManager.onInitialized (ViewManager.ts:128) installs
    //   IModelApp.accuDraw as an always-on decorator. DanQing's always-on triad is
    //   owned here; it draws only when a view's viewFlags.acsTriad is set.
    m_acsTriad = std::make_unique<AcsTriadDecorator>();
    AddDecorator(m_acsTriad.get());
}
ViewManager::~ViewManager() = default;

bool ViewManager::AddViewport(Viewport* vp)
{
    if (!vp) return false;

    // Check not already added
    auto it = std::find(m_viewports.begin(), m_viewports.end(), vp);
    if (it != m_viewports.end()) return false;

    // ← ViewManager.ts:290 — newVp.onViewManagerAdd() creates the EventController
    // (DOM input bindings). DanQing's input path is the Viewport's QWidget event
    // overrides (platform adaptation), so there is nothing to install here.

    m_viewports.push_back(vp);

    // ← ViewManager.ts:294 — setSelectedView(newVp) is UNCONDITIONAL: the newly added
    // viewport always becomes the selected one.
    SetSelectedViewport(vp);

    // ← itwinjs-core: if (1 === this._viewports.length) IModelApp.startEventLoop();
    // Auto-start render loop when first viewport is added
    if (m_viewports.size() == 1) {
        Application::Get().StartEventLoop();
    }

    OnViewOpen.Raise(vp);
    return true;
}

void ViewManager::DropViewport(Viewport* vp)
{
    auto it = std::find(m_viewports.begin(), m_viewports.end(), vp);
    if (it == m_viewports.end()) return;

    m_viewports.erase(it);

    // ← ViewManager.ts:331-332 — if removed viewport was selectedView, set it to
    // undefined (setSelectedView(undefined) → getFirstOpenView() fallback, :230-231).
    if (m_selectedViewport == vp) {
        SetSelectedViewport(m_viewports.empty() ? nullptr : m_viewports.front());
    }

    OnViewClose.Raise(vp);
}

void ViewManager::ShutdownAll()
{
    // Quit-path teardown: drop + shut down every viewport so no live GL
    // pipeline outlives the native windows during QApplication teardown
    // (crash-on-close: RenderFrame -> Swapchain::acquire ->
    // [ctx setView:<freed NSView>] use-after-free).
    while (!m_viewports.empty()) {
        Viewport* vp = m_viewports.front();
        DropViewport(vp);   // removes from m_viewports + raises OnViewClose
        vp->Shutdown();     // release GL pipeline (idempotent)
    }
}

void ViewManager::RenderLoop()
{
    if (m_viewports.empty()) return;

    // Frame timing (PerformanceMetrics equivalent — Debug-info FPS source).
    // The reference records spf inside the render system's draw callback;
    // DanQing times the loop iteration from the App layer.
    static std::chrono::steady_clock::time_point s_lastFrame = std::chrono::steady_clock::now();
    auto const now = std::chrono::steady_clock::now();
    m_frameMetrics.recordFrame(std::chrono::duration<double>(now - s_lastFrame).count());
    s_lastFrame = now;

    OnBeginRender.Raise();

    for (auto* vp : m_viewports) {
        if (vp) {
            vp->RenderFrame();
        }
    }

    OnFinishRender.Raise();
}

// ---------------------------------------------------------------------------
// FrameMetrics — Ported from: itwinjs-core PerformanceMetrics.ts (spf window;
// FPS = window length / spf sum, FpsTracker.ts:62).
// ---------------------------------------------------------------------------
void FrameMetrics::recordFrame(double spfSeconds)
{
    if (m_count < kWindow) {
        m_sum += spfSeconds;
        m_spf[m_next] = spfSeconds;
        m_next = (m_next + 1) % kWindow;
        ++m_count;
    } else {
        m_sum += spfSeconds - m_spf[m_next];
        m_spf[m_next] = spfSeconds;
        m_next = (m_next + 1) % kWindow;
    }
}

double FrameMetrics::fps() const noexcept
{
    return m_sum > 0.0 ? static_cast<double>(m_count) / m_sum : 0.0;
}

double FrameMetrics::meanSpf() const noexcept
{
    return m_count > 0 ? m_sum / static_cast<double>(m_count) : 0.0;
}

// Ported from: itwinjs-core MemoryTracker calcMem across viewManager viewports
// (MemoryTracker.ts:98-104 — System case walks every viewport's target; the
// texture slice walks each viewport's driver textures).
void ViewManager::collectTextureStatistics(dqRender::RenderMemory::Statistics& stats) const
{
    for (auto const* vp : m_viewports) {
        if (vp)
            vp->collectTextureStatistics(stats);
    }
}

// Ported from: itwinjs-core ViewManager.purgeTileTrees (ViewManager.ts:430-454).
// EQUIVALENCE（登记于头文件注释）：参考经 iModel.tiles.purge（supplier 注册表）
// 释放未使用树；DanQing 无该注册表，按 TileAdmin LRU "not selected" 分区释放
// 已加载内容（drop），效果等价于 owner.dispose() 的内存回收。
void ViewManager::purgeTileTrees(double olderThanMs)
{
    (void)olderThanMs;  // LRU 无 per-tree 时间戳（注册表落地时接回时间过滤）
    dqRender::TileAdmin::instance().purgeUnselectedTileContents();
}

Viewport* ViewManager::GetViewport(size_t index) const
{
    if (index >= m_viewports.size()) return nullptr;
    return m_viewports[index];
}

// Ported from: itwinjs-core ViewManager.setSelectedView (ViewManager.ts:229-250) +
//              clearSelectedView (:222-226) + notifySelectedViewportChanged (:253-259).
// Note: the reference is async (notifySelectedViewportChanged chains the toolAdmin
// promise + emits the event); DanQing is synchronous — the order is preserved.
void ViewManager::SetSelectedViewport(Viewport* vp)
{
    // ViewManager.ts:230-231 — undefined means "select the first open view".
    if (!vp && !m_viewports.empty()) {
        vp = m_viewports.front();
    }

    // ViewManager.ts:233-234 — already the selected view: no-op, EXCEPT the
    // first-selection case: the reference starts the default tool on the first
    // selection (ViewManager.ts:246-247), and AddViewport pre-selects the
    // viewport before anyone else can select it — without this the default
    // tool (SelectionTool → locate circle) never installs.
    Viewport* previous = m_selectedViewport;
    bool const isFirstSelection = (previous == nullptr && vp != nullptr);
    if (m_selectedViewport == vp && !isFirstSelection) return;

    m_selectedViewport = vp;

    // ViewManager.ts:253-259 — notifySelectedViewportChanged: toolAdmin first, then event.
    Application::Get().GetToolAdmin().onSelectedViewportChanged(previous, vp);
    OnSelectedViewportChanged.Raise({previous, vp});

    // ViewManager.ts:246-247 — the default tool is started when the FIRST viewport is
    // selected (previousVp === undefined). ToolAdmin.onInitialized does NOT start it
    // (ToolAdmin.ts:510-525).
    // The first viewport is pre-selected by AddViewport (the constructor registers
    // it as selected) — "previous == nullptr" there means StartDefaultTool must
    // fire here, not only on a later change.
    if (previous == nullptr && vp != nullptr) {
        Application::Get().GetToolAdmin().StartDefaultTool();
    }
}

void ViewManager::AddDecorator(IDecorator* decorator)
{
    if (!decorator) return;
    auto it = std::find(m_decorators.begin(), m_decorators.end(), decorator);
    if (it != m_decorators.end()) return;
    m_decorators.push_back(decorator);
}

void ViewManager::DropDecorator(IDecorator* decorator)
{
    auto it = std::find(m_decorators.begin(), m_decorators.end(), decorator);
    if (it == m_decorators.end()) return;
    m_decorators.erase(it);
}

void ViewManager::AddFeatureOverrideProvider(dqCommon::FeatureOverrideProvider* provider)
{
    if (!provider) return;
    auto it = std::find(m_featureOverrideProviders.begin(), m_featureOverrideProviders.end(), provider);
    if (it != m_featureOverrideProviders.end()) return;
    m_featureOverrideProviders.push_back(provider);
}

void ViewManager::DropFeatureOverrideProvider(dqCommon::FeatureOverrideProvider* provider)
{
    auto it = std::find(m_featureOverrideProviders.begin(), m_featureOverrideProviders.end(), provider);
    if (it == m_featureOverrideProviders.end()) return;
    m_featureOverrideProviders.erase(it);
}

IDecorator* ViewManager::FindDecoratorForHit(uint32_t featureId) const
{
    for (auto* dec : m_decorators) {
        if (dec && dec->TestDecorationHit(featureId)) {
            return dec;
        }
    }
    return nullptr;
}

QString ViewManager::GetDecorationToolTip(uint32_t featureId) const
{
    auto* dec = FindDecoratorForHit(featureId);
    if (dec) {
        return dec->GetDecorationToolTip(featureId);
    }
    return {};
}

void ViewManager::OnSelectionSetChanged()
{
    // Mark every viewport's selection set dirty (next frame pushes the iModel's
    // HiliteSet to the render target, Viewport.ts:2613-2617) and request an
    // animation tick so that frame actually happens.
    // Ported from: itwinjs-core ViewManager.onSelectionSetChanged
    //               (ViewManager.ts:375-380)
    for (auto* vp : m_viewports) {
        if (vp) {
            vp->SetSelectionSetDirty();
            vp->RequestRedraw();
            vp->update();
        }
    }
}

// Ported from: itwinjs-core ViewManager.setViewCursor (ViewManager.ts:597-603 —
// same-name short-circuit, store, then apply to every registered viewport).
void ViewManager::setViewCursor(std::string const& cursor)
{
    if (cursor == m_cursor)
        return;
    m_cursor = cursor;
    for (auto* vp : m_viewports) {
        if (vp)
            vp->setCursor(cursor);
    }
}

// Ported from: itwinjs-core ViewManager.invalidateDecorationsAllViews
//              (ViewManager.ts:361-364 — `for (const vp of this) vp.invalidateDecorations()`)。
void ViewManager::invalidateDecorationsAllViews()
{
    for (auto* vp : m_viewports) {
        if (vp) {
            vp->InvalidateDecorations();
        }
    }
}

}  // namespace dqApp
