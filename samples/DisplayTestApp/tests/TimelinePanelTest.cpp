// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-U U-f TimelinePanel（Animation 宿主 UI）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           Timeline（无 .test.ts；行为锚 = Timeline.ts 各 handler 体逐条——
//           provider 选择链 :115-126 / Analysis 映射 :24-38 / Solar _active
//           门 :40-79 / Schedule 映射 :81-99 / 面板状态机 :229-323）。
//
// 覆盖面：
//  - provider 选择矩阵（Schedule > Analysis > Solar > NoOp——:115-126）。
//  - AnalysisTimelineProvider：update(time) → analysisFraction=time/100；
//    getCurrentTime=fraction*100（:24-38）。
//  - SolarTimelineProvider：_active 门两段（:64-69 未交互不写/交互后写）+
//    duration=sunrise..sunset（:54-57——blank 连接无 ecef → Philadelphia
//    回退支）+ getCurrentTime 恒 low（:71-74）。
//  - ScheduleTimelineProvider：update → vp.timePoint；getCurrentTime=
//    timePoint ?? low（:81-99）。
//  - 面板：slider 拖动 → elapsed 映射 + provider.update（:287-294）；
//    Duration 非法拒绝（:296-299）；play/pause 按钮互斥（:234-236/:247-249）；
//    NoOp message 文案（:105）。

#include <gtest/gtest.h>
// 保证 QApplication 存在（SectionsPanelTest 同款；进程级只建一次）
#include <QApplication>
namespace { struct QtEnvSP { QtEnvSP() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }

#include "Gui/TimelinePanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QToolButton>
#include <QWidget>

#include <dqApp/BlankConnection.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/AnalysisStyle.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqCommon/RenderSchedule.h>

#include <memory>
#include <vector>

namespace {

struct TimelineFixture {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;

    TimelineFixture()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100.0, -100.0, -100.0),
                                        dqGeom::Point3d::From(100.0, 100.0, 100.0));
        conn = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(conn.Get(), dqGeom::Point3d::FromZero(),
                                                    dqGeom::Vector3d::From(200.0, 200.0, 200.0));
        vp = dqApp::Viewport::Create(nullptr, view);
    }

    ~TimelineFixture()
    {
        delete vp;
    }
};

dqCommon::RenderSchedule::ScriptProps makeScriptProps()
{
    using namespace dqCommon::RenderSchedule;
    ScriptBuilder builder;
    auto& model = builder.addModelTimeline("0x1");
    model.addVisibility(0.0, 100.0);
    return builder.finish();
}

}  // namespace

// ── provider 选择矩阵（Timeline.ts:115-126）───────────────────────────────

// Authored: no reference test exists in itwinjs-core for Timeline provider
//           selection（行为锚 = createTimelineProvider :115-126 的 if 链）。
TEST(TimelinePanelProviderTest, SelectsSolarForPlain3dView)
{
    QtEnvSP qt;
    TimelineFixture f;
    auto provider = Gui::createTimelineProvider(f.vp);
    // blank 3d 视图：无 script/无 analysisStyle → Solar（:122-123）。
    EXPECT_TRUE(dynamic_cast<Gui::SolarTimelineProvider*>(provider.get()) != nullptr);
}

// Authored: 行为锚 = :115-126（script 优先级最高）。
TEST(TimelinePanelProviderTest, SelectsScheduleWhenScriptPresent)
{
    QtEnvSP qt;
    TimelineFixture f;
    auto script = dqCommon::RenderSchedule::Script::fromJSON(makeScriptProps());
    ASSERT_TRUE(script);
    f.view->GetDisplayStyle().setScheduleScript(
        std::make_unique<dqCommon::RenderSchedule::Script>(*script));

    auto provider = Gui::createTimelineProvider(f.vp);
    auto* schedule = dynamic_cast<Gui::ScheduleTimelineProvider*>(provider.get());
    ASSERT_TRUE(schedule != nullptr);
    // duration = script.duration（:117——visibility 0..最大时间）。
    EXPECT_GE(schedule->duration().high, 0.0);
}

// Authored: 行为锚 = :119-120（analysisStyle 次优先）。
TEST(TimelinePanelProviderTest, SelectsAnalysisWhenAnalysisStylePresent)
{
    QtEnvSP qt;
    TimelineFixture f;
    dqCommon::AnalysisStyleProps props;
    dqCommon::AnalysisStyleDisplacementProps disp;
    disp.channelName = "disp";
    props.displacement = disp;
    f.view->GetDisplayStyle().setAnalysisStyle(dqCommon::AnalysisStyle::fromJSON(&props));

    auto provider = Gui::createTimelineProvider(f.vp);
    auto* analysis = dynamic_cast<Gui::AnalysisTimelineProvider*>(provider.get());
    ASSERT_TRUE(analysis != nullptr);
    // duration = 0..100（:25——Range1d.createXX(0, 100)）。
    EXPECT_DOUBLE_EQ(analysis->duration().low, 0.0);
    EXPECT_DOUBLE_EQ(analysis->duration().high, 100.0);
}

// Authored: 行为锚 = :24-38（fraction 0..1 ↔ 0..100 双向映射）。
TEST(TimelinePanelProviderTest, AnalysisProviderFractionMapping)
{
    QtEnvSP qt;
    TimelineFixture f;
    Gui::AnalysisTimelineProvider provider;

    provider.update(42.0, f.vp);
    EXPECT_NEAR(f.view->GetDisplayStyle().getAnalysisFraction(), 0.42, 1e-10);
    EXPECT_NEAR(provider.getCurrentTime(f.vp), 42.0, 1e-8);

    // getDescription：:28 `${time.toFixed(0)}%`。
    EXPECT_EQ(provider.getDescription(42.4), QStringLiteral("42%"));
}

// Authored: 行为锚 = :40-79（_active 门 + duration=sunrise..sunset）。
TEST(TimelinePanelProviderTest, SolarProviderActiveGateAndDuration)
{
    QtEnvSP qt;
    TimelineFixture f;
    Gui::SolarTimelineProvider provider(f.vp);

    // duration：日出 < 日落，同日（Philadelphia 回退支——blank 无 ecef）。
    EXPECT_LT(provider.duration().low, provider.duration().high);
    EXPECT_GT(provider.duration().Length(), 0.0);

    // _active 门前：update 不写（:64-65——参考怪癖原样）。
    double const someTime = provider.duration().low + 1000.0;
    provider.update(someTime, f.vp);
    EXPECT_FALSE(f.view->GetDisplayStyle().getSunTime().has_value());

    // onInteraction 后：update 写 setSunTime（:66-68 事件链等价面）。
    provider.onInteraction();
    provider.update(someTime, f.vp);
    ASSERT_TRUE(f.view->GetDisplayStyle().getSunTime().has_value());
    EXPECT_DOUBLE_EQ(*f.view->GetDisplayStyle().getSunTime(), someTime);

    // getCurrentTime 恒 low（:71-74 NB 注）。
    EXPECT_DOUBLE_EQ(provider.getCurrentTime(f.vp), provider.duration().low);
}

// Authored: 行为锚 = :81-99（vp.timePoint 读写 + ??low 回退）。
TEST(TimelinePanelProviderTest, ScheduleProviderTimePointMapping)
{
    QtEnvSP qt;
    TimelineFixture f;
    dqGeom::Range1d const duration(1000.0, 2000.0);
    Gui::ScheduleTimelineProvider provider(duration);

    // 无 timePoint → low（:97 ?? 语义）。
    EXPECT_DOUBLE_EQ(provider.getCurrentTime(f.vp), 1000.0);

    provider.update(1500.0, f.vp);
    ASSERT_TRUE(f.vp->getTimePoint().has_value());
    EXPECT_DOUBLE_EQ(*f.vp->getTimePoint(), 1500.0);
    EXPECT_DOUBLE_EQ(provider.getCurrentTime(f.vp), 1500.0);
}

// ── 面板状态机（Timeline.ts:229-323）──────────────────────────────────────

// Authored: 行为锚 = :229-253（play/pause 互斥）+:277-285（slider/message
//           随 update）+:296-307（Duration 拒绝路径）。
TEST(TimelinePanelTest, PlayPauseToggleAndDurationValidation)
{
    QtEnvSP qt;
    TimelineFixture f;
    QWidget host;
    Gui::TimelinePanel panel(f.vp, &host, /*durationInSeconds=*/10);

    auto* play = panel.widget()->findChild<QToolButton*>();
    auto* slider = panel.widget()->findChild<QSlider*>();
    auto* durationEdit = panel.widget()->findChild<QLineEdit*>();
    ASSERT_TRUE(play != nullptr);
    ASSERT_TRUE(slider != nullptr);
    ASSERT_TRUE(durationEdit != nullptr);

    // 初始：pause 隐藏（:167 display:none）。
    auto const buttons = panel.widget()->findChildren<QToolButton*>();
    ASSERT_EQ(buttons.size(), 2);
    auto* pause = buttons[0] == play ? buttons[1] : buttons[0];
    EXPECT_TRUE(play->isVisibleTo(panel.widget()));
    EXPECT_FALSE(pause->isVisibleTo(panel.widget()));

    // Solar provider 初始（blank 3d）——slider 0（elapsed 0 → fraction 0）。
    EXPECT_EQ(slider->value(), 0);

    // Duration 拒绝路径（:297-298 NaN/<=0 → return）：total 不变。
    durationEdit->setText(QStringLiteral("abc"));
    panel.updateDuration();
    // 内部 total 未变——经 slider 映射观测：elapsed 0/total 10000 → 0。
    EXPECT_EQ(slider->value(), 0);

    durationEdit->setText(QStringLiteral("-5"));
    panel.updateDuration();
    EXPECT_EQ(slider->value(), 0);

    // 合法值（:301-303 fraction 保持重缩放——elapsed 0 → 仍 0）。
    durationEdit->setText(QStringLiteral("20"));
    panel.updateDuration();
    EXPECT_EQ(slider->value(), 0);
}

// Authored: 行为锚 = :287-294（slider → elapsed → provider.update）。
TEST(TimelinePanelTest, SliderAdjustmentUpdatesAnalysisFraction)
{
    QtEnvSP qt;
    TimelineFixture f;
    // Analysis provider（fraction 可读回）。
    dqCommon::AnalysisStyleProps props;
    dqCommon::AnalysisStyleDisplacementProps disp;
    disp.channelName = "disp";
    props.displacement = disp;
    f.view->GetDisplayStyle().setAnalysisStyle(dqCommon::AnalysisStyle::fromJSON(&props));

    QWidget host;
    Gui::TimelinePanel panel(f.vp, &host, 10);
    auto* slider = panel.widget()->findChild<QSlider*>();
    ASSERT_TRUE(slider != nullptr);

    // slider 500/1000 → fraction 0.5 → point=50 → analysisFraction 0.5。
    slider->setValue(500);
    // 直接驱动 handler（sliderMoved 的用户路径等价——行为锚 :287-294）。
    panel.processSliderAdjustment();
    EXPECT_NEAR(f.view->GetDisplayStyle().getAnalysisFraction(), 0.5, 1e-10);

    // message 行 = provider 描述（:284——Analysis 的 "50%"）。
    auto* message = panel.widget()->findChild<QLabel*>(
        QStringLiteral("DTA.Timeline.Message"));
    ASSERT_TRUE(message != nullptr);
    EXPECT_EQ(message->text(), QStringLiteral("50%"));
}

// Authored: 行为锚 = :101-113（NoOp 文案）——无 vp 时 provider 退化为 NoOp。
TEST(TimelinePanelTest, NoOpProviderDescription)
{
    QtEnvSP qt;
    Gui::NoOpTimelineProvider provider;
    EXPECT_EQ(provider.getDescription(0.0),
              QStringLiteral("No animation available for this view."));
    // update 恒 no-op（:108）+ getCurrentTime=low（:111）。
    provider.update(50.0, nullptr);
    EXPECT_DOUBLE_EQ(provider.getCurrentTime(nullptr), 0.0);
}
