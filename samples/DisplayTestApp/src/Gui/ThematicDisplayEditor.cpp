// ThematicDisplayEditor — Thematic Display 编辑区实现。
// Ported from: itwinjs-core display-test-app ThematicDisplay.ts:37-777
// （ThematicDisplayEditor 全量——构造/写回环/传感器编辑/Reset/UI 回读）。
#include "ThematicDisplayEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <dqApp/Application.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/SolarCalculate.h>  // calculateSolarDirectionFromAngles（S-a 移植面——默认太阳向）

namespace Gui {
namespace {

// 参考模块级 defaultSettings（ThematicDisplay.ts:23-36——**可变共享态**：
// updateDefaultRange/enableThematicDisplay 就地改它；此处同形承载）。
dqCommon::ThematicDisplayProps& defaultSettings()
{
    static dqCommon::ThematicDisplayProps s_default = [] {
        dqCommon::ThematicDisplayProps p;
        p.displayMode = dqCommon::ThematicDisplayMode::Height;
        dqCommon::ThematicGradientSettingsProps g;
        g.mode = dqCommon::ThematicGradientMode::Smooth;
        g.marginColor = dqCommon::ColorDef::from(0xFF, 0xEB, 0xCD).getTbgr();  // ColorByName.blanchedAlmond
        g.colorScheme = dqCommon::ThematicGradientColorScheme::BlueRed;
        p.gradientSettings = g;
        p.axis = dqGeom::Vector3d::From(0.0, 0.0, 1.0);
        p.range = dqGeom::Range1d(0.0, 1.0);
        // calculateSolarDirectionFromAngles({azimuth:315, elevation:45})（S-a
        // 移植面——SolarCalculate.ts:191-197）。
        p.sunDirection = dqCommon::calculateSolarDirectionFromAngles(315.0, 45.0);
        dqCommon::ThematicDisplaySensorSettingsProps ss;
        ss.sensors = std::vector<dqCommon::ThematicDisplaySensorProps>{};
        ss.distanceCutoff = 0.0;
        p.sensorSettings = ss;
        return p;
    }();
    return s_default;
}

dqApp::Viewport* activeViewport()
{
    return dqApp::Application::Get().GetViewManager().GetActiveViewport();
}

// 渐变条目表（:126-135——Height 四项[含 IsoLines]/其余两项）。
constexpr std::pair<char const*, int> kGradientEntriesForHeight[] = {
    { "Smooth", 0 }, { "Stepped", 1 }, { "SteppedWithDelimiter", 2 }, { "IsoLines", 3 },
};
constexpr std::pair<char const*, int> kGradientEntriesForOthers[] = {
    { "Smooth", 0 }, { "Stepped", 1 },
};

// Custom 伪项键值表（:276-284——[value, r, g, b, transparency] 五行原样）。
constexpr double kCustomColorSchemes[][5][5] = {
    // All opaque
    {{0.0, 255, 255, 0, 0}, {0.5, 255, 0, 255, 0}, {1.0, 0, 255, 255, 0}},
    // All variously transparent
    {{0.0, 255, 0, 0, 0xff}, {0.25, 0, 255, 0, 0xbf}, {0.5, 0, 0, 255, 0x7f}, {0.75, 255, 0, 255, 0x3f}},
    // Variously transparent and one opaque
    {{0.0, 255, 0, 0, 0xff}, {0.2, 0, 255, 0, 0xbf}, {0.4, 0, 0, 255, 0x7f}, {0.6, 255, 0, 255, 0x3f}, {0.8, 0, 255, 255, 0}},
};

// 传感器网格值循环（:44——17 值原样）。
constexpr double kSensorValues[] = {0.1, 0.9, 0.25, 0.15, 0.8, 0.34, 0.78, 0.32,
                                    0.15, 0.29, 0.878, 0.95, 0.5, 0.278, 0.44, 0.33, 0.71};

QDoubleSpinBox* makeNumeric(QWidget* parent, QBoxLayout* layout, char const* label,
                            double value, double min, double max, double step,
                            std::function<void(double)> handler,
                            char const* objectName = nullptr)
{
    // createLabeledNumericInput 等价（label+输入同行；parseAsFloat 恒 true 域）。
    auto* row = new QWidget(parent);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->addWidget(new QLabel(QString::fromLatin1(label), row));
    auto* input = new QDoubleSpinBox(row);
    if (objectName)
        input->setObjectName(QString::fromLatin1(objectName));
    input->setRange(min, max);
    input->setSingleStep(step);
    input->setValue(value);
    rl->addWidget(input);
    layout->addWidget(row);
    QObject::connect(input, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     input, [handler](double v) { handler(v); });
    return input;
}

}  // namespace

// ---------------------------------------------------------------------------
// 构造（参考 ctor :161-679——控件序 1:1：checkbox → displayMode →
// gradientMode → stepCount+colorScheme → alpha → range High/Low → axis XYZ →
// colorMix → sunDir XYZ → distanceCutoff → sensor 选择 → sensor XYZ →
// sensorValue → Add/Delete/Grid → Reset）。
// ---------------------------------------------------------------------------
ThematicDisplayEditor::ThematicDisplayEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // 开关（:205-213 createCheckBox——handler=enableThematicDisplay）。
    m_checkbox = new QCheckBox(QStringLiteral("Thematic Display"), this);
    m_checkbox->setObjectName(QStringLiteral("cbx_Thematic"));
    layout->addWidget(m_checkbox);
    connect(m_checkbox, &QCheckBox::toggled, this,
            [this](bool on) { enableThematicDisplay(on); });

    m_controls = new QWidget(this);
    auto* cl = new QVBoxLayout(m_controls);
    cl->setContentsMargins(0, 0, 0, 0);

    // Display Mode（:230-246——四条目定序）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Display Mode: "), row));
        m_displayMode = new QComboBox(row);
        m_displayMode->setObjectName(QStringLiteral("thematic_displayMode"));
        constexpr std::pair<char const*, int> entries[] = {
            { "Height", 0 }, { "InverseDistanceWeightedSensors", 1 },
            { "Slope", 2 }, { "HillShade", 3 },
        };
        for (auto const& e : entries)
            m_displayMode->addItem(QString::fromLatin1(e.first), e.second);
        rl->addWidget(m_displayMode);
        cl->addWidget(row);
        connect(m_displayMode, &QComboBox::currentIndexChanged, this, [this](int) {
            setDisplayMode(m_displayMode->currentData().toInt());
        });
    }

    // Gradient Mode（:248-256——条目随 displayMode 重建[updateUI 面]）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Gradient Mode: "), row));
        m_gradientMode = new QComboBox(row);
        m_gradientMode->setObjectName(QStringLiteral("thematic_gradientMode"));
        for (auto const& e : kGradientEntriesForHeight)
            m_gradientMode->addItem(QString::fromLatin1(e.first), e.second);
        rl->addWidget(m_gradientMode);
        cl->addWidget(row);
        connect(m_gradientMode, &QComboBox::currentIndexChanged, this, [this](int) {
            setGradientMode(m_gradientMode->currentData().toInt());
        });
    }

    // Step Count + Color Scheme 同排（:258-323 的 spanStepAndColor）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Step Count: "), row));
        m_stepCount = new QSpinBox(row);
        m_stepCount->setObjectName(QStringLiteral("thematic_stepCount"));
        m_stepCount->setRange(2, 65536);  // :267-268 min/max
        m_stepCount->setSingleStep(1);
        m_stepCount->setValue(1);         // :262 初始值（updateUI 回读覆盖）
        rl->addWidget(m_stepCount);
        connect(m_stepCount, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this](int v) { setStepCount(v); });

        rl->addWidget(new QLabel(QStringLiteral("Color Scheme: "), row));
        m_colorScheme = new QComboBox(row);
        m_colorScheme->setObjectName(QStringLiteral("thematic_colorScheme"));
        // :293-302——5 内置 + Custom 三伪项（value=Custom+0/1/2）。
        constexpr std::pair<char const*, int> schemes[] = {
            { "BlueRed", 0 }, { "RedBlue", 1 }, { "Monochrome", 2 },
            { "Topographic", 3 }, { "SeaMountain", 4 },
            { "Custom (opaque)", 5 }, { "Custom (transparent)", 6 },
            { "Custom (mixed)", 7 },
        };
        for (auto const& s : schemes)
            m_colorScheme->addItem(QString::fromLatin1(s.first), s.second);
        rl->addWidget(m_colorScheme);
        cl->addWidget(row);
        connect(m_colorScheme, &QComboBox::currentIndexChanged, this, [this](int) {
            setColorScheme(m_colorScheme->currentData().toInt());
        });
    }

    // Multiply gradient alpha（:325-332）。
    m_alpha = new QCheckBox(QStringLiteral("Multiply gradient alpha"), m_controls);
    m_alpha->setObjectName(QStringLiteral("thematic_alpha"));
    cl->addWidget(m_alpha);
    connect(m_alpha, &QCheckBox::toggled, this,
            [this](bool on) { setMultiplyAlpha(on); });

    // Range High/Low 同排（:334-367——High 先、Low 后[参考控件序]）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        m_rangeHigh = makeNumeric(row, rl, "High range: ", -1.0, -100000.0, 100000.0, 1.0,
                                  [this](double v) { setRangeHigh(v); }, "thematic_rangeHigh");
        m_rangeLow = makeNumeric(row, rl, "Low range: ", 1.0, -100000.0, 100000.0, 1.0,
                                 [this](double v) { setRangeLow(v); }, "thematic_rangeLow");
        cl->addWidget(row);
    }

    // Axis XYZ 同排（:369-420）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto const defAxis = *defaultSettings().axis;
        m_axisX = makeNumeric(row, rl, "Axis X: ", defAxis.x, -1.0, 1.0, 0.1,
                              [this](double v) { setAxisComponent(0, v); }, "thematic_axisX");
        m_axisY = makeNumeric(row, rl, "Y: ", defAxis.y, -1.0, 1.0, 0.1,
                              [this](double v) { setAxisComponent(1, v); }, "thematic_axisY");
        m_axisZ = makeNumeric(row, rl, "Z: ", defAxis.z, -1.0, 1.0, 0.1,
                              [this](double v) { setAxisComponent(2, v); }, "thematic_axisZ");
        cl->addWidget(row);
    }

    // Terrain/PointCloud Mix 滑条（:432-441——0..1 step 0.05 + 右侧读数）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Terrain/PointCloud Mix"), row));
        m_colorMix = new QSlider(Qt::Horizontal, row);
        m_colorMix->setObjectName(QStringLiteral("thematic_colorMix"));
        m_colorMix->setRange(0, 20);  // 0..1 / 0.05
        m_colorMix->setValue(0);
        rl->addWidget(m_colorMix);
        m_colorMixReadout = new QLabel(QStringLiteral("0"), row);
        rl->addWidget(m_colorMixReadout);
        cl->addWidget(row);
        connect(m_colorMix, &QSlider::valueChanged, this, [this](int v) {
            setColorMix(v * 0.05);
        });
    }

    // Sun Direction XYZ 同排（:445-501）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        m_sunDirX = makeNumeric(row, rl, "Sun Direction X: ", 0.0, -1.0, 1.0, 0.1,
                                [this](double v) { setSunDirComponent(0, v); }, "thematic_sunDirX");
        m_sunDirY = makeNumeric(row, rl, "Y: ", 0.0, -1.0, 1.0, 0.1,
                                [this](double v) { setSunDirComponent(1, v); }, "thematic_sunDirY");
        m_sunDirZ = makeNumeric(row, rl, "Z: ", 0.0, -1.0, 1.0, 0.1,
                                [this](double v) { setSunDirComponent(2, v); }, "thematic_sunDirZ");
        cl->addWidget(row);
    }

    // Distance Cutoff（:503-515）。
    m_distanceCutoff = makeNumeric(m_controls, cl, "Distance Cutoff: ", 0.0,
                                   -999999.0, 999999.0, 0.1,
                                   [this](double v) { setDistanceCutoff(v); }, "thematic_distanceCutoff");

    // Selected Sensor 下拉（:517-526）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Selected Sensor: "), row));
        m_sensor = new QComboBox(row);
        m_sensor->setObjectName(QStringLiteral("thematic_sensor"));
        rl->addWidget(m_sensor);
        cl->addWidget(row);
        connect(m_sensor, &QComboBox::currentIndexChanged, this,
                [this](int i) { selectSensor(i); });
    }

    // Sensor XYZ 同排（:528-600）+ Value（:602-614）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        m_sensorX = makeNumeric(row, rl, "Sensor X: ", 0.0, -999999.0, 999999.0, 0.1,
                                [this](double v) { setSensorComponent(0, v); }, "thematic_sensorX");
        m_sensorY = makeNumeric(row, rl, "Y: ", 0.0, -999999.0, 999999.0, 0.1,
                                [this](double v) { setSensorComponent(1, v); }, "thematic_sensorY");
        m_sensorZ = makeNumeric(row, rl, "Z: ", 0.0, -999999.0, 999999.0, 0.1,
                                [this](double v) { setSensorComponent(2, v); }, "thematic_sensorZ");
        cl->addWidget(row);
    }
    m_sensorValue = makeNumeric(m_controls, cl, "Sensor Value: ", 0.0, 0.0, 1.0, 0.025,
                                [this](double v) { setSensorValue(v); }, "thematic_sensorValue");

    // Add/Delete/Create Grid 三钮（:619-665——居中排）。
    {
        auto* row = new QWidget(m_controls);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* add = new QPushButton(QStringLiteral("Add Sensor"), row);
        add->setObjectName(QStringLiteral("thematic_addSensor"));
        auto* del = new QPushButton(QStringLiteral("Delete Sensor"), row);
        del->setObjectName(QStringLiteral("thematic_deleteSensor"));
        auto* grid = new QPushButton(QStringLiteral("Create Sensor Grid"), row);
        grid->setObjectName(QStringLiteral("thematic_createSensorGrid"));
        rl->addWidget(add);
        rl->addWidget(del);
        rl->addWidget(grid);
        cl->addWidget(row);
        connect(add, &QPushButton::clicked, this, [this]() { addSensor(); });
        connect(del, &QPushButton::clicked, this, [this]() { deleteSensor(); });
        connect(grid, &QPushButton::clicked, this, [this]() { createSensorGrid(); });
    }

    // Reset（:667-672——居中）。
    {
        auto* reset = new QPushButton(QStringLiteral("Reset"), this);
        reset->setObjectName(QStringLiteral("thematic_reset"));
        layout->addWidget(reset);
        connect(reset, &QPushButton::clicked, this,
                [this]() { resetThematicDisplay(); });
    }

    layout->addWidget(m_controls);
    m_controls->setVisible(false);  // showHideControls(false) 初始态

    // 末节分隔线（:675-678 的 hr）。
    auto* hr = new QFrame(this);
    hr->setFrameShape(QFrame::HLine);
    hr->setStyleSheet(QStringLiteral("color: grey;"));
    layout->addWidget(hr);
}

// ---------------------------------------------------------------------------
// 写回环（:758-763 1:1）
// ---------------------------------------------------------------------------
void ThematicDisplayEditor::updateThematicDisplay(
    std::function<void(dqCommon::ThematicDisplayProps&)> mod)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto& style = vp->GetView()->GetDisplayStyle();
    // getThematicSettingsProps（:684-686——settings.thematic.toJSON()）。
    auto props = style.getThematic().toJSON();
    mod(props);
    // :762——setter 赋值（S-f 门面：equals 短路 + OnThematicChanged）。
    style.setThematic(dqCommon::ThematicDisplay::fromJSON(&props));
    // sync()（:773-776——vp.synchWithView() 无参形）。
    vp->synchWithView();
}

// ---------------------------------------------------------------------------
// 开关（:180-208——首启副作用全量）
// ---------------------------------------------------------------------------
void ThematicDisplayEditor::enableThematicDisplay(bool enabled)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto* im = vp->GetView()->GetDisplayStyle().getIModel();
    if (!im)
        return;

    {   // 直驱路径的控件态一致化（UI 路径下 toggled 已置位——阻断重入）。
        QSignalBlocker const b(m_checkbox);
        m_checkbox->setChecked(enabled);
    }
    auto const& extents = im->GetProjectExtents();
    auto& defs = defaultSettings();
    defs.range = dqGeom::Range1d(extents.low.z, extents.high.z);

    auto& ss = *defs.sensorSettings;
    if (!ss.sensors)
        ss.sensors = std::vector<dqCommon::ThematicDisplaySensorProps>{};
    // :184——distanceCutoff = extents.xLength()/25（参考原式）。
    ss.distanceCutoff = (extents.high.x - extents.low.x) / 25.0;

    // 四传感器（:190-199——XY 对角线 25%/50%/65%/75% 插值 @ 中 z，
    // 值 0.025/0.5/0.025/0.75）。
    double const sensorZ = extents.low.z + (extents.high.z - extents.low.z) / 2.0;
    auto sensorLow = extents.low;
    auto sensorHigh = extents.high;
    sensorLow.z = sensorHigh.z = sensorZ;
    auto const mkSensor = [&](double f, double v) {
        dqCommon::ThematicDisplaySensorProps s;
        s.position = dqGeom::Point3d::FromInterpolate(sensorLow, f, sensorHigh);
        s.value = v;
        return s;
    };
    auto& sensors = *ss.sensors;
    sensors.resize(4);
    sensors[0] = mkSensor(0.25, 0.025);
    sensors[1] = mkSensor(0.5, 0.5);
    sensors[2] = mkSensor(0.65, 0.025);
    sensors[3] = mkSensor(0.75, 0.75);

    resetSensorEntries(4);

    auto& style = vp->GetView()->GetDisplayStyle();
    // :202——settings.thematic = fromJSON(defaultSettings)（门面=事件+赋值）。
    style.setThematic(dqCommon::ThematicDisplay::fromJSON(&defs));
    // :203——vp.viewFlags.with("thematicDisplay", enabled)（vf 位经样式面）。
    {
        auto p = style.getViewFlags().Properties();
        p.thematicDisplay = enabled;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    m_controls->setVisible(enabled);  // showHideControls
    vp->synchWithView();
}

// ---------------------------------------------------------------------------
// Display Mode（:230-246 handler）
// ---------------------------------------------------------------------------
void ThematicDisplayEditor::setDisplayMode(int mode)
{
    updateThematicDisplay([this, mode](dqCommon::ThematicDisplayProps& props) {
        auto const prevDisplayMode = props.displayMode.value_or(dqCommon::ThematicDisplayMode::Height);
        props.displayMode = static_cast<dqCommon::ThematicDisplayMode>(mode);
        if (dqCommon::ThematicDisplayMode::Slope == props.displayMode) {
            props.range = dqGeom::Range1d(0.0, 90.0);
        } else if (dqCommon::ThematicDisplayMode::Slope == prevDisplayMode) {
            // 离 Slope → 默认域回填（updateDefaultRange :157-160——
            // defaultSettings.range 先按 extents 刷新再读）。
            auto* vp = activeViewport();
            auto const* im = vp && vp->GetView() ? vp->GetView()->GetDisplayStyle().getIModel() : nullptr;
            if (im) {
                auto const& ext = im->GetProjectExtents();
                defaultSettings().range = dqGeom::Range1d(ext.low.z, ext.high.z);
                props.range = *defaultSettings().range;
            }
        }
    });
    // 渐变条目随模式重建（:707-712 的 updateUI 面——写后立即重建，与参考
    // sync→下次 update 同效）。
    updateThematicDisplayUI();
}

void ThematicDisplayEditor::setGradientMode(int mode)
{
    updateThematicDisplay([mode](dqCommon::ThematicDisplayProps& props) {
        if (props.gradientSettings)
            props.gradientSettings->mode = static_cast<dqCommon::ThematicGradientMode>(mode);
    });
}

void ThematicDisplayEditor::setStepCount(int count)
{
    updateThematicDisplay([count](dqCommon::ThematicDisplayProps& props) {
        if (props.gradientSettings)
            props.gradientSettings->stepCount = count;
    });
}

void ThematicDisplayEditor::setColorScheme(int scheme)
{
    updateThematicDisplay([scheme](dqCommon::ThematicDisplayProps& props) {
        if (!props.gradientSettings)
            return;
        auto constexpr custom = static_cast<int>(dqCommon::ThematicGradientColorScheme::Custom);
        if (scheme < custom) {
            props.gradientSettings->colorScheme = static_cast<dqCommon::ThematicGradientColorScheme>(scheme);
        } else {
            // Custom 三伪项（:312-321——colorScheme=Custom + customKeys 查表）。
            props.gradientSettings->colorScheme = dqCommon::ThematicGradientColorScheme::Custom;
            auto const& keys = kCustomColorSchemes[scheme - custom];
            size_t const n = (scheme - custom == 1) ? 4 : ((scheme - custom == 2) ? 5 : 3);
            std::vector<dqCommon::GradientKeyColorProps> out;
            for (size_t i = 0; i < n; ++i) {
                dqCommon::GradientKeyColorProps k;
                k.value = keys[i][0];
                k.color = dqCommon::ColorDef::computeTbgrFromComponents(
                    static_cast<int>(keys[i][1]), static_cast<int>(keys[i][2]),
                    static_cast<int>(keys[i][3]), static_cast<int>(keys[i][4]));
                out.push_back(k);
            }
            props.gradientSettings->customKeys = std::move(out);
        }
    });
}

void ThematicDisplayEditor::setMultiplyAlpha(bool on)
{
    updateThematicDisplay([on](dqCommon::ThematicDisplayProps& props) {
        if (props.gradientSettings)
            props.gradientSettings->transparencyMode = on
                ? dqCommon::ThematicGradientTransparencyMode::MultiplySurfaceAndGradient
                : dqCommon::ThematicGradientTransparencyMode::SurfaceOnly;
    });
}

void ThematicDisplayEditor::setRangeHigh(double v)
{
    updateThematicDisplay([v](dqCommon::ThematicDisplayProps& props) {
        auto const old = props.range.value_or(dqGeom::Range1d::CreateNull());
        props.range = dqGeom::Range1d(old.low, v);
    });
}

void ThematicDisplayEditor::setRangeLow(double v)
{
    updateThematicDisplay([v](dqCommon::ThematicDisplayProps& props) {
        auto const old = props.range.value_or(dqGeom::Range1d::CreateNull());
        props.range = dqGeom::Range1d(v, old.high);
    });
}

void ThematicDisplayEditor::setAxisComponent(int axis, double v)
{
    updateThematicDisplay([axis, v](dqCommon::ThematicDisplayProps& props) {
        auto vec = props.axis.value_or(dqGeom::Vector3d::From(0, 0, 0));
        (axis == 0 ? vec.x : axis == 1 ? vec.y : vec.z) = v;
        props.axis = vec;
    });
}

void ThematicDisplayEditor::setColorMix(double v)
{
    m_colorMixReadout->setText(QString::number(v));
    updateThematicDisplay([v](dqCommon::ThematicDisplayProps& props) {
        if (props.gradientSettings)
            props.gradientSettings->colorMix = v;
    });
}

void ThematicDisplayEditor::setSunDirComponent(int axis, double v)
{
    updateThematicDisplay([axis, v](dqCommon::ThematicDisplayProps& props) {
        auto vec = props.sunDirection.value_or(dqGeom::Vector3d::From(0, 0, 0));
        (axis == 0 ? vec.x : axis == 1 ? vec.y : vec.z) = v;
        props.sunDirection = vec;
    });
}

void ThematicDisplayEditor::setDistanceCutoff(double v)
{
    updateThematicDisplay([v](dqCommon::ThematicDisplayProps& props) {
        // 参考的非空断言面（props.sensorSettings!）——防御性补位：toJSON 在
        // sensors 空时省略 sensorSettings 段，此径先补空壳（编辑器流内传感器
        // 恒非空[Delete 的 >1 门]，此处为零值直写防御，EQUIVALENCE 登记：
        // 参考同位置崩溃 vs DanQing 补空壳——无行为分叉的可达路径）。
        if (!props.sensorSettings)
            props.sensorSettings = dqCommon::ThematicDisplaySensorSettingsProps{};
        props.sensorSettings->distanceCutoff = v;
    });
}

// ---------------------------------------------------------------------------
// 传感器编辑（:517-614 + :619-665）
// ---------------------------------------------------------------------------
void ThematicDisplayEditor::selectSensor(int index)
{
    // 参考 handler（:517-526）仅触发写回环刷新 UI——DanQing 直读选中项刷新
    // 四输入（同效；写面零改动）。
    updateThematicDisplay([](dqCommon::ThematicDisplayProps&) {});
    if (index >= 0)
        updateThematicDisplayUI();
}

void ThematicDisplayEditor::setSensorComponent(int axis, double v)
{
    int const sel = m_sensor->currentIndex();
    if (sel < 0)
        return;
    updateThematicDisplay([axis, v, sel](dqCommon::ThematicDisplayProps& props) {
        if (!props.sensorSettings || !props.sensorSettings->sensors)
            return;
        auto& sensors = *props.sensorSettings->sensors;
        if (static_cast<size_t>(sel) >= sensors.size())
            return;
        auto pos = sensors[static_cast<size_t>(sel)].position.value_or(dqGeom::Point3d::From(0, 0, 0));
        (axis == 0 ? pos.x : axis == 1 ? pos.y : pos.z) = v;
        sensors[static_cast<size_t>(sel)].position = pos;
    });
}

void ThematicDisplayEditor::setSensorValue(double v)
{
    int const sel = m_sensor->currentIndex();
    if (sel < 0)
        return;
    updateThematicDisplay([v, sel](dqCommon::ThematicDisplayProps& props) {
        if (!props.sensorSettings || !props.sensorSettings->sensors)
            return;
        auto& sensors = *props.sensorSettings->sensors;
        if (static_cast<size_t>(sel) < sensors.size())
            sensors[static_cast<size_t>(sel)].value = v;
    });
}

void ThematicDisplayEditor::addSensor()
{
    updateThematicDisplay([this](dqCommon::ThematicDisplayProps& props) {
        if (!props.sensorSettings)
            props.sensorSettings = dqCommon::ThematicDisplaySensorSettingsProps{};
        if (!props.sensorSettings->sensors)
            props.sensorSettings->sensors = std::vector<dqCommon::ThematicDisplaySensorProps>{};
        auto& sensors = *props.sensorSettings->sensors;
        pushNewSensor(sensors, nullptr);
        resetSensorEntries(static_cast<int>(sensors.size()));
        if (!sensors.empty())
            m_sensor->setCurrentIndex(static_cast<int>(sensors.size()) - 1);
    });
}

void ThematicDisplayEditor::deleteSensor()
{
    int const sel = m_sensor->currentIndex();
    updateThematicDisplay([this, sel](dqCommon::ThematicDisplayProps& props) {
        if (!props.sensorSettings || !props.sensorSettings->sensors)
            return;
        auto& sensors = *props.sensorSettings->sensors;
        if (sensors.size() > 1 && sel >= 0 && static_cast<size_t>(sel) < sensors.size()) {
            sensors.erase(sensors.begin() + sel);  // splice(selectedIndex, 1)
            if (m_sensor->count() > sel)
                m_sensor->removeItem(sel);
        }
    });
}

void ThematicDisplayEditor::createSensorGrid()
{
    updateThematicDisplay([this](dqCommon::ThematicDisplayProps& props) {
        if (!props.sensorSettings)
            props.sensorSettings = dqCommon::ThematicDisplaySensorSettingsProps{};
        props.sensorSettings->sensors = std::vector<dqCommon::ThematicDisplaySensorProps>{};
        auto& sensors = *props.sensorSettings->sensors;
        createSensorGridImpl(sensors);
        resetSensorEntries(static_cast<int>(sensors.size()));
        if (!sensors.empty())
            m_sensor->setCurrentIndex(static_cast<int>(sensors.size()) - 1);
    });
}

// :767-772 resetThematicDisplay。
void ThematicDisplayEditor::resetThematicDisplay()
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto td = dqCommon::ThematicDisplay::fromJSON(&defaultSettings());
    vp->GetView()->GetDisplayStyle().setThematic(td);
    resetSensorEntries(static_cast<int>(td.sensorSettings.sensors.size()));
    vp->synchWithView();
    updateThematicDisplayUI();
}

// ---------------------------------------------------------------------------
// 传感器条目/网格（:38-104）
// ---------------------------------------------------------------------------
void ThematicDisplayEditor::resetSensorEntries(int count)
{
    m_sensor->clear();
    for (int i = 0; i < count; ++i)
        m_sensor->addItem(QStringLiteral("Sensor %1").arg(i));
}

void ThematicDisplayEditor::pushNewSensor(
    std::vector<dqCommon::ThematicDisplaySensorProps>& sensors,
    dqCommon::ThematicDisplaySensorProps const* sensorProps)
{
    if (sensorProps) {
        sensors.push_back(*sensorProps);
        return;
    }

    auto* vp = activeViewport();
    auto const* im = vp && vp->GetView() ? vp->GetView()->GetDisplayStyle().getIModel() : nullptr;
    if (!im)
        return;
    auto const& extents = im->GetProjectExtents();
    // 参考副作用位（:72——_pushNewSensor 内顺带 updateDefaultRange）。
    defaultSettings().range = dqGeom::Range1d(extents.low.z, extents.high.z);

    double const sensorZ = extents.low.z + (extents.high.z - extents.low.z) / 2.0;
    auto sensorLow = extents.low;
    auto sensorHigh = extents.high;
    sensorLow.z = sensorHigh.z = sensorZ;

    dqCommon::ThematicDisplaySensorProps s;
    s.position = dqGeom::Point3d::FromInterpolate(sensorLow, 0.5, sensorHigh);
    s.value = 0.5;
    sensors.push_back(s);
}

void ThematicDisplayEditor::createSensorGridImpl(
    std::vector<dqCommon::ThematicDisplaySensorProps>& sensors)
{
    auto* vp = activeViewport();
    auto const* im = vp && vp->GetView() ? vp->GetView()->GetDisplayStyle().getIModel() : nullptr;
    if (!im)
        return;
    auto const& extents = im->GetProjectExtents();
    constexpr int gridX = 32, gridY = 32;
    dqGeom::Range1d const xRange = dqGeom::Range1d::CreateXX(extents.low.x, extents.high.x);
    dqGeom::Range1d const yRange = dqGeom::Range1d::CreateXX(extents.low.y, extents.high.y);
    double const sensorZ = extents.low.z + (extents.high.z - extents.low.z) / 2.0;

    size_t vi = 0;
    for (int y = 0; y < gridY; ++y) {
        double const sy = yRange.FractionToPoint(static_cast<double>(y) / (gridY - 1));
        for (int x = 0; x < gridX; ++x) {
            double const sx = xRange.FractionToPoint(static_cast<double>(x) / (gridX - 1));
            dqCommon::ThematicDisplaySensorProps s;
            s.position = dqGeom::Point3d::From(sx, sy, sensorZ);
            s.value = kSensorValues[vi];
            sensors.push_back(s);
            if (++vi >= sizeof(kSensorValues) / sizeof(kSensorValues[0]))
                vi = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// UI 回读（:688-752）
// ---------------------------------------------------------------------------
void ThematicDisplayEditor::syncEnabledState(bool enabled)
{
    QSignalBlocker const b(m_checkbox);
    m_checkbox->setChecked(enabled);
    m_controls->setVisible(enabled);  // showHideControls
}

void ThematicDisplayEditor::updateThematicDisplayUI()
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto const& settings = vp->GetView()->GetDisplayStyle().getThematic();

    QSignalBlocker const blockers[] = {
        QSignalBlocker(m_displayMode), QSignalBlocker(m_gradientMode),
        QSignalBlocker(m_stepCount), QSignalBlocker(m_colorScheme),
        QSignalBlocker(m_alpha), QSignalBlocker(m_rangeHigh),
        QSignalBlocker(m_rangeLow), QSignalBlocker(m_axisX),
        QSignalBlocker(m_axisY), QSignalBlocker(m_axisZ),
        QSignalBlocker(m_colorMix), QSignalBlocker(m_sunDirX),
        QSignalBlocker(m_sunDirY), QSignalBlocker(m_sunDirZ),
        QSignalBlocker(m_distanceCutoff), QSignalBlocker(m_sensorX),
        QSignalBlocker(m_sensorY), QSignalBlocker(m_sensorZ),
        QSignalBlocker(m_sensorValue),
    };
    (void)blockers;

    // range null → 默认域显示（:695-698——updateDefaultRange 副作用同位）。
    auto range = settings.range;
    if (range.isNull()) {
        auto const* im = vp->GetView()->GetDisplayStyle().getIModel();
        if (im) {
            auto const& ext = im->GetProjectExtents();
            defaultSettings().range = dqGeom::Range1d(ext.low.z, ext.high.z);
            range = *defaultSettings().range;
        }
    }
    m_rangeLow->setValue(range.low);
    m_rangeHigh->setValue(range.high);

    m_displayMode->setCurrentIndex(static_cast<int>(settings.displayMode));

    // :707-712——条目随 displayMode 重建（Height 四项/其余两项）。
    {
        QSignalBlocker b(m_gradientMode);
        int const current = m_gradientMode->currentData().isValid()
            ? m_gradientMode->currentData().toInt() : 0;
        m_gradientMode->clear();
        if (dqCommon::ThematicDisplayMode::Height == settings.displayMode) {
            for (auto const& e : kGradientEntriesForHeight)
                m_gradientMode->addItem(QString::fromLatin1(e.first), e.second);
        } else {
            for (auto const& e : kGradientEntriesForOthers)
                m_gradientMode->addItem(QString::fromLatin1(e.first), e.second);
        }
        (void)current;
    }
    {
        int const gm = static_cast<int>(settings.gradientSettings.mode);
        int const idx = m_gradientMode->findData(gm);
        if (idx >= 0)
            m_gradientMode->setCurrentIndex(idx);
    }
    m_stepCount->setValue(settings.gradientSettings.stepCount);
    {
        int const cs = static_cast<int>(settings.gradientSettings.colorScheme);
        int const idx = m_colorScheme->findData(cs);
        if (idx >= 0)
            m_colorScheme->setCurrentIndex(idx);
    }
    m_colorMix->setValue(static_cast<int>(settings.gradientSettings.colorMix / 0.05));
    m_colorMixReadout->setText(QString::number(settings.gradientSettings.colorMix));

    m_axisX->setValue(settings.axis.x);
    m_axisY->setValue(settings.axis.y);
    m_axisZ->setValue(settings.axis.z);

    m_sunDirX->setValue(settings.sunDirection.x);
    m_sunDirY->setValue(settings.sunDirection.y);
    m_sunDirZ->setValue(settings.sunDirection.z);

    m_distanceCutoff->setValue(settings.sensorSettings.distanceCutoff);
    auto const& sensors = settings.sensorSettings.sensors;
    if (!sensors.empty()) {
        if (m_sensor->count() < 1)
            resetSensorEntries(static_cast<int>(sensors.size()));
        int sel = m_sensor->currentIndex();
        if (sel < 0 || static_cast<size_t>(sel) >= sensors.size())
            sel = 0;
        auto const& pos = sensors[static_cast<size_t>(sel)].position;
        m_sensorX->setValue(pos.x);
        m_sensorY->setValue(pos.y);
        m_sensorZ->setValue(pos.z);
        m_sensorValue->setValue(sensors[static_cast<size_t>(sel)].value);
    }
}

// ---------------------------------------------------------------------------
// 测试读面
// ---------------------------------------------------------------------------
int ThematicDisplayEditor::sensorCount() const
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return 0;
    return static_cast<int>(
        vp->GetView()->GetDisplayStyle().getThematic().sensorSettings.sensors.size());
}

dqCommon::ThematicDisplaySensor ThematicDisplayEditor::sensorAt(int index) const
{
    auto* vp = activeViewport();
    auto const& sensors =
        vp->GetView()->GetDisplayStyle().getThematic().sensorSettings.sensors;
    return sensors.at(static_cast<size_t>(index));
}

int ThematicDisplayEditor::selectedSensor() const
{
    return m_sensor->currentIndex();
}

}  // namespace Gui
