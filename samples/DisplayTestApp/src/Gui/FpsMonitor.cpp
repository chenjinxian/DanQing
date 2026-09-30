// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — FPS monitor + frame-recording tool implementation
// Ported from: itwinjs-core display-test-app FpsMonitor.ts
#include "FpsMonitor.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

#include <cmath>

namespace Gui {

namespace {
dqApp::Viewport* activeViewport()
{
    return dqApp::Application::Get().GetViewManager().GetActiveViewport();
}
}  // namespace

// ---------------------------------------------------------------------------
// FpsMonitor
// ---------------------------------------------------------------------------

FpsMonitor::FpsMonitor(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("DTA.FpsMonitor"));

    // Ported from: Surface index.html fps-container — checkbox + "FPS" label +
    // output span (FpsMonitorProps.checkbox/label/output).
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(3);
    m_checkbox = new QCheckBox(this);
    m_checkbox->setObjectName(QStringLiteral("DTA.FpsMonitor.Checkbox"));
    auto* label = new QLabel(QStringLiteral("FPS"), this);
    m_output = new QLabel(this);
    m_output->setObjectName(QStringLiteral("DTA.FpsMonitor.Output"));
    layout->addWidget(m_checkbox);
    layout->addWidget(label);
    layout->addWidget(m_output);

    // Ported from: FpsMonitor ctor (FpsMonitor.ts:49-55) — checkbox click drives
    // `enabled`. The reference also flips continuousRendering for viewports opened
    // while enabled (onViewOpen listener, FpsMonitor.ts:51); DanQing's wire-up is
    // the same listener on ViewManager.OnViewOpen.
    connect(m_checkbox, &QCheckBox::toggled, this, [this](bool on) { setEnabled(on); });
    auto& viewMgr = dqApp::Application::Get().GetViewManager();
    m_scope.add(viewMgr.OnViewOpen.AddListener([this](dqApp::Viewport* vp) {
        if (m_enabled && vp)
            vp->setContinuousRendering(true);
    }));
    // The reference's rAF pump (FpsMonitor.ts:78 requestAnimationFrame(update))
    // — DanQing's per-frame event is ViewManager.OnFinishRender.
    m_scope.add(viewMgr.OnFinishRender.AddListener([this]() { update(); }));
}

FpsMonitor::~FpsMonitor()
{
    m_scope.DisconnectAll();
}

void FpsMonitor::setEnabled(bool enabled)
{
    // Ported from: FpsMonitor.enabled setter (FpsMonitor.ts:59-76).
    if (enabled == m_enabled)
        return;

    m_enabled = enabled;
    m_frameCount = 0;
    auto& viewMgr = dqApp::Application::Get().GetViewManager();
    for (size_t i = 0; i < viewMgr.GetViewportCount(); ++i)
        if (auto* vp = viewMgr.GetViewport(i))
            vp->setContinuousRendering(enabled);

    m_output->setText(QString());
    if (enabled)
        m_prevTime = std::chrono::steady_clock::now();
}

void FpsMonitor::update()
{
    if (!m_enabled)
        return;

    // Ported from: FpsMonitor.update (FpsMonitor.ts:78-95) — count frames; every
    // >= 1000ms publish frames*1000/(curTime - prevTime), then reset the window.
    ++m_frameCount;
    auto const curTime = std::chrono::steady_clock::now();
    double const elapsedMs
        = std::chrono::duration<double, std::milli>(curTime - m_prevTime).count();
    if (elapsedMs >= 1000.0) {
        double const fps = (static_cast<double>(m_frameCount) * 1000.0) / elapsedMs;
        m_output->setText(QString::number(fps, 'f', 2));
        m_prevTime = curTime;
        m_frameCount = 0;
    }
}

// ---------------------------------------------------------------------------
// RecordFpsTool
// ---------------------------------------------------------------------------

bool RecordFpsTool::run()
{
    // Ported from: RecordFpsTool.run (FpsMonitor.ts:110-124).
    dqApp::Viewport* vp = activeViewport();
    if (nullptr == vp || 0 >= m_numFramesToRecord)
        return true;

    m_hadContinuousRendering = vp->continuousRendering();
    vp->setContinuousRendering(true);

    // The reference subscribes vp.onRender for the selected viewport and awaits
    // frames across async run() microtasks (FpsMonitor.ts:118 + _dispose at :144).
    // RecordFpsTool instances live only for the parseAndRun call here (the
    // registry deletes them afterwards — GC in the reference), so the record
    // window completes synchronously: pump the render loop until N frames have
    // been observed.
    m_recording = true;
    m_numFramesRecorded = 0;
    m_startTime = std::chrono::steady_clock::now();
    auto& viewMgr = dqApp::Application::Get().GetViewManager();
    auto const token = viewMgr.OnFinishRender.AddListener([this]() { onUpdate(); });
    dqApp::Application::Get().GetNotificationManager().OutputMessage(
        dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Info, "Recording..."));
    while (m_recording) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        viewMgr.RenderLoop();
    }
    token();

    vp->setContinuousRendering(m_hadContinuousRendering);
    return true;
}

bool RecordFpsTool::parseAndRun(std::vector<std::string> const& args)
{
    // Ported from: RecordFpsTool.parseAndRun (FpsMonitor.ts:126-138) — optional
    // frames argument; non-numeric input keeps the default (returns true either way).
    if (1 == args.size()) {
        bool ok = false;
        int const numFramesToRecord = QString::fromStdString(args[0]).toInt(&ok);
        if (!ok)
            return true;
        m_numFramesToRecord = numFramesToRecord;
    }
    else {
        m_numFramesToRecord = 150;
    }

    return run();
}

void RecordFpsTool::onUpdate()
{
    // Ported from: RecordFpsTool.update (FpsMonitor.ts:140-160).
    if (++m_numFramesRecorded < m_numFramesToRecord)
        return;

    m_recording = false;

    // Reference: fps = metrics.spfTimes.length / metrics.spfSum — frames over the
    // summed seconds-per-frame of the same frames. Elapsed wall time equals the
    // sum of frame times while continuous rendering is active (see header note).
    double const elapsedSec
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_startTime).count();
    double const fps
        = elapsedSec > 0.0 ? static_cast<double>(m_numFramesRecorded) / elapsedSec : 0.0;

    dqApp::Application::Get().GetNotificationManager().OutputMessage(
        dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Info,
                                    QStringLiteral("FPS %1").arg(fps, 0, 'f', 2).toStdString()));
}

}  // namespace Gui
