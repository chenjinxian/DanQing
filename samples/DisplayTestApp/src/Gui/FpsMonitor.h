// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — FPS monitor + frame-recording tool
// Ported from: itwinjs-core display-test-app FpsMonitor.ts (FpsMonitor — the
// status-bar checkbox + label + output that toggles continuous rendering and
// displays a per-second FPS reading; RecordFpsTool — the key-in tool that records
// N frames and reports the average FPS).
//
// Qt mapping (§3.4): checkbox/label/output spans → a QCheckBox + QLabel pair
// mounted in the status bar; requestAnimationFrame pump → ViewManager.
// OnFinishRender subscription; performance.now → std::chrono::steady_clock.
// RecordFpsTool's PerformanceMetrics target hook (FpsMonitor.ts:94/:106) is not
// ported (PerformanceMetrics lives on the render target — TODO registered); the
// FPS figure is frames / elapsed wall time over the same frames, which equals the
// reference's spfTimes.length/spfSum while continuous rendering is active (no
// idle gaps between recorded frames).
#pragma once

#include <QWidget>

#include <chrono>

#include <dqApp/ToolAdmin.h>
#include <dqBase/DqEvent.h>

class QCheckBox;
class QLabel;

namespace dqApp {
class Viewport;
}

namespace Gui {

class FpsMonitor : public QWidget
{
    Q_OBJECT
public:
    // Ported from: FpsMonitorProps (checkbox + label + output elements).
    explicit FpsMonitor(QWidget* parent = nullptr);
    ~FpsMonitor() override;

private:
    // Ported from: FpsMonitor.enabled setter (FpsMonitor.ts:59-76) — toggles
    // continuous rendering on every viewport and starts/stops the counter.
    void setEnabled(bool enabled);
    // Ported from: FpsMonitor.update (FpsMonitor.ts:78-95) — per-second FPS readout.
    void update();

    QCheckBox* m_checkbox = nullptr;
    QLabel* m_output = nullptr;
    bool m_enabled = false;
    int m_frameCount = 0;
    std::chrono::steady_clock::time_point m_prevTime;
    dqBase::DqEventScope m_scope;   // OnViewOpen + OnFinishRender subscriptions
};

// Key-in tool that records a number of frames and reports the average FPS.
// Ported from: RecordFpsTool (FpsMonitor.ts:98-143) — toolId "RecordFps",
// maxArgs 1, keyin "dta record fps" (SVTTools.json tools.RecordFps.keyin).
class RecordFpsTool final : public dqApp::InteractiveTool
{
public:
    // Ported from: RecordFpsTool.toolId (FpsMonitor.ts:99).
    const char* getToolId() const override { return "RecordFps"; }
    // Ported from: RecordFpsTool.maxArgs (FpsMonitor.ts:102-104).
    int maxArgs() const override { return 1; }
    // Ported from: Tool.get englishKeyin — SVTTools.json "tools.RecordFps.keyin".
    std::string englishKeyin() const override { return "dta record fps"; }

    // Ported from: RecordFpsTool.run (FpsMonitor.ts:110-124) — default 150 frames.
    bool run() override;
    // Ported from: RecordFpsTool.parseAndRun (FpsMonitor.ts:126-138) — optional
    // frame count argument.
    bool parseAndRun(std::vector<std::string> const& args) override;

private:
    // Ported from: RecordFpsTool.update (FpsMonitor.ts:140-160) — after N frames,
    // report "FPS {value}" and restore the prior continuous-rendering state.
    void onUpdate();

    int m_numFramesToRecord = 0;
    int m_numFramesRecorded = 0;
    bool m_hadContinuousRendering = false;
    bool m_recording = false;
    std::chrono::steady_clock::time_point m_startTime;
};

}  // namespace Gui
