// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — View manager (viewport collection + render loop)
// Ported from: itwinjs-core core/frontend/src/ViewManager.ts
#pragma once

#include "Export.h"

#include <dqBase/DqEvent.h>

#include <dqCommon/FeatureOverrides.h>
#include <dqRender/RenderMemory.h>

#include <QString>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dqApp {

class Viewport;
class IDecorator;
class AcsTriadDecorator;

// ---------------------------------------------------------------------------
// FrameMetrics — rolling per-frame-time window.
// Ported from: itwinjs-core core/frontend/src/PerformanceMetrics.ts
// (the DiagnosticsPanel FpsTracker data source; FpsTracker.ts:62 computes
// FPS = spfTimes.length / spfSum over the recorded window).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT FrameMetrics {
public:
    // Record one frame's seconds-per-frame (called from RenderLoop).
    void recordFrame(double spfSeconds);

    // Frames per second over the recorded window (0 before any sample).
    // ← FpsTracker.ts:62 — spfTimes.length / spfSum.
    double fps() const noexcept;

    // Mean seconds per frame over the window.
    double meanSpf() const noexcept;

private:
    static constexpr size_t kWindow = 120;   // ~2s at 60fps
    double m_spf[kWindow] = {};
    size_t m_count = 0;
    size_t m_next = 0;
    double m_sum = 0.0;
};

// Argument for ViewManager::OnSelectedViewportChanged.
// Ported from: itwinjs-core ViewManager.ts SelectedViewportChangedArgs
struct SelectedViewportChangedArgs {
    Viewport* previous = nullptr;
    Viewport* current = nullptr;
};

// ---------------------------------------------------------------------------
// ViewManager — manages all active viewports and the render loop
// Ported from: itwinjs-core ViewManager.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewManager {
public:
    ViewManager();
    ~ViewManager();

    // Viewport management (← ViewManager.addViewport/dropViewport)
    bool AddViewport(Viewport* vp);
    void DropViewport(Viewport* vp);

    // Quit-path teardown: drop every viewport from the render list and release
    // its GL pipeline (Viewport::Shutdown), before QApplication destroys the
    // native windows. Called from Application on QCoreApplication::aboutToQuit.
    void ShutdownAll();

    // Render loop (← ViewManager.renderLoop)
    void RenderLoop();

    // Frame timing for diagnostics (PerformanceMetrics equivalent — the
    // Debug-info panel's FPS tracker source). RecordFrame is called by
    // RenderLoop each frame; reading fps() is always safe.
    FrameMetrics& frameMetrics() noexcept { return m_frameMetrics; }
    FrameMetrics const& frameMetrics() const noexcept { return m_frameMetrics; }

    // Decorator management (← ViewManager.addDecorator/dropDecorator)
    void AddDecorator(IDecorator* decorator);
    void DropDecorator(IDecorator* decorator);

    // Aggregate texture-memory statistics across all registered viewports
    // (MemoryTracker "Textures" consumer equivalent — MemoryTracker.ts:219;
    // Texture consumers only: buffers need the tile-tree/statistics engine).
    void collectTextureStatistics(dqRender::RenderMemory::Statistics& stats) const;

    // Purge tile trees that haven't been drawn since the specified time and are
    // not in use by any viewport. Intended strictly for debugging purposes.
    // Ported from: itwinjs-core ViewManager.purgeTileTrees (ViewManager.ts:430).
    // EQUIVALENCE: 参考源=ViewManager.ts:430-454 → iModel.tiles.purge(olderThan,
    // exclude)（Tiles.ts:234-247，supplier 注册表体系）；发散=DanQing 无 Tiles/
    // TileTreeSupplier 注册表（tile 内容管线未落地），按 TileAdmin LRU 的
    // "not selected" 分区逐 tile 释放内容（drop），等价于参考 owner.dispose()
    // 的内存回收效果；验证法=TileAdminTest 加载后 purge → totalTileContentBytes
    // 下降、selected 分区不受影响。olderThanMs 参数保留参考签名语义（当前
    // LRU 无 per-tree 时间戳，恒为"全部未选中"——注册表落地时接回时间过滤）。
    void purgeTileTrees(double olderThanMs = 0.0);

    // Feature override provider management (← ViewManager.addFeatureOverrideProvider/dropFeatureOverrideProvider)
    void AddFeatureOverrideProvider(dqCommon::FeatureOverrideProvider* provider);
    void DropFeatureOverrideProvider(dqCommon::FeatureOverrideProvider* provider);
    std::vector<dqCommon::FeatureOverrideProvider*> const& GetFeatureOverrideProviders() const { return m_featureOverrideProviders; }

    /// Find the decorator that owns the given feature ID.
    /// @return The decorator, or nullptr if no decorator claims the hit.
    IDecorator* FindDecoratorForHit(uint32_t featureId) const;

    /// Get tooltip for a decoration hit.
    /// @return Tooltip text, or empty string if no decorator claims the hit.
    QString GetDecorationToolTip(uint32_t featureId) const;

    /// Notify all viewports that the selection set changed.
    void OnSelectionSetChanged();

    // Force each registered Viewport to regenerate its Decorations on the next frame.
    // Ported from: itwinjs-core ViewManager.invalidateDecorationsAllViews
    //              (ViewManager.ts:360-364)。
    void invalidateDecorationsAllViews();

    // Query
    size_t GetViewportCount() const { return m_viewports.size(); }
    Viewport* GetViewport(size_t index) const;
    std::vector<IDecorator*> const& GetDecorators() const { return m_decorators; }
    Viewport* GetSelectedViewport() const { return m_selectedViewport; }

    // The CSS cursor name currently applied to every registered viewport
    // (ViewManager.ts:94 `public cursor = "default"`). Names are kept 1:1 as
    // the reference's CSS strings ("default"/"crosshair"/"move"/"not-allowed"/
    // "pointer"/...); Viewport::setCursor maps them to Qt cursor shapes.
    std::string const& cursor() const { return m_cursor; }

    // Set the cursor for all viewports. Same-name requests short-circuit.
    // Ported from: itwinjs-core ViewManager.setViewCursor (ViewManager.ts:597-603).
    void setViewCursor(std::string const& cursor = "default");

    // Cursor family (ViewManager.ts:585-592). The reference URLs point at
    // .cur image files with a CSS fallback shape; DanQing converts the same
    // .cur assets to PNG + hotspot (third_party/itwinjs-cursors/, converted
    // from the reference's public/cursors) and Viewport::setCursor loads the
    // bitmap first, falling back to the Qt shape when the bitmap is missing
    // (the reference's fallback-when-URL-fails semantics).
    std::string crossHairCursor() const { return "crosshair"; }   // crosshair.cur
    std::string dynamicsCursor() const { return "dynamics"; }     // dynamics.cur
    std::string grabCursor() const { return "grab"; }             // openHand.cur
    std::string grabbingCursor() const { return "grabbing"; }     // closedHand.cur
    std::string walkCursor() const { return "walk"; }             // walk.cur
    std::string rotateCursor() const { return "rotate"; }         // rotate.cur
    std::string lookCursor() const { return "look"; }             // look.cur
    std::string zoomCursor() const { return "zoom"; }             // zoom.cur

    // Get the active viewport (alias for GetSelectedViewport).
    // Ported from: itwinjs-core IModelApp.viewManager.selectedView
    Viewport* GetActiveViewport() const { return m_selectedViewport; }
    void SetSelectedViewport(Viewport* vp);

    // Events
    dqBase::DqEvent<Viewport*> OnViewOpen;
    dqBase::DqEvent<Viewport*> OnViewClose;
    dqBase::DqEvent<> OnBeginRender;
    dqBase::DqEvent<> OnFinishRender;
    dqBase::DqEvent<SelectedViewportChangedArgs> OnSelectedViewportChanged;

private:
    std::vector<Viewport*> m_viewports;
    std::string m_cursor = "default";  // ViewManager.ts:94
    FrameMetrics m_frameMetrics;
    std::vector<IDecorator*> m_decorators;
    // Always-on ACS-triad decorator (← itwinjs ViewManager.onInitialized installs
    // AccuDraw; DanQing owns the triad here). Gated per-view by viewFlags.acsTriad.
    std::unique_ptr<AcsTriadDecorator> m_acsTriad;
    std::vector<dqCommon::FeatureOverrideProvider*> m_featureOverrideProviders;
    Viewport* m_selectedViewport = nullptr;
};

}  // namespace dqApp
