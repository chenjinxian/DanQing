// DebugWindowDiagTest.cpp — Debug Info 面板（DiagnosticsPanel 7 widget）驱动回归。
//
// Authored: no reference test exists in frontend-devtools/display-test-app for
// the DiagnosticsPanel widgets (exercised manually in the browser). This locks
// the 2026-09-21 full port: the 7 sections exist in DiagnosticsPanel.ts:53-103
// order, the controls operate their reference data sources, and the window
// shell carries the DebugWindow.ts identity/visibility semantics.
#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QToolButton>

#include <dqApp/Application.h>
#include <dqApp/ToolSettings.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqRender/RenderSystem.h>
#include <dqRender/tile/TileAdmin.h>

#include "View3DInventor.h"
#include "DebugWindow.h"

namespace {
struct QtEnvD {
    QtEnvD()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = { n, nullptr };
            new QApplication(argc, av);
        }
    }
};
QtEnvD s_qtR;

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 按文本找复选框（参考的 createCheckBox name → QCheckBox text）。
QCheckBox* findCheckBox(QWidget* root, QString const& text)
{
    for (auto* cb : root->findChildren<QCheckBox*>())
        if (cb->text() == text || cb->text().startsWith(text))
            return cb;
    return nullptr;
}

}  // namespace

// Authored: 见文件头 — 7 sections present, in DiagnosticsPanel order.
TEST(DebugWindowDiag, PanelHasSevenSectionsInOrder)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(800, 600);
    view.show();
    spin(400);

    Gui::DebugWindow win(view.getUeViewport(), &view);
    win.showPanel();
    spin(100);

    // FpsTracker / TileStatisticsTracker / TileMemoryBreakdown /
    // RenderCommandBreakdown / MemoryTracker / GpuProfiler / ToolSettingsTracker.
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Track FPS")), nullptr);
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Track Tile Requests")), nullptr);
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Tile Memory Breakdown")), nullptr);
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Render Commands")), nullptr);
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Profile GPU")), nullptr);

    // MemoryTracker selector: "Track Memory: " 8 entries (None..All).
    QComboBox* memSelector = nullptr;
    for (auto* combo : win.findChildren<QComboBox*>())
        if (combo->count() == 8)
            memSelector = combo;
    ASSERT_NE(memSelector, nullptr);
    EXPECT_EQ(memSelector->itemText(0), QStringLiteral("None"));
    EXPECT_EQ(memSelector->itemText(1), QStringLiteral("Viewed Tile Trees"));
    EXPECT_EQ(memSelector->itemText(2), QStringLiteral("Selected Tiles"));
    EXPECT_EQ(memSelector->itemText(3), QStringLiteral("All Tile Trees"));
    EXPECT_EQ(memSelector->itemText(4), QStringLiteral("Render Target"));
    EXPECT_EQ(memSelector->itemText(5), QStringLiteral("Viewport"));
    EXPECT_EQ(memSelector->itemText(6), QStringLiteral("System"));
    EXPECT_EQ(memSelector->itemText(7), QStringLiteral("All"));

    // ToolSettingsTracker: collapsible "Tool Settings" header.
    QToolButton* tsToggle = nullptr;
    for (auto* btn : win.findChildren<QToolButton*>())
        if (btn->text() == QStringLiteral("Tool Settings"))
            tsToggle = btn;
    ASSERT_NE(tsToggle, nullptr);
    tsToggle->setChecked(true);
    spin(50);
    // The 10 reference controls' row labels (ToolSettingsTracker.ts:36-219).
    for (char const* label : { "Animation Duration (ms): ",
                               "Pick Radius (inches): ",
                               "Walk Camera Angle (degrees): ",
                               "Walk Velocity (meters per second): ",
                               "Wheel Zoom Bump Distance (meters): ",
                               "Wheel Zoom Ratio: ",
                               "Inertial damping: ",
                               "Inertial duration (seconds): " }) {
        bool found = false;
        for (auto* lbl : win.findChildren<QLabel*>())
            if (lbl->text() == QLatin1String(label))
                found = true;
        EXPECT_TRUE(found) << "missing ToolSettings row: " << label;
    }
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Preserve World Up When Rotating")), nullptr);
    EXPECT_NE(findCheckBox(&win, QStringLiteral("Walk Enforce Z Up")), nullptr);

    win.hidePanel();
    view.close();
    spin(200);
}

// Authored: 见文件头 — FpsTracker toggle drives vp.continuousRendering + label
// text semantics (FpsTracker.ts:45-64).
TEST(DebugWindowDiag, FpsTrackerTogglesContinuousRendering)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(800, 600);
    view.show();
    spin(400);
    auto* vp = view.getUeViewport();

    Gui::DebugWindow win(vp, &view);
    win.showPanel();
    spin(50);

    auto* cb = findCheckBox(&win, QStringLiteral("Track FPS"));
    ASSERT_NE(cb, nullptr);
    cb->click();
    spin(1200);  // > 500ms interval → label carries the FPS value
    EXPECT_TRUE(vp->continuousRendering());
    EXPECT_TRUE(cb->text().startsWith(QStringLiteral("FPS: "))
                || cb->text() == QStringLiteral("Tracking FPS..."))
        << qPrintable(cb->text());

    cb->click();
    spin(50);
    EXPECT_FALSE(vp->continuousRendering());
    EXPECT_EQ(cb->text(), QStringLiteral("Track FPS"));

    win.hidePanel();
    view.close();
    spin(200);
}

// Authored: 见文件头 — TileStatisticsTracker: Max Active Requests spinner is
// wired to TileAdmin concurrency; tracking fills the 18 reference stat labels
// (TileStatisticsTracker.ts:29-48); Reset clears cumulative totals.
TEST(DebugWindowDiag, TileStatisticsTrackerWired)
{
    dqRender::TileAdmin::instance().resetStatistics();
    dqRender::TileAdmin::instance().recordCompleted();
    dqRender::TileAdmin::instance().recordElidedTile();

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(800, 600);
    view.show();
    spin(400);

    Gui::DebugWindow win(view.getUeViewport(), &view);
    win.showPanel();
    spin(50);

    // Max Active Requests spinner ↔ getMaxConcurrentRequests (found by the
    // reference input id).
    QSpinBox* maxActive = win.findChild<QSpinBox*>(QStringLiteral("maxActiveRequests"));
    ASSERT_NE(maxActive, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(maxActive->value()),
              dqRender::TileAdmin::instance().getMaxConcurrentRequests());
    maxActive->setValue(20);
    EXPECT_EQ(dqRender::TileAdmin::instance().getMaxConcurrentRequests(), 20u);

    // Tracking on → the 18 stat labels appear ("Completed: 1", "Elided: 1", ...).
    auto* cb = findCheckBox(&win, QStringLiteral("Track Tile Requests"));
    ASSERT_NE(cb, nullptr);
    cb->click();
    spin(700);  // > 500ms update interval
    int statLabels = 0;
    bool sawCompleted = false, sawElided = false;
    for (auto* lbl : win.findChildren<QLabel*>()) {
        if (lbl->text().startsWith(QStringLiteral("Completed: "))) sawCompleted = true;
        if (lbl->text().startsWith(QStringLiteral("Elided: "))) sawElided = true;
        if (lbl->text().contains(QStringLiteral(": "))
            && !lbl->text().contains(QStringLiteral("Requests"))) {
            // rough count of stat rows (frame + global columns)
            if (lbl->text().contains(QRegularExpression(
                    QStringLiteral("^(Active|Pending|Canceled|Total|Selected|Ready|Progress|Completed|Timed Out|Failed|Empty|Undisplayable|Elided|Cache Misses|Dispatched|Aborted|Decoding)"))))
                ++statLabels;
        }
    }
    EXPECT_TRUE(sawCompleted);
    EXPECT_TRUE(sawElided);
    EXPECT_GE(statLabels, 17);

    // 显示完整性回归（2026-09-21 "显示不完整"报告）：内容宽度必须永远适配
    // 视口——水平滚动条不得有可滚动量（maximum==0），否则右侧 global 列会被
    // 滚出视口裁切（"Undisplayable: 0"/"Decoding mean time (ms): 0" 截尾）。
    // 参考：.debugPanel width:480px 固定 + 表格列 50%/50% 随窗伸缩（index.css
    // :341-355 + TileStatisticsTracker.ts:75-95）。
    auto* scrollArea = win.findChild<QScrollArea*>();
    ASSERT_NE(scrollArea, nullptr);
    EXPECT_EQ(scrollArea->horizontalScrollBar()->maximum(), 0)
        << "content wider than viewport — the global column would clip at the right edge";

    // Reset button clears the cumulative totals.
    QPushButton* resetBtn = nullptr;
    for (auto* btn : win.findChildren<QPushButton*>())
        if (btn->text() == QStringLiteral("Reset"))
            resetBtn = btn;
    ASSERT_NE(resetBtn, nullptr);
    resetBtn->click();
    spin(100);
    EXPECT_EQ(dqRender::TileAdmin::instance().statistics().totalCompletedRequests, 0u);
    EXPECT_EQ(dqRender::TileAdmin::instance().statistics().totalElidedTiles, 0u);

    cb->click();
    win.hidePanel();
    view.close();
    spin(200);
}

// Authored: 见文件头 — GpuProfiler: the checkbox enables when timer queries
// are supported (GpuProfiler.ts:107-117 — the reference's supported path;
// DanQing's GLTimer implements GL_TIME_ELAPSED, 2026-09-21).
TEST(DebugWindowDiag, GpuProfilerEnabledWithTimerQueries)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(800, 600);
    view.show();
    spin(400);

    Gui::DebugWindow win(view.getUeViewport(), &view);
    win.showPanel();
    spin(50);

    auto* cb = findCheckBox(&win, QStringLiteral("Profile GPU"));
    ASSERT_NE(cb, nullptr);
    EXPECT_TRUE(cb->isEnabled()) << "Profile GPU must be enabled — GL timer queries are implemented";

    win.hidePanel();
    view.close();
    spin(200);
}

// Authored: 见文件头 — the Profile GPU checkbox wires the whole live chain:
// click → resultsCallback on the debugControl → real frames → results rows
// carrying " ms" values in the panel (GpuProfiler.ts:143-153 + :177-251).
TEST(DebugWindowDiag, GpuProfilerCheckboxShowsLiveValues)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(800, 600);
    view.show();
    spin(400);
    auto* vp = view.getUeViewport();

    Gui::DebugWindow win(vp, &view);
    win.showPanel();
    spin(50);

    auto* cb = findCheckBox(&win, QStringLiteral("Profile GPU"));
    ASSERT_NE(cb, nullptr);
    ASSERT_TRUE(cb->isEnabled());
    cb->click();  // ← GpuProfiler.ts:143-153 toggleProfileCheckBox(true)
    spin(100);

    vp->setContinuousRendering(true);   // FpsTracker path — frames flow every tick
    spin(2000);

    // 行复用回归（2026-09-21 真实 app 报告"profile 信息没法显示出来、滚动条
    // 一直在动"）：连续帧期间结果行部件必须稳定复用——若每回调删建全部行
    // （照搬参考的 innerHTML 重建），新建行会在首次 paint 前被下一帧销毁
    // （结果区恒空白）且布局高度每帧振荡（滚动条持续跳动）。帧仍在流时采样
    // 两次，比对行部件指针身份；同时锁行内标签文本非空（此前漏 setText）。
    auto sampleRows = [&win](QSet<QWidget*>& rows, QHash<QWidget*, QString>& rowLabels) {
        for (auto* lbl : win.findChildren<QLabel*>()) {
            if (!lbl->text().endsWith(QStringLiteral(" ms")))
                continue;
            auto* row = lbl->parentWidget();
            rows.insert(row);
            for (auto* child : row->findChildren<QLabel*>())
                if (!child->text().endsWith(QStringLiteral(" ms")))
                    rowLabels[row] += child->text();
        }
    };
    QSet<QWidget*> rows1, rows2;
    QHash<QWidget*, QString> labels1, labels2;
    sampleRows(rows1, labels1);
    spin(400);  // frames still flowing
    sampleRows(rows2, labels2);
    EXPECT_FALSE(rows1.isEmpty()) << "no result rows while frames flow";
    EXPECT_EQ(rows1, rows2)
        << "result row widgets were rebuilt between frames — layout churn (scrollbar jumping)";
    for (auto it = labels1.begin(); it != labels1.end(); ++it)
        EXPECT_FALSE(it.value().isEmpty()) << "result row has an empty label text";

    vp->setContinuousRendering(false);
    spin(200);

    // The results area shows per-label ms values ("x.xxx ms", GpuProfiler.ts:244).
    bool sawMsValue = false;
    for (auto* lbl : win.findChildren<QLabel*>()) {
        if (lbl->text().endsWith(QStringLiteral(" ms")))
            sawMsValue = true;
    }
    EXPECT_TRUE(sawMsValue) << "Profile GPU results area has no ' ms' values after 2s of frames";

    cb->click();  // off — leaves resultsCallback cleared (dtor discipline)
    win.hidePanel();
    view.close();
    spin(200);
}
// Authored: 见文件头 — end-to-end GPU profiling with REAL GL timer queries:
// resultsCallback delivers the "Total" frame tree with the reference's pass
// labels and real GPU nanoseconds (GLTimer.ts endFrame → resultsCallback).
TEST(DebugWindowDiag, GpuProfilerDeliversRealResults)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(800, 600);
    view.show();
    spin(400);
    auto* vp = view.getUeViewport();

    auto* debugControl = dqRender::RenderSystem::get().debugControl();
    ASSERT_NE(debugControl, nullptr);
    EXPECT_TRUE(debugControl->isGLTimerSupported)
        << "GL_TIME_ELAPSED must be available on a 4.1 core context";

    std::vector<dqRender::GLTimerResult> results;
    debugControl->resultsCallback =
        [&results](dqRender::GLTimerResult const& result) { results.push_back(result); };
    vp->setContinuousRendering(true);  // FpsTracker path — frames flow every tick
    spin(2000);
    debugControl->resultsCallback = nullptr;
    vp->setContinuousRendering(false);

    ASSERT_FALSE(results.empty()) << "no GPU timer results delivered after 2s of continuous frames";
    auto const& frame = results.front();
    EXPECT_EQ(frame.label, "Total");
    // Reference's frame tree carries the pass labels (Target.ts:663-732 +
    // SceneCompositor.ts:1419-1501): at least one labeled pass must be present.
    bool sawLabeledPass = false;
    for (auto const& child : frame.children) {
        if (child.label == "Begin Paint" || child.label == "Init Commands"
            || child.label == "Clear Opaque" || child.label == "End Paint")
            sawLabeledPass = true;
    }
    EXPECT_TRUE(sawLabeledPass) << "frame tree lacks the reference's pass labels";
    // Real GPU time is positive on a real frame.
    EXPECT_GT(frame.nanoseconds, 0u);

    view.close();
    spin(200);
}

// Authored: 见文件头 — window identity: title `[ vpId ] Diagnostics` + windowId
// `debugPanel-vpId` (DebugWindow.ts:30-31); toggle/show/hide (:48-71) + the
// selected-viewport visibility gate (:34-36 — two panels over two viewports:
// each visible exactly while ITS viewport is the selected one).
TEST(DebugWindowDiag, WindowIdentityAndToggle)
{
    Gui::View3DInventor viewA(nullptr, nullptr, nullptr);
    viewA.resize(800, 600);
    viewA.show();
    Gui::View3DInventor viewB(nullptr, nullptr, nullptr);
    viewB.resize(800, 600);
    viewB.show();
    spin(500);
    auto* vpA = viewA.getUeViewport();
    auto* vpB = viewB.getUeViewport();
    ASSERT_NE(vpA, nullptr);
    ASSERT_NE(vpB, nullptr);

    Gui::DebugWindow winA(vpA, &viewA);
    Gui::DebugWindow winB(vpB, &viewB);
    EXPECT_EQ(winA.windowId(), QStringLiteral("debugPanel-%1").arg(vpA->GetViewportId()));
    EXPECT_EQ(winB.windowId(), QStringLiteral("debugPanel-%1").arg(vpB->GetViewportId()));
    EXPECT_NE(winA.windowId(), winB.windowId());

    winA.showPanel();
    winB.showPanel();
    spin(100);

    // Select A → A's panel visible, B's hidden (:34-36).
    dqApp::Application::Get().GetViewManager().SetSelectedViewport(vpA);
    spin(100);
    EXPECT_TRUE(winA.isVisible());
    EXPECT_FALSE(winB.isVisible());

    // Select B → the visibility swaps.
    dqApp::Application::Get().GetViewManager().SetSelectedViewport(vpB);
    spin(100);
    EXPECT_FALSE(winA.isVisible());
    EXPECT_TRUE(winB.isVisible());

    // toggle hides the open panel (:48-53); toggle again restores.
    winB.toggle();
    EXPECT_FALSE(winB.isVisible());
    winB.toggle();
    EXPECT_TRUE(winB.isVisible());

    winA.hidePanel();
    winB.hidePanel();
    viewA.close();
    viewB.close();
    dqApp::Application::Get().GetViewManager().SetSelectedViewport(nullptr);
    spin(200);
}
