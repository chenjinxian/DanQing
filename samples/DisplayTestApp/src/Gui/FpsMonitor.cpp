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
#include <cstdio>

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
    m_label = label;
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

    // FpsMonitor.ts:48 —— `FPS${enabled ? ":" : ""}`（2026-10-07 审计 B8：标签
    // 原先恒 "FPS"）。
    if (m_label)
        m_label->setText(QStringLiteral("FPS") + (enabled ? QStringLiteral(":") : QString()));
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
    // Ported from: RecordFpsTool.run (FpsMonitor.ts:110-124)——异步面：onRender
    // 订阅 + 达到帧数经 update 自退（dispose + 恢复 continuousRendering +
    // "FPS {value}" 消息），run 订阅后立即返回。2026-10-07 裁决 D-2：原同步
    // processEvents+RenderLoop 泵循环删除（录制期 UI 冻结与参考不符）。
    // 参考的 PerformanceMetrics 钩（metrics.spfTimes 每帧时长）未移植——DanQing
    // 以 frames/elapsedSec 承载（既有 EQUIVALENCE 登记，见文件头）。
    dqApp::Viewport* vp = activeViewport();
    if (nullptr == vp || 0 >= m_numFramesToRecord)
        return true;

    // 录制窗口内自持（C++ 对应 GC 语义——参考实例由 GC 托管至 update 完成；
    // parseAndRun 的调用尾 delete tool 不应杀活跃录制——审计 D-2 取证实录）。
    // 守卫：被归属所有权管理时（智能指针已接管）跳过 shared_from_this。
    if (!weak_from_this().expired())
        m_keepAlive = shared_from_this();
    m_vp = vp;
    m_hadContinuousRendering = vp->continuousRendering();


    m_numFramesRecorded = 0;
    m_startTime = std::chrono::steady_clock::now();
    auto& viewMgr = dqApp::Application::Get().GetViewManager();
    m_scope.add(viewMgr.OnFinishRender.AddListener([this]() { onUpdate(); }));
    dqApp::Application::Get().GetNotificationManager().OutputMessage(
        dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Info, "Recording..."));
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
    // Ported from: RecordFpsTool.update (FpsMonitor.ts:140-160)——达到帧数：
    // dispose 订阅 + 恢复 continuousRendering + "FPS {value}" + 自退（exitTool）。
    if (++m_numFramesRecorded < m_numFramesToRecord)
        return;

    m_scope.DisconnectAll();
    if (m_vp)
        m_vp->setContinuousRendering(m_hadContinuousRendering);
    m_keepAlive.reset();   // 自持释放——参考 GC 托管的 C++ 对应（D-2）

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
    exitTool();   // 录制完成自退（参考工具完成后续驻留 GC——DanQing 显式面）。
}

}  // namespace Gui
