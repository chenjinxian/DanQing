// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — frontend-devtools diagnostics widgets implementation
// Ported from: itwinjs-core core/frontend-devtools/src/widgets/*.ts
//   (FpsTracker / TileStatisticsTracker / TileMemoryBreakdown /
//    RenderCommandBreakdown / MemoryTracker / GpuProfiler / ToolSettingsTracker)
#include "DevToolsWidgets.h"

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ToolSettings.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqRender/RenderTarget.h>
#include <dqRender/RenderSystem.h>
#include <dqRender/tile/Tile.h>
#include <dqRender/tile/TileAdmin.h>
#include <dqRender/tile/TileTree.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace {

// Stat-row label factory — the reference's stat cells are right-aligned and
// shrink to their column (textAlign:right, no width pressure). Plain QLabels
// force the layout wider than the window when the text is long (the
// "Decoding mean time (ms)" global column clipped the whole panel → the
// 2026-09-21 显示不完整/跳动 report). Ignored horizontal policy + elide
// keeps every row inside the panel width — no horizontal scrollbar.
// Stat-row label factory — default (Preferred) policy so the layout stretches
// each label to its FULL cell width: text then renders inside the cell.
// Ignored was a mistake — the layout then undersizes the label's rect and
// QLabel draws the text overflowing past it (clipped only by the viewport
// edge → the "cut at colon" artifact seen in the 显示不完整 report).
// `align` carries the reference's text-align: TileStatisticsTracker /
// TileMemoryBreakdown / RenderCommandBreakdown / MemoryTracker all wrap their
// stat rows in a `textAlign: right` div (TileStatisticsTracker.ts:73,
// TileMemoryBreakdown.ts:159, RenderCommandBreakdown.ts:29, MemoryTracker.ts:194);
// GpuProfiler rows use the browser default (left).
QLabel* makeStatLabel(QWidget* parent, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto* label = new QLabel(parent);
    label->setAlignment(align);
    return label;
}

}  // namespace

namespace Gui {

// ===========================================================================
// FpsTracker
// Ported from: frontend-devtools/src/widgets/FpsTracker.ts
// ===========================================================================

// Ported from: FpsTracker.ts:24-32 (createCheckBox "Track FPS").
FpsTracker::FpsTracker(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent)
    , m_vp(vp)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_checkBox = new QCheckBox(QStringLiteral("Track FPS"), this);
    layout->addWidget(m_checkBox);
    connect(m_checkBox, &QCheckBox::clicked, this, [this](bool checked) {
        toggle(checked);
    });
}

FpsTracker::~FpsTracker()
{
    // FpsTracker.ts:34-36 [Symbol.dispose] → toggle(false).
    toggle(false);
}

// Ported from: FpsTracker.ts:45-58 toggle.
void FpsTracker::toggle(bool enabled)
{
    // ← Viewport.continuousRendering (Viewport.ts:1455): every render-loop tick
    // draws a new frame while tracking.
    if (m_vp)
        m_vp->setContinuousRendering(enabled);
    if (enabled) {
        // new PerformanceMetrics(false, true) + attach to target
        // (performanceMetrics — DanQing: FrameMetrics is always live on the
        // ViewManager; "attaching" is the metrics-enabled flag).
        m_metricsEnabled = true;
        m_interval = new QTimer(this);
        m_interval->setInterval(500);
        connect(m_interval, &QTimer::timeout, this, &FpsTracker::updateFPS);
        m_interval->start();
        m_checkBox->setText(QStringLiteral("Tracking FPS..."));
    } else {
        m_metricsEnabled = false;
        if (m_interval) {
            m_interval->stop();
            m_interval->deleteLater();
            m_interval = nullptr;
        }
        m_checkBox->setText(QStringLiteral("Track FPS"));
    }
}

// Ported from: FpsTracker.ts:60-64 updateFPS — label = `FPS: ${(spfTimes.length / spfSum).toFixed(2)}`.
void FpsTracker::updateFPS()
{
    if (!m_vp || !m_metricsEnabled)
        return;
    // spfTimes.length / spfSum — DanQing's FrameMetrics::fps() is that ratio
    // over the recorded window (ViewManager.h FrameMetrics).
    double const fps = dqApp::Application::Get().GetViewManager().frameMetrics().fps();
    m_checkBox->setText(QStringLiteral("FPS: %1").arg(fps, 0, 'f', 2));
}

// ===========================================================================
// TileStatisticsTracker
// Ported from: frontend-devtools/src/widgets/TileStatisticsTracker.ts
// ===========================================================================

namespace {

// computeProgress — TileStatisticsTracker.ts:21-27.
int computeTileProgress(dqApp::Viewport* vp)
{
    size_t const ready = vp->numReadyTiles();
    size_t const requested = vp->numRequestedTiles();
    size_t const total = ready + requested;
    double const ratio = (total > 0) ? static_cast<double>(ready) / static_cast<double>(total) : 1.0;
    return static_cast<int>(std::lround(ratio * 100.0));
}

// statEntries — TileStatisticsTracker.ts:29-48, order 1:1. Each entry's value
// is computed in update() from TileAdmin::Statistics + the viewport.
struct TileStatEntry {
    char const* label;
    bool global;  // >= indexOfFirstGlobalStatistic (7 = "Completed", :50)
};

TileStatEntry const kTileStatEntries[] = {
    { "Active", false },          // numActiveRequests + external.requested
    { "Pending", false },
    { "Canceled", false },
    { "Total", false },           // numActiveRequests + numPendingRequests
    { "Selected", false },        // vp.numSelectedTiles
    { "Ready", false },           // vp.numReadyTiles
    { "Progress", false },        // computeProgress
    { "Completed", true },
    { "Timed Out", true },
    { "Failed", true },
    { "Empty", true },
    { "Undisplayable", true },
    { "Elided", true },
    { "Cache Misses", true },
    { "Dispatched", true },
    { "Aborted", true },
    { "Decoding mean time (ms)", true },
    { "Decoding max time (ms)", true },
};
int const kNumTileStatEntries = static_cast<int>(sizeof(kTileStatEntries) / sizeof(kTileStatEntries[0]));

}  // namespace

// Ported from: TileStatisticsTracker.ts:61-107 constructor.
TileStatisticsTracker::TileStatisticsTracker(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent)
    , m_vp(vp)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    addMaxActive();

    m_checkBox = new QCheckBox(QStringLiteral("Track Tile Requests"), this);
    layout->addWidget(m_checkBox);
    connect(m_checkBox, &QCheckBox::clicked, this, [this]() { toggle(); });

    m_div = new QWidget(this);
    m_div->hide();
    auto* divLayout = new QVBoxLayout(m_div);
    divLayout->setContentsMargins(0, 0, 0, 0);

    // Two-column table (frame | global), each 50% of the panel width —
    // :75-95 (table width:100%, frameColumn/globalColumn width:50%). The
    // columns STRETCH with the window; they never force it wider. (An earlier
    // fixed-170px-column fabrication made the content wider than the window →
    // h-scrollbar + right column clipped at the viewport edge.)
    auto* columns = new QWidget(m_div);
    auto* colsLayout = new QHBoxLayout(columns);
    colsLayout->setContentsMargins(0, 0, 0, 0);
    auto* frameColumnW = new QWidget(columns);
    auto* frameColumn = new QVBoxLayout(frameColumnW);
    auto* globalColumnW = new QWidget(columns);
    auto* globalColumn = new QVBoxLayout(globalColumnW);
    frameColumn->setSpacing(0);
    globalColumn->setSpacing(0);
    frameColumn->setContentsMargins(0, 0, 0, 0);
    globalColumn->setContentsMargins(0, 0, 0, 0);
    colsLayout->addWidget(frameColumnW, 1);  // td width:50%
    colsLayout->addWidget(globalColumnW, 1); // td width:50%
    divLayout->addWidget(columns);

    // :73 — _div.style.textAlign = "right" (all stat rows right-aligned).
    for (int i = 0; i < kNumTileStatEntries; ++i) {
        auto* elem = makeStatLabel(columns, Qt::AlignRight | Qt::AlignVCenter);
        m_statElements.append(elem);
        (kTileStatEntries[i].global ? globalColumn : frameColumn)->addWidget(elem);
    }

    // Reset button (:98-104 — centered, tooltip "Reset all cumulative statistics").
    auto* resetBtn = new QPushButton(QStringLiteral("Reset"), m_div);
    resetBtn->setToolTip(QStringLiteral("Reset all cumulative statistics"));
    connect(resetBtn, &QPushButton::clicked, this, [this]() { reset(); });
    divLayout->addWidget(resetBtn, 0, Qt::AlignHCenter);

    layout->addWidget(m_div);
}

TileStatisticsTracker::~TileStatisticsTracker()
{
    // :109-111 [Symbol.dispose] → clearInterval.
    if (m_interval) {
        m_interval->stop();
        m_interval->deleteLater();
    }
}

// Ported from: TileStatisticsTracker.ts:113-133 addMaxActive — "Max Active
// Requests: " + numeric input (min 0, step 1, initial rpcConcurrency).
void TileStatisticsTracker::addMaxActive()
{
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(QStringLiteral("Max Active Requests: "), row);
    h->addWidget(label);
    m_maxActive = new QSpinBox(row);
    m_maxActive->setObjectName(QStringLiteral("maxActiveRequests"));  // reference input id
    m_maxActive->setMinimum(0);
    m_maxActive->setSingleStep(1);
    // initial = tileAdmin.channels.rpcConcurrency (DanQing: maxConcurrentRequests).
    m_maxActive->setValue(static_cast<int>(dqRender::TileAdmin::instance().getMaxConcurrentRequests()));
    h->addWidget(m_maxActive);
    h->addStretch(1);
    // updateMaxActive (:135-137) → setRpcConcurrency.
    connect(m_maxActive, &QSpinBox::valueChanged, this, [](int value) {
        dqRender::TileAdmin::instance().setMaxConcurrentRequests(static_cast<uint32_t>(value));
    });
    layout()->addWidget(row);
}

// Ported from: TileStatisticsTracker.ts:146-157 toggle.
void TileStatisticsTracker::toggle()
{
    if (m_interval) {
        m_div->hide();
        m_interval->stop();
        m_interval->deleteLater();
        m_interval = nullptr;
    } else {
        m_div->show();
        update();
        m_interval = new QTimer(this);
        m_interval->setInterval(500);
        connect(m_interval, &QTimer::timeout, this, [this]() { update(); });
        m_interval->start();
    }
}

// Ported from: TileStatisticsTracker.ts:159-166 update — `${label}: ${value}`
// per statEntries[i] (values per the reference's getValue lambdas, :30-47).
void TileStatisticsTracker::update()
{
    auto const& stats = dqRender::TileAdmin::instance().statistics();
    dqRender::TileAdmin::SelectedAndReadyTiles tiles;
    bool const hasTiles = m_vp && dqRender::TileAdmin::instance().getTilesForUser(*m_vp, tiles);
    uint32_t const externalRequested = hasTiles ? tiles.external.requested : 0;

    for (int i = 0; i < kNumTileStatEntries; ++i) {
        QString value;
        switch (i) {
        case 0: value = QString::number(stats.numActiveRequests + externalRequested); break;  // Active
        case 1: value = QString::number(stats.numPendingRequests); break;                     // Pending
        case 2: value = QString::number(stats.numCanceled); break;                            // Canceled
        case 3: value = QString::number(stats.numActiveRequests + stats.numPendingRequests); break;  // Total
        case 4: value = QString::number(m_vp ? m_vp->numSelectedTiles() : 0); break;         // Selected
        case 5: value = QString::number(m_vp ? m_vp->numReadyTiles() : 0); break;            // Ready
        case 6: value = QString::number(m_vp ? computeTileProgress(m_vp) : 100); break;      // Progress
        case 7: value = QString::number(stats.totalCompletedRequests); break;                 // Completed
        case 8: value = QString::number(stats.totalTimedOutRequests); break;                  // Timed Out
        case 9: value = QString::number(stats.totalFailedRequests); break;                    // Failed
        case 10: value = QString::number(stats.totalEmptyTiles); break;                       // Empty
        case 11: value = QString::number(stats.totalUndisplayableTiles); break;               // Undisplayable
        case 12: value = QString::number(stats.totalElidedTiles); break;                      // Elided
        case 13: value = QString::number(stats.totalCacheMisses); break;                      // Cache Misses
        case 14: value = QString::number(stats.totalDispatchedRequests); break;               // Dispatched
        case 15: value = QString::number(stats.totalAbortedRequests); break;                  // Aborted
        case 16: value = QString::number(std::lround(stats.decoding.mean)); break;            // Decoding mean
        case 17: value = QString::number(stats.decoding.max); break;                          // Decoding max
        }
        m_statElements[i]->setText(
            QStringLiteral("%1: %2").arg(QLatin1String(kTileStatEntries[i].label), value));
    }
}

// Ported from: TileStatisticsTracker.ts:168-171 reset.
void TileStatisticsTracker::reset()
{
    dqRender::TileAdmin::instance().resetStatistics();
    update();
}

// ===========================================================================
// TileMemoryBreakdown
// Ported from: frontend-devtools/src/widgets/TileMemoryBreakdown.ts
// ===========================================================================

namespace {

// TileMemorySelector — TileMemoryBreakdown.ts:18-25.
enum class TileMemorySelector {
    Selected,    // tiles selected for display by at least one viewport
    Ancestors,   // ancestors of selected tiles, not themselves selected
    Descendants, // descendants of selected tiles, not themselves selected
    Orphaned,    // not selected, no ancestors nor descendants selected
    Total,
    Count,
};

char const* const kTileMemoryLabels[] = { "Selected", "Ancestors", "Descendants", "Orphaned", "Total" };

// TileMemoryTracer — TileMemoryBreakdown.ts:27-119 (the right-pane walk).
class TileMemoryTracer {
public:
    struct Counter {
        uint32_t numTiles = 0;
        uint64_t bytesUsed = 0;
    };
    QVector<Counter> counters;
    uint32_t numSelected = 0;

    TileMemoryTracer() : counters(static_cast<int>(TileMemorySelector::Count)) {}

    void update()
    {
        // :38-69 update() — reset; collect selected tiles across all viewports;
        // classify selected/ancestors/descendants; orphan walk over all trees.
        for (auto& c : counters) {
            c.numTiles = 0;
            c.bytesUsed = 0;
        }
        m_processedTiles.clear();
        numSelected = 0;

        std::set<dqRender::Tile*> selectedTiles;
        auto& viewManager = dqApp::Application::Get().GetViewManager();
        std::set<dqRender::TileTree*> allTrees;
        for (size_t i = 0; i < viewManager.GetViewportCount(); ++i) {
            auto* vp = viewManager.GetViewport(i);
            if (!vp)
                continue;
            dqRender::TileAdmin::SelectedAndReadyTiles tiles;
            if (dqRender::TileAdmin::instance().getTilesForUser(*vp, tiles) && tiles.selected) {
                for (auto* tile : *tiles.selected)
                    if (tile)
                        selectedTiles.insert(tile);
            }
            // EQUIVALENCE（:59-65 imodels.tiles.forEachTreeOwner）：DanQing 无
            // Tiles/TileTreeSupplier 注册表——以全部视口披露的树全集代之
            // （验证法：注册表落地后两集合应一致）。
            std::vector<dqRender::TileTree*> trees;
            vp->discloseTileTrees(trees);
            for (auto* tree : trees)
                if (tree)
                    allTrees.insert(tree);
        }

        for (auto* selected : selectedTiles)
            add(selected, TileMemorySelector::Selected);

        for (auto* selected : selectedTiles) {
            processParent(selected->getParent());
            for (auto* child : selected->getChildren())
                processChildren(child);
        }

        for (auto* tree : allTrees) {
            if (auto* root = tree->getRootTile())
                processOrphan(root);
        }

        uint32_t totalTiles = 0;
        uint64_t totalBytes = 0;
        for (auto const& c : counters) {
            totalTiles += c.numTiles;
            totalBytes += c.bytesUsed;
        }
        counters[static_cast<int>(TileMemorySelector::Total)].numTiles = totalTiles;
        counters[static_cast<int>(TileMemorySelector::Total)].bytesUsed = totalBytes;
    }

private:
    std::set<dqRender::Tile*> m_processedTiles;

    // :78-89 add — collectStatistics(stats, false); only tiles consuming bytes count.
    void add(dqRender::Tile* tile, TileMemorySelector selector)
    {
        m_processedTiles.insert(tile);
        dqRender::RenderMemory::Statistics stats;
        tile->collectStatistics(stats, false);
        uint64_t const bytesUsed = stats.totalBytes;
        if (bytesUsed > 0) {
            auto& counter = counters[static_cast<int>(selector)];
            ++counter.numTiles;
            counter.bytesUsed += bytesUsed;
        }
    }

    // :91-97 processParent.
    void processParent(dqRender::Tile* parent)
    {
        if (parent && !m_processedTiles.count(parent)) {
            add(parent, TileMemorySelector::Ancestors);
            processParent(parent->getParent());
        }
    }

    // :98-108 processChildren.
    void processChildren(dqRender::Tile* child)
    {
        if (!child || m_processedTiles.count(child))
            return;
        add(child, TileMemorySelector::Descendants);
        for (auto* grandchild : child->getChildren())
            processChildren(grandchild);
    }

    // :110-118 processOrphan.
    void processOrphan(dqRender::Tile* tile)
    {
        if (!m_processedTiles.count(tile))
            add(tile, TileMemorySelector::Orphaned);
        for (auto* child : tile->getChildren())
            processOrphan(child);
    }
};

// format — TileMemoryBreakdown.ts:123-125.
QString formatTileMemory(uint32_t count, char const* label, uint64_t bytesUsed)
{
    return QStringLiteral("%1 %2: %3")
        .arg(count)
        .arg(QLatin1String(label), MemoryTracker::formatMemory(static_cast<double>(bytesUsed)));
}

}  // namespace

// Ported from: TileMemoryBreakdown.ts:149-189 constructor.
TileMemoryBreakdown::TileMemoryBreakdown(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_checkBox = new QCheckBox(QStringLiteral("Tile Memory Breakdown"), this);
    layout->addWidget(m_checkBox);
    connect(m_checkBox, &QCheckBox::clicked, this, [this]() { toggle(); });

    m_div = new QWidget(this);
    m_div->hide();
    auto* divLayout = new QVBoxLayout(m_div);
    divLayout->setContentsMargins(0, 0, 0, 0);

    // :161-172 — table width:100%, leftCell/rightCell width:50% (stretch with
    // the window); :159 — _div textAlign:right.
    auto* columns = new QWidget(m_div);
    auto* colsLayout = new QHBoxLayout(columns);
    colsLayout->setContentsMargins(0, 0, 0, 0);
    auto* leftCellW = new QWidget(columns);
    auto* leftCell = new QVBoxLayout(leftCellW);
    auto* rightCellW = new QWidget(columns);
    auto* rightCell = new QVBoxLayout(rightCellW);
    leftCell->setSpacing(0);
    rightCell->setSpacing(0);
    leftCell->setContentsMargins(0, 0, 0, 0);
    rightCell->setContentsMargins(0, 0, 0, 0);
    colsLayout->addWidget(leftCellW, 1);   // td width:50%
    colsLayout->addWidget(rightCellW, 1);  // td width:50%
    divLayout->addWidget(columns);

    for (int i = 0; i < static_cast<int>(TileMemorySelector::Count); ++i) {
        auto* elem = makeStatLabel(columns, Qt::AlignRight | Qt::AlignVCenter);
        m_statsElements.append(elem);
        rightCell->addWidget(elem);
    }
    for (int i = 0; i < 3; ++i) {
        auto* elem = makeStatLabel(columns, Qt::AlignRight | Qt::AlignVCenter);
        m_totalsElements.append(elem);
        leftCell->addWidget(elem);
    }

    layout->addWidget(m_div);
}

TileMemoryBreakdown::~TileMemoryBreakdown()
{
    if (m_interval) {
        m_interval->stop();
        m_interval->deleteLater();
    }
}

// Ported from: TileMemoryBreakdown.ts:195-204 toggle.
void TileMemoryBreakdown::toggle()
{
    if (m_interval) {
        m_div->hide();
        m_interval->stop();
        m_interval->deleteLater();
        m_interval = nullptr;
    } else {
        m_div->show();
        update();
        m_interval = new QTimer(this);
        m_interval->setInterval(500);
        connect(m_interval, &QTimer::timeout, this, [this]() { update(); });
        m_interval->start();
    }
}

// Ported from: TileMemoryBreakdown.ts:213-237 update.
void TileMemoryBreakdown::update()
{
    TileMemoryTracer tracer;
    tracer.update();
    for (int i = 0; i < m_statsElements.size(); ++i) {
        auto const& counter = tracer.counters[i];
        m_statsElements[i]->setText(
            formatTileMemory(counter.numTiles, kTileMemoryLabels[i], counter.bytesUsed));
    }

    // Left pane — TileAdmin loaded-tile totals (:220-236).
    auto& admin = dqRender::TileAdmin::instance();
    uint32_t numSelected = 0;
    uint64_t selectedBytes = 0;
    admin.forEachSelectedLoadedTile([&](dqRender::Tile& tile) {
        ++numSelected;
        selectedBytes += tile.getBytesUsed();
    });
    uint32_t numUnselected = 0;
    uint64_t unselectedBytes = 0;
    admin.forEachUnselectedLoadedTile([&](dqRender::Tile& tile) {
        ++numUnselected;
        unselectedBytes += tile.getBytesUsed();
    });
    m_totalsElements[0]->setText(formatTileMemory(numSelected, "Selected", selectedBytes));
    m_totalsElements[1]->setText(formatTileMemory(numUnselected, "Unselected", unselectedBytes));
    m_totalsElements[2]->setText(formatTileMemory(
        numSelected + numUnselected, "Total", admin.totalTileContentBytes()));
}

// ===========================================================================
// RenderCommandBreakdown
// Ported from: frontend-devtools/src/widgets/RenderCommandBreakdown.ts
// ===========================================================================

// Ported from: RenderCommandBreakdown.ts:19-35 constructor.
RenderCommandBreakdown::RenderCommandBreakdown(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_checkBox = new QCheckBox(QStringLiteral("Render Commands"), this);
    layout->addWidget(m_checkBox);
    connect(m_checkBox, &QCheckBox::clicked, this, [this]() { toggle(); });

    m_div = new QWidget(this);
    m_div->hide();
    auto* divLayout = new QVBoxLayout(m_div);
    divLayout->setContentsMargins(0, 0, 0, 0);

    // :29 — _div textAlign:right.
    m_total = makeStatLabel(m_div, Qt::AlignRight | Qt::AlignVCenter);
    m_total->setText(QStringLiteral("Total: 0"));
    divLayout->addWidget(m_total);

    layout->addWidget(m_div);
}

RenderCommandBreakdown::~RenderCommandBreakdown()
{
    if (m_interval) {
        m_interval->stop();
        m_interval->deleteLater();
    }
}

// Ported from: RenderCommandBreakdown.ts:41-50 toggle.
void RenderCommandBreakdown::toggle()
{
    if (m_interval) {
        m_div->hide();
        m_interval->stop();
        m_interval->deleteLater();
        m_interval = nullptr;
    } else {
        m_div->show();
        update();
        m_interval = new QTimer(this);
        m_interval->setInterval(500);
        connect(m_interval, &QTimer::timeout, this, [this]() { update(); });
        m_interval->start();
    }
}

// Ported from: RenderCommandBreakdown.ts:59-78 update — per-name cells from
// debugControl.getRenderCommands() (RenderCommands.dump: exactly the three
// named counters Primitives/Batches/Branches) + Total.
void RenderCommandBreakdown::update()
{
    // IModelApp.viewManager.selectedView?.target.debugControl
    auto* selected = dqApp::Application::Get().GetViewManager().GetSelectedViewport();
    if (!selected)
        return;
    auto* target = selected->renderTarget();
    if (!target)
        return;  // reference: no debugControl → no update

    auto const cmds = target->getRenderCommands();
    struct NamedCommand {
        char const* name;
        uint32_t count;
    } const named[] = {
        { "Primitives", cmds.primitives },
        { "Batches", cmds.batches },
        { "Branches", cmds.branches },
    };

    uint32_t total = 0;
    for (auto const& cmd : named) {
        QLabel* cell = m_cells.value(QLatin1String(cmd.name), nullptr);
        if (!cell) {
            cell = makeStatLabel(m_div, Qt::AlignRight | Qt::AlignVCenter);  // :29 textAlign:right
            m_cells.insert(QLatin1String(cmd.name), cell);
            m_div->layout()->addWidget(cell);  // rows appear in first-seen order
        }
        total += cmd.count;
        cell->setText(QStringLiteral("%1: %2").arg(QLatin1String(cmd.name)).arg(cmd.count));
    }
    m_total->setText(QStringLiteral("Total: %1").arg(total));
}

// ===========================================================================
// MemoryTracker
// Ported from: frontend-devtools/src/widgets/MemoryTracker.ts
// ===========================================================================

// memLabels — MemoryTracker.ts:41-50 (includes "None" at index 0 of the combo;
// MemIndex itself excludes it, None == -1).
char const* const kMemLabels[] = {
    "None",
    "Viewed Tile Trees",
    "Selected Tiles",
    "All Tile Trees",
    "Render Target",
    "Viewport",
    "System",
    "All",
};
// MemIndex — MemoryTracker.ts:26-38.
enum MemIndex {
    kMemNone = -1,
    kMemViewportTileTrees = 0,
    kMemSelectedTiles,
    kMemAllTileTrees,
    kMemRenderTarget,
    kMemViewport,
    kMemSystem,
    kMemAll,
    kMemCount,
};

// Texture consumer labels — MemoryTracker.ts:219 (MemoryPanel "Textures").
char const* const kTextureConsumerLabels[] = {
    "Surface Textures", "Vertex Tables", "Edge Tables", "Feature Tables",
    "Feature Overrides", "Clip Volumes", "Planar Classifiers", "Shadow Maps",
    "Texture Attachments", "Thematic Textures",
};
// Buffer consumer labels — MemoryTracker.ts:220 (MemoryPanel "Buffers").
char const* const kBufferConsumerLabels[] = {
    "Surfaces", "Visible Edges", "Silhouettes", "Polyline Edges", "Indexed Edges",
    "Polylines", "Point Strings", "Point Clouds", "Instances", "Terrain",
    "Reality Mesh",
};

// Ported from: MemoryTracker.ts:114-126 formatMemory.
QString MemoryTracker::formatMemory(double numBytes)
{
    QString suffix = QStringLiteral("b");
    if (numBytes >= 1024) {
        numBytes /= 1024;
        suffix = QStringLiteral("kb");
        if (numBytes >= 1024) {
            numBytes /= 1024;
            suffix = QStringLiteral("mb");
        }
    }
    return QString::number(numBytes, 'f', 2) + suffix;
}

// Ported from: MemoryTracker.ts:128-172 MemoryPanel.
MemoryTracker::MemoryPanel::MemoryPanel(QWidget* parent, QString label, QStringList entryLabels)
    : QWidget(parent)
    , m_label(std::move(label))
    , m_labels(std::move(entryLabels))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    // Panel sits inside a 50% cell of the tracker's textAlign:right div
    // (:194) — header + entries inherit right alignment.
    m_header = makeStatLabel(this, Qt::AlignRight | Qt::AlignVCenter);
    m_header->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_header);
    for (int i = 0; i < m_labels.size(); ++i) {
        auto* elem = makeStatLabel(this, Qt::AlignRight | Qt::AlignVCenter);
        m_elems.append(elem);
        layout->addWidget(elem);
    }
}

// Ported from: MemoryTracker.ts:154-171 update — header `${label}: ${fmt(total)}`;
// per-consumer `${label} (${count}): ${fmt(bytes)}`, hidden when 0.
void MemoryTracker::MemoryPanel::update(
    QVector<dqRender::RenderMemory::Consumers> const& stats, double total)
{
    m_header->setText(QStringLiteral("%1: %2").arg(m_label, formatMemory(total)));
    for (int i = 0; i < m_labels.size(); ++i) {
        auto const& stat = stats[i];
        if (stat.totalBytes == 0) {
            m_elems[i]->hide();
            continue;
        }
        m_elems[i]->show();
        m_elems[i]->setText(QStringLiteral("%1 (%2): %3")
                                .arg(m_labels[i])
                                .arg(stat.count)
                                .arg(formatMemory(static_cast<double>(stat.totalBytes))));
    }
}

// Ported from: MemoryTracker.ts:189-227 constructor.
MemoryTracker::MemoryTracker(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent)
    , m_vp(vp)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    addSelector();

    m_div = new QWidget(this);
    m_div->hide();
    auto* divLayout = new QVBoxLayout(m_div);
    divLayout->setContentsMargins(0, 0, 0, 0);

    // :199-214 — table width:100%, cells width:50% (stretch); :194 — _div
    // textAlign:right (panels inherit it → header + entries right-aligned).
    auto* columns = new QWidget(m_div);
    auto* colsLayout = new QHBoxLayout(columns);
    colsLayout->setContentsMargins(0, 0, 0, 0);
    auto* cell00W = new QWidget(columns);
    auto* cell00 = new QVBoxLayout(cell00W);
    auto* cell01W = new QWidget(columns);
    auto* cell01 = new QVBoxLayout(cell01W);
    cell00->setSpacing(0);
    cell01->setSpacing(0);
    cell00->setContentsMargins(0, 0, 0, 0);
    cell01->setContentsMargins(0, 0, 0, 0);
    colsLayout->addWidget(cell00W, 1);  // td width:50%
    colsLayout->addWidget(cell01W, 1);  // td width:50%

    QStringList textureLabels;
    for (auto const* l : kTextureConsumerLabels)
        textureLabels << QLatin1String(l);
    QStringList bufferLabels;
    for (auto const* l : kBufferConsumerLabels)
        bufferLabels << QLatin1String(l);

    m_textures = new MemoryPanel(columns, QStringLiteral("Textures"), textureLabels);
    cell00->addWidget(m_textures);
    m_buffers = new MemoryPanel(columns, QStringLiteral("Buffers"), bufferLabels);
    cell01->addWidget(m_buffers);

    m_totalElem = makeStatLabel(columns, Qt::AlignRight | Qt::AlignVCenter);
    m_totalElem->setStyleSheet(QStringLiteral("font-weight: bold;"));
    cell00->addWidget(m_totalElem);
    m_totalTreesElem = makeStatLabel(columns, Qt::AlignRight | Qt::AlignVCenter);
    m_totalTreesElem->setStyleSheet(QStringLiteral("font-weight: bold;"));
    cell01->addWidget(m_totalTreesElem);

    divLayout->addWidget(columns);

    // addPurgeButton (:248-260).
    m_purgeButton = new QPushButton(QStringLiteral("Purge"), m_div);
    connect(m_purgeButton, &QPushButton::clicked, this, [this]() { purge(); });
    divLayout->addWidget(m_purgeButton, 0, Qt::AlignHCenter);

    layout->addWidget(m_div);
}

MemoryTracker::~MemoryTracker()
{
    if (m_interval) {
        m_interval->stop();
        m_interval->deleteLater();
    }
}

// Ported from: MemoryTracker.ts:233-246 addSelector — combo with all entries
// (None first), initial value None.
void MemoryTracker::addSelector()
{
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(QStringLiteral("Track Memory: "), row);
    h->addWidget(label);
    m_selector = new QComboBox(row);
    for (int i = kMemNone; i < kMemCount; ++i)
        m_selector->addItem(QLatin1String(kMemLabels[i + 1]));
    h->addWidget(m_selector);
    h->addStretch(1);
    // Reference: entries carry value i (MemIndex.None == -1 … All == 6); the
    // combo's item VALUE equals position - 1 ("None" first at position 0).
    connect(m_selector, qOverload<int>(&QComboBox::activated), this,
            [this](int position) { change(position - 1); });
    layout()->addWidget(row);
}

// calcMem — MemoryTracker.ts:52-105, per-index collector. Returns #tile trees.
int MemoryTracker::collectForIndex(int index, dqRender::RenderMemory::Statistics& stats)
{
    auto& viewManager = dqApp::Application::Get().GetViewManager();
    switch (index) {
    case kMemViewportTileTrees: {
        // :52-57 collectStatisticsForViewedTileTrees.
        m_vp->collectStatistics(stats);
        std::vector<dqRender::TileTree*> trees;
        m_vp->discloseTileTrees(trees);
        return static_cast<int>(trees.size());
    }
    case kMemSelectedTiles: {
        // :59-70 collectStatisticsForSelectedTiles — selected tiles + tree count.
        std::set<dqRender::TileTree*> trees;
        dqRender::TileAdmin::SelectedAndReadyTiles tiles;
        if (dqRender::TileAdmin::instance().getTilesForUser(*m_vp, tiles) && tiles.selected) {
            for (auto* tile : *tiles.selected) {
                if (!tile)
                    continue;
                trees.insert(&tile->getTree());
                tile->collectStatistics(stats, true);
            }
        }
        return static_cast<int>(trees.size());
    }
    case kMemAllTileTrees: {
        // :72-80 collectStatisticsForAllTileTrees — EQUIVALENCE
        //（vp.view.iModel.tiles.forEachTreeOwner）：DanQing 无注册表，取全部视口
        // 披露树的并集逐树统计（注册表落地后换回 owner 遍历）。
        std::set<dqRender::TileTree*> trees;
        for (size_t i = 0; i < viewManager.GetViewportCount(); ++i) {
            if (auto* vp = viewManager.GetViewport(i)) {
                std::vector<dqRender::TileTree*> disclosed;
                vp->discloseTileTrees(disclosed);
                for (auto* t : disclosed)
                    if (t)
                        trees.insert(t);
            }
        }
        for (auto* tree : trees)
            tree->collectStatistics(stats);
        return static_cast<int>(trees.size());
    }
    case kMemRenderTarget: {
        // :86-90 vp.target.collectStatistics(stats).
        if (auto* target = m_vp->renderTarget())
            target->collectStatistics(stats);
        return 0;
    }
    case kMemViewport: {
        // :91-95 target + viewed tile trees.
        if (auto* target = m_vp->renderTarget())
            target->collectStatistics(stats);
        m_vp->collectStatistics(stats);
        std::vector<dqRender::TileTree*> trees;
        m_vp->discloseTileTrees(trees);
        return static_cast<int>(trees.size());
    }
    case kMemSystem: {
        // :96-99 renderSystem.collectStatistics(stats).
        dqRender::RenderSystem::get().collectStatistics(stats);
        return 0;
    }
    case kMemAll: {
        // :100-104 system + every viewport target + all tile trees.
        dqRender::RenderSystem::get().collectStatistics(stats);
        for (size_t i = 0; i < viewManager.GetViewportCount(); ++i) {
            if (auto* vp = viewManager.GetViewport(i)) {
                if (auto* target = vp->renderTarget())
                    target->collectStatistics(stats);
            }
        }
        return collectForIndex(kMemAllTileTrees, stats);
    }
    default:
        return 0;
    }
}

// Ported from: MemoryTracker.ts:278-297 change — None clears; else 1s interval
// + show; purge enabled only where purgeMem[i] is defined (:108-111 — only
// "Selected Tiles" maps to viewManager.purgeTileTrees).
void MemoryTracker::change(int newIndex)
{
    if (newIndex == m_memIndex)
        return;
    m_memIndex = newIndex;
    if (newIndex == kMemNone) {
        if (m_interval) {
            m_interval->stop();
            m_interval->deleteLater();
            m_interval = nullptr;
        }
        m_div->hide();
        return;
    }
    if (!m_interval) {
        m_interval = new QTimer(this);
        m_interval->setInterval(1000);
        connect(m_interval, &QTimer::timeout, this, [this]() { update(); });
        m_interval->start();
        m_div->show();
    }
    // purgeMem (:108-111): [undefined, purgeTileTrees] — Purge enabled only
    // for combo index 1 ("Selected Tiles" ↔ MemIndex.SelectedTiles).
    m_purgeButton->setEnabled(newIndex == kMemSelectedTiles);
    update();
}

// Ported from: MemoryTracker.ts:299-308 update.
void MemoryTracker::update()
{
    dqRender::RenderMemory::Statistics stats;
    int const numTrees = collectForIndex(m_memIndex, stats);
    m_totalElem->setText(
        QStringLiteral("Total: %1").arg(formatMemory(static_cast<double>(stats.totalBytes))));
    m_totalTreesElem->setText(QStringLiteral("Total Tile Trees: %1").arg(numTrees));

    QVector<dqRender::RenderMemory::Consumers> textureStats;
    for (size_t i = 0; i < stats.consumers.size(); ++i)
        textureStats.append(stats.consumers[i]);
    QVector<dqRender::RenderMemory::Consumers> bufferStats;
    for (size_t i = 0; i < stats.buffers.consumers.size(); ++i)
        bufferStats.append(stats.buffers.consumers[i]);

    m_textures->update(textureStats,
                       static_cast<double>(stats.totalBytes - stats.buffers.totalBytes));
    m_buffers->update(bufferStats, static_cast<double>(stats.buffers.totalBytes));
}

// Ported from: MemoryTracker.ts:310-317 purge.
void MemoryTracker::purge()
{
    // purgeMem[1] → viewManager.purgeTileTrees(now) + invalidateScene + update.
    if (m_memIndex == kMemSelectedTiles) {
        dqApp::Application::Get().GetViewManager().purgeTileTrees(0.0);
        if (m_vp)
            m_vp->InvalidateScene();  // to trigger reloading of tiles we want to keep
        update();
    }
}

// ===========================================================================
// GpuProfiler
// Ported from: frontend-devtools/src/widgets/GpuProfiler.ts
// ===========================================================================

namespace {

// createTraceEvent — GpuProfiler.ts:45-54 (Chrome Trace Event Format "X" events).
QJsonObject createTraceEvent(QString const& name, double startUs, double durUs)
{
    return QJsonObject{
        { "pid", 1 },
        { "ts", startUs },
        { "dur", durUs },
        { "ph", QStringLiteral("X") },
        { "name", name },
        { "args", QJsonObject{ { QStringLiteral("0"), 0 } } },
    };
}

// createTraceFromTimerResults — GpuProfiler.ts:56-83 (Frame N wrappers with
// nested children; <100ns entries skipped — noise).
QJsonArray createTraceFromTimerResults(QList<dqRender::GLTimerResult> const& timerResults)
{
    QJsonArray traceEvents;
    std::function<void(double, QList<dqRender::GLTimerResult> const&)> addChildren =
        [&](double startTime, QList<dqRender::GLTimerResult> const& children) {
            for (auto const& child : children) {
                if (child.nanoseconds < 100)
                    continue;
                double const microseconds = static_cast<double>(child.nanoseconds) / 1e3;
                traceEvents.append(createTraceEvent(
                    QString::fromStdString(child.label), startTime, microseconds));
                addChildren(startTime, { child.children.begin(), child.children.end() });
                startTime += microseconds;
            }
        };

    double frameStartTime = 0.0;
    int frameNumber = 0;
    for (auto const& tr : timerResults) {
        double const microseconds = static_cast<double>(tr.nanoseconds) / 1e3;
        traceEvents.append(createTraceEvent(QStringLiteral("Frame %1").arg(frameNumber),
                                            frameStartTime, microseconds));
        addChildren(frameStartTime, { tr.children.begin(), tr.children.end() });
        frameStartTime += microseconds;
        ++frameNumber;
    }
    return traceEvents;
}

}  // namespace

// Ported from: GpuProfiler.ts:104-137 constructor.
GpuProfiler::GpuProfiler(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_checkBox = new QCheckBox(QStringLiteral("Profile GPU"), this);
    layout->addWidget(m_checkBox);
    connect(m_checkBox, &QCheckBox::clicked, this, [this](bool checked) {
        toggleProfileCheckBox(checked);
    });

    // :114-117 — timer queries unavailable → checkbox disabled + tooltip.
    // (DanQing RHI: isGLTimerSupported == false until timer queries land — the
    // reference's exact "EXT_disjoint_timer_query is not available" path.)
    auto* debugControl = dqRender::RenderSystem::get().debugControl();
    if (!debugControl || !debugControl->isGLTimerSupported) {
        m_checkBox->setEnabled(false);
        m_checkBox->setToolTip(
            QStringLiteral("EXT_disjoint_timer_query is not available in this browser"));
    }

    m_div = new QWidget(this);
    m_div->hide();

    m_recordButton = new QPushButton(QStringLiteral("Record Profile"), m_div);
    m_recordButton->setToolTip(
        QStringLiteral("Record a profile to open with chrome://tracing"));
    connect(m_recordButton, &QPushButton::clicked, this, [this]() { clickRecord(); });
    auto* divLayout = new QVBoxLayout(m_div);
    divLayout->setContentsMargins(0, 0, 0, 0);
    divLayout->addWidget(m_recordButton, 0, Qt::AlignHCenter);

    m_resultsDiv = new QWidget(m_div);
    auto* resultsLayout = new QVBoxLayout(m_resultsDiv);
    resultsLayout->setContentsMargins(0, 0, 0, 0);
    resultsLayout->setSpacing(0);
    divLayout->addWidget(m_resultsDiv);

    layout->addWidget(m_div);
}

// :139-141 [Symbol.dispose] — resultsCallback = undefined.
GpuProfiler::~GpuProfiler()
{
    if (auto* debugControl = dqRender::RenderSystem::get().debugControl())
        debugControl->resultsCallback = nullptr;
}

// Ported from: GpuProfiler.ts:143-153 toggleProfileCheckBox.
void GpuProfiler::toggleProfileCheckBox(bool isEnabled)
{
    auto* debugControl = dqRender::RenderSystem::get().debugControl();
    if (!debugControl)
        return;
    if (isEnabled) {
        debugControl->resultsCallback =
            [this](dqRender::GLTimerResult const& result) { resultsCallback(result); };
        // :145 — _resultsDiv.innerHTML = ""（清空结果区重来）。行部件随条目
        // 持有（resultsCallback 的 EQUIVALENCE），清空 m_results 时一并 delete。
        for (auto& value : m_results)
            delete value.row;  // 子部件（label/leader/value）随 row 销毁
        m_results.clear();
        m_resultsDiv->hide();
        m_div->show();
    } else {
        debugControl->resultsCallback = nullptr;
        m_div->hide();
        stopRecording();
    }
}

// Ported from: GpuProfiler.ts:155-163 _clickRecord.
void GpuProfiler::clickRecord()
{
    if (!m_isRecording) {
        m_isRecording = true;
        m_recordButton->setText(QStringLiteral("Stop Recording"));
        return;
    }
    stopRecording();
}

// Ported from: GpuProfiler.ts:165-175 stopRecording — export chrome trace JSON.
void GpuProfiler::stopRecording()
{
    m_isRecording = false;
    m_recordButton->setText(QStringLiteral("Record Profile"));

    if (!m_recordedResults.isEmpty()) {
        QJsonObject const chromeTrace{
            { "traceEvents", createTraceFromTimerResults(m_recordedResults) }
        };
        // saveAs(blob, "gpu-profile.json") — QSaveFile for an atomic write.
        QString const path = QFileDialog::getSaveFileName(
            nullptr, QStringLiteral("Save GPU Profile"), QStringLiteral("gpu-profile.json"),
            QStringLiteral("JSON (*.json)"));
        if (!path.isEmpty()) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(chromeTrace).toJson(QJsonDocument::Compact));
                file.commit();
            }
        }
        m_recordedResults.clear();
    }
}

// Ported from: GpuProfiler.ts:177-251 _resultsCallback — rolling 120-frame
// average per label, tree indent, <100ns high-pass, "Read Pixels" pinned last.
void GpuProfiler::resultsCallback(dqRender::GLTimerResult const& result)
{
    if (m_isRecording)
        m_recordedResults.append(result);

    int const numSavedFrames = 120;
    QString lastValue;
    QVector<bool> changedResults(m_results.size(), false);

    std::function<void(int, dqRender::GLTimerResult const&)> printDepth =
        [&](int depth, dqRender::GLTimerResult const& currentRes) {
            int index = -1;
            for (int i = 0; i < m_results.size(); ++i) {
                if (m_results[i].label == QString::fromStdString(currentRes.label)) {
                    index = i;
                    break;
                }
            }
            if (index < 0) {  // Add brand new entry (:187-204)
                Results data;
                data.label = QString::fromStdString(currentRes.label);
                data.paddingLeft = depth;
                data.sum = static_cast<double>(currentRes.nanoseconds);
                data.values.append(static_cast<double>(currentRes.nanoseconds));
                if (lastValue.isEmpty()) {
                    m_results.prepend(data);
                    changedResults.prepend(true);
                } else if (data.label == QStringLiteral("Read Pixels")) {
                    m_results.append(data);  // Read Pixels goes at the end
                    changedResults.append(true);
                } else {
                    int prevIndex = -1;
                    for (int i = 0; i < m_results.size(); ++i) {
                        if (m_results[i].label == lastValue) {
                            prevIndex = i;
                            break;
                        }
                    }
                    m_results.insert(prevIndex + 1, data);
                    changedResults.insert(prevIndex + 1, true);
                }
            } else {  // Edit old entry (:205-215)
                Results& savedResults = m_results[index];
                double oldVal = 0.0;
                if (savedResults.values.size() >= numSavedFrames)
                    oldVal = savedResults.values.takeFirst();
                double const newVal =
                    static_cast<double>(currentRes.nanoseconds) < 100.0
                        ? 0.0
                        : static_cast<double>(currentRes.nanoseconds);  // high-pass
                savedResults.sum += newVal - oldVal;
                savedResults.values.append(newVal);
                changedResults[index] = true;
            }
            lastValue = QString::fromStdString(currentRes.label);

            for (auto const& childRes : currentRes.children)
                printDepth(depth + 1, childRes);
        };
    printDepth(0, result);

    // Rebuild the results view (:226-250): label + dotted leader + ms value.
    for (int i = 0; i < m_results.size(); ++i) {
        if (!changedResults.value(i, false)) {  // no data this frame → 0.0
            Results& value = m_results[i];
            double const oldVal =
                value.values.size() >= numSavedFrames && !value.values.isEmpty()
                    ? value.values.takeFirst()
                    : 0.0;
            value.sum -= oldVal;
            value.values.append(0.0);
        }
    }

    // 行同步：label + dotted leader + ms value（:226-250 的行结构）。
    //
    // EQUIVALENCE: 参考源=GpuProfiler.ts:226-250 —— 参考每回调
    // `innerHTML = ""` 后整段重建 fragment；浏览器 DOM 重建廉价且窗口定高不
    // 回流。发散=Qt 侧若照搬"每回调删建全部行部件"：①布局高度每帧 0↔N 振荡
    // （面板滚动条持续跳动）②新建部件的首次 paint 是异步的，下一帧回调又把
    // 它们销毁——连续帧下结果区**恒空白**（2026-09-21 用户真实 app 报告
    // "profile 信息没法显示出来，滚动条一直在动"的双根因）。验证法=行随条目
    // 创建一次、插入序与参考 splice 逻辑 1:1（prepend/Read-Pixels-末位/
    // prevIndex+1），此后每回调仅更新 ms 文本：条目集合/顺序/滚动均值/文本
    // 格式与参考逐帧一致（可观察行为不变，仅部件生命周期适配）。
    for (int i = 0; i < m_results.size(); ++i) {
        Results& value = m_results[i];
        if (!value.row) {  // 新条目 → 在显示位 i 建行（序与 m_results 平行）
            value.row = new QWidget(m_resultsDiv);
            auto* h = new QHBoxLayout(value.row);
            h->setContentsMargins(value.paddingLeft * 10, 0, 0, 0);
            h->setSpacing(2);
            auto* textLabel = makeStatLabel(value.row);
            textLabel->setText(value.label);  // ← 此前漏 setText，标签恒空
            h->addWidget(textLabel);
            auto* leader = new QFrame(value.row);
            leader->setFrameShape(QFrame::HLine);
            leader->setStyleSheet(
                QStringLiteral("border: none; border-top: 1px dotted gray; max-height: 1px;"));
            h->addWidget(leader, 1);
            value.textValue = makeStatLabel(value.row);
            h->addWidget(value.textValue);
            static_cast<QVBoxLayout*>(m_resultsDiv->layout())->insertWidget(i, value.row);
        }
        double const ms = value.values.isEmpty()
                              ? 0.0
                              : value.sum / static_cast<double>(value.values.size()) / 1e6;
        value.textValue->setText(QString::number(ms, 'f', 3) + QStringLiteral(" ms"));
    }
    m_resultsDiv->show();
}

// ===========================================================================
// ToolSettingsTracker
// Ported from: frontend-devtools/src/widgets/ToolSettingsTracker.ts
// ===========================================================================

namespace {
// Static expand/collapse persistence across panel open/close (:20, :49-50).
bool s_expandToolSettings = false;
}  // namespace

// Ported from: ToolSettingsTracker.ts:22-220 constructor — collapsible
// "Tool Settings" section with the reference's 10 controls, in order.
ToolSettingsTracker::ToolSettingsTracker(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent)
    , m_vp(vp)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // createNestedMenu — QToolButton arrow + toggleable body (details/summary).
    auto* toggleBtn = new QToolButton(this);
    toggleBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleBtn->setText(QStringLiteral("Tool Settings"));
    toggleBtn->setCheckable(true);
    toggleBtn->setChecked(s_expandToolSettings);
    toggleBtn->setArrowType(s_expandToolSettings ? Qt::DownArrow : Qt::RightArrow);
    layout->addWidget(toggleBtn);

    auto* settingsDiv = new QWidget(this);
    settingsDiv->setVisible(s_expandToolSettings);
    auto* settingsLayout = new QVBoxLayout(settingsDiv);
    settingsLayout->setContentsMargins(8, 0, 0, 0);
    settingsLayout->setSpacing(2);
    connect(toggleBtn, &QToolButton::toggled, this, [toggleBtn, settingsDiv](bool expanded) {
        s_expandToolSettings = expanded;  // :31 — static persists across panels
        toggleBtn->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        settingsDiv->setVisible(expanded);
    });

    // Every handler ends with IModelApp.toolAdmin.exitViewTool() (:45 etc.) —
    // settings apply to the next tool run.
    auto exitViewTool = []() {
        dqApp::Application::Get().GetToolAdmin().exitViewTool();
    };
    auto makeRow = [&](QString const& text, QWidget* input) {
        auto* row = new QWidget(settingsDiv);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        h->addWidget(new QLabel(text, row));
        h->addWidget(input);
        h->addStretch(1);
        settingsLayout->addWidget(row);
    };

    // :36-47 — "Preserve World Up When Rotating" checkbox.
    {
        auto* cb = new QCheckBox(QStringLiteral("Preserve World Up When Rotating"), settingsDiv);
        cb->setChecked(dqApp::ToolSettings::preserveWorldUp);
        connect(cb, &QCheckBox::clicked, this, [exitViewTool](bool) {
            dqApp::ToolSettings::preserveWorldUp = !dqApp::ToolSettings::preserveWorldUp;
            exitViewTool();
        });
        settingsLayout->addWidget(cb);
    }

    // :52-70 — "Animation Duration (ms): " int input (ScreenViewport.animation.time.normal).
    {
        auto* spin = new QSpinBox(settingsDiv);
        spin->setObjectName(QStringLiteral("ts_animationTime"));  // reference input id
        spin->setMinimum(0);
        spin->setSingleStep(1);
        spin->setValue(static_cast<int>(dqApp::Viewport::animation().time.normal));
        connect(spin, &QSpinBox::valueChanged, this, [exitViewTool](int value) {
            dqApp::Viewport::animation().time.normal = static_cast<double>(value);
            exitViewTool();
        });
        makeRow(QStringLiteral("Animation Duration (ms): "), spin);
    }

    // :72-91 — "Pick Radius (inches): " float (min 0, step 0.01).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(0.0);
        spin->setSingleStep(0.01);
        spin->setDecimals(3);
        spin->setValue(dqApp::ToolSettings::viewToolPickRadiusInches);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::viewToolPickRadiusInches = value;
            exitViewTool();
        });
        makeRow(QStringLiteral("Pick Radius (inches): "), spin);
    }

    // :94-108 — "Walk Enforce Z Up" checkbox.
    {
        auto* cb = new QCheckBox(QStringLiteral("Walk Enforce Z Up"), settingsDiv);
        cb->setChecked(dqApp::ToolSettings::walkEnforceZUp);
        connect(cb, &QCheckBox::clicked, this, [exitViewTool](bool) {
            dqApp::ToolSettings::walkEnforceZUp = !dqApp::ToolSettings::walkEnforceZUp;
            exitViewTool();
        });
        settingsLayout->addWidget(cb);
    }

    // :110-129 — "Walk Camera Angle (degrees): " float (min 0, step 0.1).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(0.0);
        spin->setSingleStep(0.1);
        spin->setDecimals(2);
        spin->setValue(dqApp::ToolSettings::walkCameraAngle.Degrees());
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::walkCameraAngle.SetDegrees(value);
            exitViewTool();
        });
        makeRow(QStringLiteral("Walk Camera Angle (degrees): "), spin);
    }

    // :131-149 — "Walk Velocity (meters per second): " float (min 0, step 0.1).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(0.0);
        spin->setSingleStep(0.1);
        spin->setDecimals(2);
        spin->setValue(dqApp::ToolSettings::walkVelocity);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::walkVelocity = value;
            exitViewTool();
        });
        makeRow(QStringLiteral("Walk Velocity (meters per second): "), spin);
    }

    // :151-170 — "Wheel Zoom Bump Distance (meters): " float (min 0, step 0.025).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(0.0);
        spin->setSingleStep(0.025);
        spin->setDecimals(4);
        spin->setValue(dqApp::ToolSettings::wheelZoomBumpDistance);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::wheelZoomBumpDistance = value;
            exitViewTool();
        });
        makeRow(QStringLiteral("Wheel Zoom Bump Distance (meters): "), spin);
    }

    // :172-190 — "Wheel Zoom Ratio: " float (min 1.0, step 0.025).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(1.0);
        spin->setSingleStep(0.025);
        spin->setDecimals(3);
        spin->setValue(dqApp::ToolSettings::wheelZoomRatio);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::wheelZoomRatio = value;
            exitViewTool();
        });
        makeRow(QStringLiteral("Wheel Zoom Ratio: "), spin);
    }

    // :192-205 — "Inertial damping: " float (min 0, max 1, step 0.05).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(0.0);
        spin->setMaximum(1.0);
        spin->setSingleStep(0.05);
        spin->setDecimals(3);
        spin->setValue(dqApp::ToolSettings::viewingInertia.damping);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::viewingInertia.damping = value;
            exitViewTool();
        });
        makeRow(QStringLiteral("Inertial damping: "), spin);
    }

    // :206-219 — "Inertial duration (seconds): " float (min 0, max 10, step 0.5).
    {
        auto* spin = new QDoubleSpinBox(settingsDiv);
        spin->setMinimum(0.0);
        spin->setMaximum(10.0);
        spin->setSingleStep(0.5);
        spin->setDecimals(2);
        spin->setValue(static_cast<double>(
            dqApp::ToolSettings::viewingInertia.duration.ToMilliseconds()) / 1000.0);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [exitViewTool](double value) {
            dqApp::ToolSettings::viewingInertia.duration =
                dqBase::DqDuration::FromMilliseconds(
                    static_cast<int64_t>(value * 1000.0));
            exitViewTool();
        });
        makeRow(QStringLiteral("Inertial duration (seconds): "), spin);
    }

    layout->addWidget(settingsDiv);
}

}  // namespace Gui
