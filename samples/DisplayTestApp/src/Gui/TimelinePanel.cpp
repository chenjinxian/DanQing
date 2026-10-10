// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Animation / solar time 弹出面板实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/Timeline.ts
//              （行号锚见各段注——handler 体 1:1）。
#include "TimelinePanel.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <dqApp/DisplayStyle.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/Cartographic.h>
#include <dqCommon/RenderSchedule.h>
#include <dqCommon/SolarCalculate.h>

#include "DtaToolBars.h"  // dtaGlyphIcon

namespace Gui {

// ---------------------------------------------------------------------------
// ScheduleTimelineProvider（Timeline.ts:81-99）
// ---------------------------------------------------------------------------
QString ScheduleTimelineProvider::getDescription(double time) const
{
    // :89 `${new Date(time * 1000).toString()}`——秒 epoch → ms。
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(time * 1000.0)).toString();
}

void ScheduleTimelineProvider::update(double time, dqApp::Viewport* vp)
{
    // :93 vp.timePoint = time（Viewport.timePoint 直通——Viewport.ts:579-584）。
    if (vp)
        vp->setTimePoint(time);
}

double ScheduleTimelineProvider::getCurrentTime(dqApp::Viewport* vp) const
{
    // :97 vp.timePoint ?? this.duration.low。
    if (vp) {
        if (auto const tp = vp->getTimePoint(); tp.has_value())
            return *tp;
    }
    return m_duration.low;
}

// ---------------------------------------------------------------------------
// AnalysisTimelineProvider（Timeline.ts:24-38）
// ---------------------------------------------------------------------------
QString AnalysisTimelineProvider::getDescription(double time) const
{
    // :28 `${time.toFixed(0)}%`。
    return QString::number(static_cast<int>(time)) + QStringLiteral("%");
}

void AnalysisTimelineProvider::update(double time, dqApp::Viewport* vp)
{
    // :32 vp.displayStyle.settings.analysisFraction = time / 100。
    if (vp && vp->GetView())
        vp->GetView()->GetDisplayStyle().setAnalysisFraction(time / 100.0);
}

double AnalysisTimelineProvider::getCurrentTime(dqApp::Viewport* vp) const
{
    // :36 vp.displayStyle.settings.analysisFraction * 100。
    if (vp && vp->GetView())
        return vp->GetView()->GetDisplayStyle().getAnalysisFraction() * 100.0;
    return m_duration.low;
}

// ---------------------------------------------------------------------------
// SolarTimelineProvider（Timeline.ts:40-79）
// ---------------------------------------------------------------------------
SolarTimelineProvider::SolarTimelineProvider(dqApp::Viewport* vp)
{
    // :45-52 cartoCenter——isGeoLocated 分支 + Philadelphia 回退（文件头
    // EQUIVALENCE 注；DanQing 的 HasEcefLocation=参考 ecefLocation 在场性）。
    dqCommon::Cartographic carto =
        dqCommon::Cartographic::fromDegrees(-75.17035, 39.954927, 0.0);
    if (vp) {
        if (auto* imodel = vp->GetIModel(); imodel && imodel->HasEcefLocation()) {
            if (auto const c = dqCommon::Cartographic::fromEcef(imodel->GetEcefLocation().origin))
                carto = *c;
        }
    }

    // :54-57 const today = new Date(Date.now())——当日（本地历元 ms）。
    double const todayMs = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    double const sunrise = dqCommon::calculateSunriseOrSunset(todayMs, carto, true);
    double const sunset = dqCommon::calculateSunriseOrSunset(todayMs, carto, false);
    // :57 Range1d.createXX（低高序——CreateXX 同语义）。
    m_duration = dqGeom::Range1d::CreateXX(sunrise, sunset);
}

QString SolarTimelineProvider::getDescription(double time) const
{
    // :61 new Date(time).toTimeString()——ms epoch；本地化 hh:mm:ss 短形
    // （文件头格式等价登记）。
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(time)).time().toString(
        QStringLiteral("hh:mm:ss"));
}

void SolarTimelineProvider::update(double time, dqApp::Viewport* vp)
{
    // :64-68 if (this._active && vp.displayStyle.is3d()) { setSunTime; invalidate }
    // ——_active 门=参考怪癖原样（首次交互前拖 slider/播放不写）；setSunTime
    // 的事件链（OnLightSettingsChanged→Viewport 监听→InvalidateRenderPlan）
    // 承载参考的显式 invalidate（文件头事件链等价登记）。
    if (!m_active || !vp || !vp->GetView())
        return;
    vp->GetView()->GetDisplayStyle().setSunTime(time);
}

double SolarTimelineProvider::getCurrentTime(dqApp::Viewport* /*vp*/) const
{
    // :71-74 NB: All we have is the solar direction - cannot compute date from
    // that. 恒 duration.low。
    return m_duration.low;
}

void SolarTimelineProvider::onInteraction()
{
    // :76-78 this._active = true。
    m_active = true;
}

// ---------------------------------------------------------------------------
// NoOpTimelineProvider（Timeline.ts:101-113）
// ---------------------------------------------------------------------------
QString NoOpTimelineProvider::getDescription(double /*time*/) const
{
    // :105 "No animation available for this view."
    return QStringLiteral("No animation available for this view.");
}

double NoOpTimelineProvider::getCurrentTime(dqApp::Viewport* /*vp*/) const
{
    // :111 this.duration.low。
    return m_duration.low;
}

// ---------------------------------------------------------------------------
// createTimelineProvider（Timeline.ts:115-126）
// ---------------------------------------------------------------------------
std::unique_ptr<ITimelineProvider> createTimelineProvider(dqApp::Viewport* vp)
{
    // :116-117 Schedule：vp.displayStyle.scheduleScript 在场。
    if (vp && vp->GetView()) {
        auto& style = vp->GetView()->GetDisplayStyle();
        if (auto const* script = style.getScheduleScript())
            return std::make_unique<ScheduleTimelineProvider>(script->duration);

        // :119-120 Analysis：settings.analysisStyle 在场。
        if (style.getAnalysisStyle().has_value())
            return std::make_unique<AnalysisTimelineProvider>();

        // :122-123 Solar：displayStyle.is3d()——DanQing 的 DisplayStyle 恒持
        // 3d 设置载体（is3d() 恒 true——同 D-5 登记族）。
        if (style.getSettings().is3d())
            return std::make_unique<SolarTimelineProvider>(vp);
    }

    // :125 NoOp。
    return std::make_unique<NoOpTimelineProvider>();
}

// ---------------------------------------------------------------------------
// TimelinePanel（Timeline.ts:128-323）
// ---------------------------------------------------------------------------
TimelinePanel::TimelinePanel(dqApp::Viewport* vp, QWidget* parentWidget, double durationInSeconds)
    : QObject(parentWidget)
    , m_vp(vp)
    // :146 this._totalMillis = 1000 * durationInSeconds;
    , m_totalMillis(1000.0 * durationInSeconds)
{
    // :147 this._provider = this.createProvider();
    createProvider();

    // 参考的 DOM 构建（:149-201）→ QWidget 承载。
    m_widget = new QWidget(parentWidget);
    m_widget->setWindowTitle(QStringLiteral("Animation"));
    auto* root = new QVBoxLayout(m_widget);
    root->setContentsMargins(6, 6, 6, 6);

    // controls 行（:155-187）：play + pause + Duration 输入。
    auto* controls = new QHBoxLayout();

    // :156-159 createToolButton({ iconUnicode: "", click: start })
    m_playButton = new QToolButton(m_widget);
    m_playButton->setIcon(dtaGlyphIcon(0xea32));
    m_playButton->setToolTip(QStringLiteral("Start"));
    connect(m_playButton, &QToolButton::clicked, this, [this] { start(); });
    controls->addWidget(m_playButton);

    // :160-163 createToolButton({ iconUnicode: "", click: pause })
    m_pauseButton = new QToolButton(m_widget);
    m_pauseButton->setIcon(dtaGlyphIcon(0xea33));
    m_pauseButton->setToolTip(QStringLiteral("Pause"));
    connect(m_pauseButton, &QToolButton::clicked, this, [this] { pause(); });
    controls->addWidget(m_pauseButton);

    // :167 this._pauseButton.style.display = "none"——暂停钮初始隐藏。
    m_pauseButton->setVisible(false);

    // :169-185 Duration 行（label + 数字输入——onchange 语义见 updateDuration）。
    controls->addWidget(new QLabel(QStringLiteral("Duration: "), m_widget));
    m_durationEdit = new QLineEdit(m_widget);
    m_durationEdit->setObjectName(QStringLiteral("anim_duration"));  // :176 id
    m_durationEdit->setFixedWidth(4 * 12);                           // :181 width 4em
    // :180 value = durationInSeconds.toString()
    m_durationEdit->setText(QString::number(durationInSeconds));
    // :183 onchange → updateDuration()
    connect(m_durationEdit, &QLineEdit::editingFinished, this, [this] { updateDuration(); });
    controls->addWidget(m_durationEdit);
    // :182 " second(s)"
    controls->addWidget(new QLabel(QStringLiteral(" second(s)"), m_widget));
    controls->addStretch(1);
    root->addLayout(controls);

    // :189-196 slider（range 0..1000，value 0；input → processSliderAdjustment）。
    m_slider = new QSlider(Qt::Horizontal, m_widget);
    m_slider->setRange(0, 1000);  // :192-193 min=0 max=1000
    m_slider->setValue(0);        // :194 value=0
    // :195 addEventListener("input", ...)——sliderMoved=用户拖动语义（程序
    // setValue 不触发，同参考直赋 value 不回调）。
    connect(m_slider, &QSlider::sliderMoved, this, [this](int) { processSliderAdjustment(); });
    root->addWidget(m_slider);

    // :198-199 message 行。
    m_message = new QLabel(m_widget);
    m_message->setObjectName(QStringLiteral("DTA.Timeline.Message"));  // 测试缝
    root->addWidget(m_message);

    // rAF 泵的 QTimer 承载（单发链=参考的逐帧 queueAnimationFrame 语义——
    // 暂停路径不补链，泵自然死亡，同参考早退不 queue）。
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(15);  // E5：rAF ≈ vSync → 15ms 近似
    connect(m_timer, &QTimer::timeout, this, [this] { onAnimationFrame(); });

    // :218-227 onViewChanged——参考 ToolBarDropDown 的视图变更契约；DanQing
    // 经 Viewport.onViewChanged 事件（工具栏下拉宿主等价面）。
    if (m_vp) {
        m_eventScope.add(m_vp->onViewChanged.AddListener([this](dqApp::Viewport*) {
            recreateProvider();
        }));
    }

    // :203 this.update();
    update();
}

TimelinePanel::~TimelinePanel() = default;

void TimelinePanel::start()
{
    // :230-231 if (this._isPlaying) return;
    if (m_isPlaying)
        return;

    // :233 this.onInteraction();
    onInteraction();
    // :234-236 状态机 + 按钮互斥。
    m_isPlaying = true;
    m_pauseButton->setVisible(true);
    m_playButton->setVisible(false);

    // :238-240 this.update(); this._lastMillis = Date.now();
    //         this.queueAnimationFrame();
    update();
    m_lastMillis = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    queueAnimationFrame();
}

void TimelinePanel::pause()
{
    // :244-245 if (!this._isPlaying) return;
    if (!m_isPlaying)
        return;

    // :247-249 状态机 + 按钮互斥。
    m_isPlaying = false;
    m_pauseButton->setVisible(false);
    m_playButton->setVisible(true);

    // :251-253 this._lastMillis = Date.now(); this.update();
    //         this.queueAnimationFrame();
    m_lastMillis = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    update();
    queueAnimationFrame();
}

void TimelinePanel::onAnimationFrame()
{
    // :257-258 if (!this._isPlaying) return;（早退——不补链，泵死亡）。
    if (!m_isPlaying)
        return;

    // :260-263 elapsed 累计。
    double const now = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    double const elapsed = now - m_lastMillis;
    m_lastMillis = now;
    m_elapsedMillis += elapsed;

    // :265 this.update();
    update();

    // :267-268 if (this._elapsedMillis >= this._totalMillis)
    //             this._elapsedMillis = 0;
    if (m_elapsedMillis >= m_totalMillis)
        m_elapsedMillis = 0.0;

    // :270 this.queueAnimationFrame();
    queueAnimationFrame();
}

void TimelinePanel::update()
{
    // :278-279 fraction = Math.min(1, this._elapsedMillis / this._totalMillis)
    //   this._slider.value = (fraction * 1000).toString();
    double const fraction = std::min(1.0, m_elapsedMillis / m_totalMillis);
    m_slider->setValue(static_cast<int>(fraction * 1000.0));

    // :281-282 const point = this._provider.duration.fractionToPoint(fraction);
    //           this._provider.update(point, this._vp);
    double const point = m_provider->duration().FractionToPoint(fraction);
    m_provider->update(point, m_vp);

    // :284 this._messageElement.innerText = this._provider.getDescription(point);
    m_message->setText(m_provider->getDescription(point));
}

void TimelinePanel::processSliderAdjustment()
{
    // :288 this.onInteraction();
    onInteraction();
    // :289-290 const sliderValue = parseInt(this._slider.value, 10);
    //           const fraction = sliderValue / 1000;
    double const fraction = static_cast<double>(m_slider->value()) / 1000.0;
    // :291 this._elapsedMillis = fraction * this._totalMillis;
    m_elapsedMillis = fraction * m_totalMillis;
    // :292 this._lastMillis = Date.now();
    m_lastMillis = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    // :293 this.update();
    update();
}

void TimelinePanel::updateDuration()
{
    // :297-298 const seconds = parseInt(this._durationElement.value, 10);
    //           if (Number.isNaN(seconds) || seconds <= 0) return;
    // parseInt 语义：前导整数解析（"3.5" → 3——参考怪癖原样）。
    bool ok = false;
    QString const text = m_durationEdit->text().trimmed();
    int const seconds = text.toInt(&ok, 10);
    if (!ok || seconds <= 0) {
        // 拒绝路径：total/elapsed 不动（参考 return 语义）——文本回滚为现值。
        m_durationEdit->setText(QString::number(m_totalMillis / 1000.0));
        return;
    }

    // :301-303 fraction 保持；total/elapsed 重缩放。
    double const fraction = std::min(1.0, m_elapsedMillis / m_totalMillis);
    m_totalMillis = seconds * 1000.0;
    m_elapsedMillis = fraction * m_totalMillis;
    m_lastMillis = static_cast<double>(QDateTime::currentMSecsSinceEpoch());

    // :305 this.update();
    update();
}

void TimelinePanel::onInteraction()
{
    // :310-311 if (this._provider.onInteraction) this._provider.onInteraction();
    m_provider->onInteraction();
}

void TimelinePanel::createProvider()
{
    // :315-322 createProvider——换 provider + elapsed 与当前时间同步：
    //   const provider = createTimelineProvider(this._vp);
    //   const time = provider.getCurrentTime(this._vp);
    //   const timeFraction = (time - provider.duration.low) / provider.duration.length();
    //   this._elapsedMillis = timeFraction * this._totalMillis;
    //   return provider;
    //（:318 length 的 0/负防御：参考直除——DanQing 0 守卫登记[null 区间
    // 在现实 provider 不出现，分母 0 → 按 0 fraction 载 elapsed]。）
    m_provider = createTimelineProvider(m_vp);

    double const time = m_provider->getCurrentTime(m_vp);
    double const length = m_provider->duration().Length();
    double const timeFraction = length > 0.0 ? (time - m_provider->duration().low) / length
                                             : 0.0;
    m_elapsedMillis = timeFraction * m_totalMillis;
}

void TimelinePanel::recreateProvider()
{
    // :219-224 onViewChanged——先换 provider（elapsed 同步内联），再播停分支。
    createProvider();

    if (m_isPlaying)
        pause();
    else
        update();
}

void TimelinePanel::queueAnimationFrame()
{
    // :273-275 requestAnimationFrame(() => this.onAnimationFrame())——单发链承载。
    m_timer->start();
}

}  // namespace Gui
