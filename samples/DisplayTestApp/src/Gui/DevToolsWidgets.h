// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — frontend-devtools diagnostics widgets (Qt port)
//
// Ported from: itwinjs-core core/frontend-devtools/src/widgets/*.ts
//   FpsTracker.ts / TileStatisticsTracker.ts / TileMemoryBreakdown.ts /
//   RenderCommandBreakdown.ts / MemoryTracker.ts / GpuProfiler.ts /
//   ToolSettingsTracker.ts
//
// The DTA DebugWindow builds its DiagnosticsPanel from these 7 widgets
// (DiagnosticsPanel.ts:53-103 order; DTA excludes KeyinField — DebugWindow.ts:20
// `exclude: { keyin: true }`). Qt control mapping (ui/CheckBox.ts etc.):
//   createCheckBox     → QCheckBox
//   createNumericInput → QSpinBox (int) / QDoubleSpinBox (parseAsFloat)
//   createComboBox     → QComboBox
//   createButton       → QPushButton
//   createNestedMenu   → arrow QToolButton + toggleable body (details/summary)
//   window.setInterval → QTimer
#pragma once

#include <dqRender/RenderMemory.h>   // Statistics/Consumers（MemoryTracker 载荷）
#include <dqRender/RenderSystem.h>  // GLTimerResult（GpuProfiler 回调载荷）

#include <QList>
#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

namespace dqApp {
class Viewport;
}

namespace dqRender {
namespace RenderMemory {
struct Statistics;
}
}  // namespace dqRender

namespace Gui {

// ---------------------------------------------------------------------------
// FpsTracker — displays average frames-per-second.
// Ported from: frontend-devtools/src/widgets/FpsTracker.ts
// ---------------------------------------------------------------------------
class FpsTracker : public QWidget {
    Q_OBJECT
public:
    explicit FpsTracker(dqApp::Viewport* vp, QWidget* parent);
    ~FpsTracker() override;

private:
    void toggle(bool enabled);   // FpsTracker.ts:45-58
    void updateFPS();            // FpsTracker.ts:60-64

    dqApp::Viewport* m_vp;
    QCheckBox* m_checkBox = nullptr;  // "Track FPS" (label text carries the FPS value)
    QTimer* m_interval = nullptr;     // window.setInterval(updateFPS, 500)
    bool m_metricsEnabled = false;    // metrics attached to target (performanceMetrics)
};

// ---------------------------------------------------------------------------
// TileStatisticsTracker — tile-request statistics (frame + global columns).
// Ported from: frontend-devtools/src/widgets/TileStatisticsTracker.ts
// ---------------------------------------------------------------------------
class TileStatisticsTracker : public QWidget {
    Q_OBJECT
public:
    explicit TileStatisticsTracker(dqApp::Viewport* vp, QWidget* parent);
    ~TileStatisticsTracker() override;

private:
    void addMaxActive();                          // :113-133
    void toggle();                                // :146-157
    void update();                                // :159-166
    void reset();                                 // :168-171

    dqApp::Viewport* m_vp;
    QSpinBox* m_maxActive = nullptr;              // numeric input (min 0, step 1)
    QCheckBox* m_checkBox = nullptr;              // "Track Tile Requests"
    QWidget* m_div = nullptr;                     // the stats table block
    QVector<QLabel*> m_statElements;              // statEntries[i] label
    QTimer* m_interval = nullptr;
};

// ---------------------------------------------------------------------------
// TileMemoryBreakdown — GPU tile memory by category, two panes.
// Ported from: frontend-devtools/src/widgets/TileMemoryBreakdown.ts
// ---------------------------------------------------------------------------
class TileMemoryBreakdown : public QWidget {
    Q_OBJECT
public:
    explicit TileMemoryBreakdown(QWidget* parent);
    ~TileMemoryBreakdown() override;

private:
    void toggle();                                // :195-204
    void update();                                // :213-237 (TileMemoryTracer + left pane)

    QCheckBox* m_checkBox = nullptr;              // "Tile Memory Breakdown"
    QWidget* m_div = nullptr;
    QVector<QLabel*> m_statsElements;             // right pane (5 categories)
    QVector<QLabel*> m_totalsElements;            // left pane (TileAdmin: 3 rows)
    QTimer* m_interval = nullptr;
};

// ---------------------------------------------------------------------------
// RenderCommandBreakdown — per-name render command counts.
// Ported from: frontend-devtools/src/widgets/RenderCommandBreakdown.ts
// ---------------------------------------------------------------------------
class RenderCommandBreakdown : public QWidget {
    Q_OBJECT
public:
    explicit RenderCommandBreakdown(QWidget* parent);
    ~RenderCommandBreakdown() override;

private:
    void toggle();                                // :41-50
    void update();                                // :59-78

    QCheckBox* m_checkBox = nullptr;              // "Render Commands"
    QWidget* m_div = nullptr;
    QMap<QString, QLabel*> m_cells;               // per-command-name row
    QLabel* m_total = nullptr;                    // "Total: N"
    QTimer* m_interval = nullptr;
};

// ---------------------------------------------------------------------------
// MemoryTracker — GPU memory by source (8-way selector) with Purge.
// Ported from: frontend-devtools/src/widgets/MemoryTracker.ts
// ---------------------------------------------------------------------------
class MemoryTracker : public QWidget {
    Q_OBJECT
public:
    explicit MemoryTracker(dqApp::Viewport* vp, QWidget* parent);
    ~MemoryTracker() override;

    // MemoryTracker.ts:114-126 formatMemory (b/kb/mb, toFixed(2)).
    static QString formatMemory(double numBytes);

private:
    // MemoryPanel — one labeled consumer group (:128-172).
    class MemoryPanel : public QWidget {
    public:
        MemoryPanel(QWidget* parent, QString label, QStringList entryLabels);

        void update(QVector<dqRender::RenderMemory::Consumers> const& stats, double total);

    private:
        QString m_label;
        QStringList m_labels;
        QLabel* m_header = nullptr;
        QVector<QLabel*> m_elems;
    };

    void addSelector();                           // :233-246
    void change(int newIndex);                    // :278-297
    void update();                                // :299-308
    void purge();                                 // :310-317
    // calcMem[i] — the 7 per-source collectors (:82-105). Return: #tile trees.
    int collectForIndex(int index, dqRender::RenderMemory::Statistics& stats);

    dqApp::Viewport* m_vp;
    QComboBox* m_selector = nullptr;              // "Track Memory: " 8 entries
    QWidget* m_div = nullptr;
    int m_memIndex = -1;                          // MemIndex.None == -1
    QTimer* m_interval = nullptr;                 // 1000ms
    QLabel* m_totalElem = nullptr;                // "Total: X"
    QLabel* m_totalTreesElem = nullptr;           // "Total Tile Trees: N"
    QPushButton* m_purgeButton = nullptr;
    MemoryPanel* m_textures = nullptr;
    MemoryPanel* m_buffers = nullptr;
};

// ---------------------------------------------------------------------------
// GpuProfiler — GPU timing trace (chrome://tracing export).
// Ported from: frontend-devtools/src/widgets/GpuProfiler.ts
// ---------------------------------------------------------------------------
class GpuProfiler : public QWidget {
    Q_OBJECT
public:
    explicit GpuProfiler(QWidget* parent);
    ~GpuProfiler() override;

private:
    void toggleProfileCheckBox(bool enabled);     // :143-153
    void clickRecord();                           // :155-163
    void stopRecording();                         // :165-175
    void resultsCallback(dqRender::GLTimerResult const& result);  // :177-251

    QCheckBox* m_checkBox = nullptr;              // "Profile GPU"
    QWidget* m_div = nullptr;
    QPushButton* m_recordButton = nullptr;        // "Record Profile"/"Stop Recording"
    QWidget* m_resultsDiv = nullptr;

    // GpuProfilerResults (:86-91)
    struct Results {
        QString label;
        double sum = 0.0;
        int paddingLeft = 0;
        QVector<double> values;
        // 显示行——随条目创建一次、仅更新文本（EQUIVALENCE 登记见 resultsCallback：
        // 参考每回调 innerHTML 整段重建，Qt 按帧删建行部件 = 布局振荡 + 行来不及
        // 首绘即毁）。toggle 清空 m_results 时随条目 delete。
        QWidget* row = nullptr;
        QLabel* textValue = nullptr;
    };
    QList<Results> m_results;
    QList<dqRender::GLTimerResult> m_recordedResults;
    bool m_isRecording = false;
};

// ---------------------------------------------------------------------------
// ToolSettingsTracker — global viewing-tool settings editor.
// Ported from: frontend-devtools/src/widgets/ToolSettingsTracker.ts
// ---------------------------------------------------------------------------
class ToolSettingsTracker : public QWidget {
    Q_OBJECT
public:
    explicit ToolSettingsTracker(dqApp::Viewport* vp, QWidget* parent);

private:
    dqApp::Viewport* m_vp;
};

}  // namespace Gui
