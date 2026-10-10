// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Animation / solar time 弹出面板
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/Timeline.ts
//              （TimelineProvider 接口 :11-22 + 四 provider :24-126 +
//               TimelinePanel :128-323 + createTimeline :325-327）
//
// M-U U-f。工具栏 Animation 下拉（Viewer.ts:382-386，tooltip
// "Animation / solar time"）。EQUIVALENCE / 裁决：
//  - rAF 帧泵 → QTimer 定时器泵（:273-275 requestAnimationFrame——E5 登记：
//    帧泵语义等价，间隔 15ms 近似 vSync）。
//  - Duration 输入：HTML <input type=number>（:175-184）→ QLineEdit +
//    参考 onchange 语义（parseFloat；NaN/<=0 拒绝——:296-299）。
//  - SolarTimelineProvider 的 cartoCenter：参考 vp.iModel.isGeoLocated 分支
//    （spatialToCartographicFromEcef）+ Philadelphia 回退；DanQing 用
//    IModelConnection::HasEcefLocation + Cartographic::fromEcef（I11 数据面
//    缺席恒 false → 回退支——与 DisplayStyle::setSunTime 门面同形）。
//  - setSunTime 的 invalidateRenderPlan：参考显式调用（:66-68）；DanQing 经
//    OnLightSettingsChanged 事件链（Viewport 监听面既有）到达同一失效——
//    事件链等价登记。
//  - getDescription 的时间串：JS Date.toTimeString/toString → QDateTime
//    本地化短形（信息性 message 行——格式语义等价登记）。
#pragma once

#include <dqBase/DqEvent.h>   // DqEventScope（onViewChanged 订阅）
#include <dqGeom/Range3d.h>  // Range1d

#include <QObject>
#include <memory>
#include <optional>
#include <QString>

class QLabel;
class QLineEdit;
class QSlider;
class QTimer;
class QToolButton;

namespace dqApp {
class Viewport;
}

namespace Gui {

// TimelineProvider — time range + 读写/描述契约。
// Ported from: itwinjs-core TimelineProvider（Timeline.ts:11-22）。
class ITimelineProvider {
public:
    virtual ~ITimelineProvider() = default;

    virtual dqGeom::Range1d const& duration() const = 0;
    virtual QString getDescription(double time) const = 0;
    virtual void update(double time, dqApp::Viewport* vp) = 0;
    virtual double getCurrentTime(dqApp::Viewport* vp) const = 0;
    // Called when user interacts with the timeline (Start / slider adjust).
    // Ported from: TimelineProvider.onInteraction（Timeline.ts:20-21）。
    virtual void onInteraction() {}
};

// Schedule script timeline：vp.timePoint 驱动 script.duration 区间。
// Ported from: itwinjs-core ScheduleTimelineProvider（Timeline.ts:81-99）。
class ScheduleTimelineProvider final : public ITimelineProvider {
public:
    explicit ScheduleTimelineProvider(dqGeom::Range1d duration)
        : m_duration(duration)
    {
    }

    dqGeom::Range1d const& duration() const override { return m_duration; }
    QString getDescription(double time) const override;
    void update(double time, dqApp::Viewport* vp) override;
    double getCurrentTime(dqApp::Viewport* vp) const override;

private:
    dqGeom::Range1d m_duration;
};

// Analysis style timeline：fraction 0..100 ↔ analysisFraction 0..1。
// Ported from: itwinjs-core AnalysisTimelineProvider（Timeline.ts:24-38）。
class AnalysisTimelineProvider final : public ITimelineProvider {
public:
    AnalysisTimelineProvider()
        : m_duration(0.0, 100.0)
    {
    }

    dqGeom::Range1d const& duration() const override { return m_duration; }
    QString getDescription(double time) const override;
    void update(double time, dqApp::Viewport* vp) override;
    double getCurrentTime(dqApp::Viewport* vp) const override;

private:
    dqGeom::Range1d m_duration;
};

// Solar timeline：当日日出—日落区间驱动 setSunTime；_active 门原样
// （首次交互前 update 恒 no-op——Timeline.ts:64-78 的参考怪癖 1:1）。
// Ported from: itwinjs-core SolarTimelineProvider（Timeline.ts:40-79）。
class SolarTimelineProvider final : public ITimelineProvider {
public:
    explicit SolarTimelineProvider(dqApp::Viewport* vp);

    dqGeom::Range1d const& duration() const override { return m_duration; }
    QString getDescription(double time) const override;
    void update(double time, dqApp::Viewport* vp) override;
    double getCurrentTime(dqApp::Viewport* vp) const override;
    void onInteraction() override;

private:
    dqGeom::Range1d m_duration;
    bool m_active = false;  // :42 _active——onInteraction 才置 true
};

// No-op fallback：无可动画内容时的描述面。
// Ported from: itwinjs-core NoOpTimelineProvider（Timeline.ts:101-113）。
class NoOpTimelineProvider final : public ITimelineProvider {
public:
    NoOpTimelineProvider()
        : m_duration(0.0, 100.0)
    {
    }

    dqGeom::Range1d const& duration() const override { return m_duration; }
    QString getDescription(double /*time*/) const override;
    void update(double /*time*/, dqApp::Viewport* /*vp*/) override {}
    double getCurrentTime(dqApp::Viewport* /*vp*/) const override;

private:
    dqGeom::Range1d m_duration;
};

// Provider 自动选择链：Schedule > Analysis > Solar(is3d) > NoOp。
// Ported from: itwinjs-core createTimelineProvider（Timeline.ts:115-126）。
std::unique_ptr<ITimelineProvider> createTimelineProvider(dqApp::Viewport* vp);

// TimelinePanel — play/pause + Duration + slider + message 行。
// Ported from: itwinjs-core TimelinePanel（Timeline.ts:128-323）。
class TimelinePanel final : public QObject {
    Q_OBJECT
public:
    // durationInSeconds：参考 createTimeline 调用点 Viewer.ts:384 的 10。
    TimelinePanel(dqApp::Viewport* vp, QWidget* parentWidget, double durationInSeconds);
    ~TimelinePanel() override;

    QWidget* widget() const noexcept { return m_widget; }

    // ── 测试缝（TimelinePanelTest 直驱 handler——SectionsPanel 的
    //    createSectionTool  seams 同族；start/pause 经按钮 click 驱动）──
    void updateDuration();           // :296-307
    void processSliderAdjustment();  // :287-294

private:
    void start();                    // :229-241
    void pause();                    // :243-254
    void onAnimationFrame();         // :256-271
    void update();                   // :277-285
    void onInteraction();            // :309-312
    void recreateProvider();         // onViewChanged 的 provider 重建（:218-227）
    void createProvider();           // :314-322——换 provider + elapsed 同步
    void queueAnimationFrame();      // :273-275

    dqApp::Viewport* m_vp;
    std::unique_ptr<ITimelineProvider> m_provider;  // :130 _provider
    double m_totalMillis;                           // :131 _totalMillis
    double m_elapsedMillis = 0.0;                   // :132 _elapsedMillis
    double m_lastMillis = 0.0;                      // :133 _lastMillis
    bool m_isPlaying = false;                       // :134 _isPlaying
    dqBase::DqEventScope m_eventScope;              // onViewChanged 订阅（:218-227）

    QWidget* m_widget = nullptr;
    QLabel* m_message = nullptr;      // :137 _messageElement
    QLineEdit* m_durationEdit = nullptr;  // :138 _durationElement
    QSlider* m_slider = nullptr;      // :139 _slider
    QToolButton* m_playButton = nullptr;   // :140 _playButton
    QToolButton* m_pauseButton = nullptr;  // :141 _pauseButton
    QTimer* m_timer = nullptr;        // rAF 泵的 QTimer 承载（E5）
};

}  // namespace Gui
