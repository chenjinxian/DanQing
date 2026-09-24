// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Debug info window
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/DebugWindow.ts
//              + frontend/Window.ts (the floating-window shell: title bar with
//              close, drag-to-move, isPinned, isResizable=false)
//              + core/frontend-devtools/src/widgets/DiagnosticsPanel.ts
//              (the 7-widget panel; DTA excludes KeyinField — DebugWindow.ts:20)
//
// Panel sections (DiagnosticsPanel.ts:53-103 order):
//   1. FPS tracker                     (FpsTracker.ts)
//   2. (Key-in — excluded by DTA too,  DebugWindow.ts:20)
//   3. Tile request statistics         (TileStatisticsTracker.ts)
//   4. Tile memory breakdown           (TileMemoryBreakdown.ts)
//   5. Render command breakdown        (RenderCommandBreakdown.ts)
//   6. GPU memory                      (MemoryTracker.ts)
//   7. GPU profiler                    (GpuProfiler.ts)
//   8. Tool settings                   (ToolSettingsTracker.ts)
// The widget implementations live in DevToolsWidgets.{h,cpp}.
#pragma once

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;

namespace dqApp {
class Viewport;
}

namespace Gui {

class FpsTracker;
class TileStatisticsTracker;
class TileMemoryBreakdown;
class RenderCommandBreakdown;
class MemoryTracker;
class GpuProfiler;
class ToolSettingsTracker;

// ---------------------------------------------------------------------------
// DebugWindow — floating diagnostics panel over a viewport.
// Ported from: DebugWindow.ts (wraps DiagnosticsPanel in a Window).
//   title `[ ${viewport.viewportId} ] Diagnostics` (:30), isPinned (:32),
//   visibility follows the selected viewport (:34-36), isResizable=false
//   (:45), toggle/show/hide (:48-71).
// ---------------------------------------------------------------------------
class DebugWindow : public QWidget {
    Q_OBJECT
public:
    explicit DebugWindow(dqApp::Viewport* vp, QWidget* parent);
    ~DebugWindow() override;

    // ← DebugWindow.ts:48-53 (toggle/show/hide).
    void toggle();
    void showPanel();
    void hidePanel();

    // ← DebugWindow.ts:46 windowId (`debugPanel-${viewportId}`).
    QString windowId() const;

protected:
    // ← Window.ts DragState (title-bar left-drag moves the window, clamped
    // to the parent surface).
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void buildUi();

    dqApp::Viewport* m_vp;
    bool m_isOpen = false;             // ← DebugWindow.ts:15 _isOpen
    QWidget* m_titleBar = nullptr;     // ← Window.ts WindowHeader
    QLabel* m_titleLabel = nullptr;    // ← :129-136 _titleElement
    QPushButton* m_closeButton = nullptr;  // ← :140-143 _closeElement
    QWidget* m_content = nullptr;      // ← contentDiv 内的 debugPanel（show 时量其天然高度）
    QPoint m_dragOffset;               // DragState position tracking

    // The 7 panel widgets (DiagnosticsPanel.ts:53-103, DTA excludes keyin).
    FpsTracker* m_fpsTracker = nullptr;
    TileStatisticsTracker* m_statsTracker = nullptr;
    TileMemoryBreakdown* m_tileMemoryBreakdown = nullptr;
    RenderCommandBreakdown* m_renderCommands = nullptr;
    MemoryTracker* m_memoryTracker = nullptr;
    GpuProfiler* m_gpuProfiler = nullptr;
    ToolSettingsTracker* m_toolSettingsTracker = nullptr;

    // onSelectedViewportChanged disconnector (DebugWindow.ts:34-36 —
    // display flex/none when this window's viewport is (de)selected).
    std::function<void()> m_selectedViewportDisconnect;
};

}  // namespace Gui
