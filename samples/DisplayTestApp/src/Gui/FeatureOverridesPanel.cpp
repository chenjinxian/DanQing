// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — feature symbology override panel implementation
// Ported from: itwinjs-core display-test-app FeatureOverrides.ts
#include "FeatureOverridesPanel.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <dqApp/IModelConnection.h>
#include <dqApp/Viewport.h>

#include <set>

namespace Gui {
namespace {

// -fno-rtti 下 instanceof 判定（参考 :79-80 `x instanceof Provider`）的
// 等价物：活跃实例注册表成员判定（谓词在 FindFeatureOverrideProvider 内
// 对基类指针调用——成员命中即 static_cast 安全）。
std::set<FeatureOverridesProvider*>& liveProviders()
{
    static std::set<FeatureOverridesProvider*> s;
    return s;
}

// FeatureAppearanceProps ↔ JSON（参考 toJSON 的 JSON.stringify(value.toJSON())
// / fromJSON(JSON.parse(eo.fsa)) 的宿主序列化半边）。
QJsonObject rgbToJson(dqCommon::RgbColorProps const& rgb)
{
    return QJsonObject{{"r", rgb.r}, {"g", rgb.g}, {"b", rgb.b}};
}
dqCommon::RgbColorProps rgbFromJson(QJsonObject const& o)
{
    dqCommon::RgbColorProps rgb;
    if (o.contains("r")) rgb.r = o.value("r").toInt();
    if (o.contains("g")) rgb.g = o.value("g").toInt();
    if (o.contains("b")) rgb.b = o.value("b").toInt();
    return rgb;
}
QJsonObject appearancePropsToJson(dqCommon::FeatureAppearanceProps const& p)
{
    QJsonObject o;
    if (p.rgb.has_value())
        o.insert("rgb", rgbToJson(*p.rgb));
    if (p.lineRgbIsFalse)
        o.insert("lineRgb", false);
    else if (p.lineRgb.has_value())
        o.insert("lineRgb", rgbToJson(*p.lineRgb));
    if (p.weight.has_value())
        o.insert("weight", *p.weight);
    if (p.transparency.has_value())
        o.insert("transparency", *p.transparency);
    if (p.lineTransparencyIsFalse)
        o.insert("lineTransparency", false);
    else if (p.lineTransparency.has_value())
        o.insert("lineTransparency", *p.lineTransparency);
    if (p.viewDependentTransparency)
        o.insert("viewDependentTransparency", true);
    if (p.linePixels.has_value())
        o.insert("linePixels", static_cast<double>(*p.linePixels));
    if (p.ignoresMaterial)
        o.insert("ignoresMaterial", true);
    if (p.nonLocatable)
        o.insert("nonLocatable", true);
    if (p.emphasized)
        o.insert("emphasized", true);
    return o;
}
dqCommon::FeatureAppearanceProps appearancePropsFromJson(QJsonObject const& o)
{
    dqCommon::FeatureAppearanceProps p;
    if (o.value("rgb").isObject())
        p.rgb = rgbFromJson(o.value("rgb").toObject());
    if (o.value("lineRgb").isObject())
        p.lineRgb = rgbFromJson(o.value("lineRgb").toObject());
    else if (o.value("lineRgb").isBool() && !o.value("lineRgb").toBool())
        p.lineRgbIsFalse = true;
    if (o.value("weight").isDouble())
        p.weight = o.value("weight").toDouble();
    if (o.value("transparency").isDouble())
        p.transparency = o.value("transparency").toDouble();
    if (o.value("lineTransparency").isObject())
        p.lineTransparency = o.value("lineTransparency").toObject().value("v").toDouble();
    else if (o.value("lineTransparency").isDouble())
        p.lineTransparency = o.value("lineTransparency").toDouble();
    else if (o.value("lineTransparency").isBool() && !o.value("lineTransparency").toBool())
        p.lineTransparencyIsFalse = true;
    if (o.value("viewDependentTransparency").toBool())
        p.viewDependentTransparency = true;
    if (o.value("linePixels").isDouble())
        p.linePixels = static_cast<dqCommon::LinePixels>(
            static_cast<uint32_t>(o.value("linePixels").toDouble()));
    if (o.value("ignoresMaterial").toBool())
        p.ignoresMaterial = true;
    if (o.value("nonLocatable").toBool())
        p.nonLocatable = true;
    if (o.value("emphasized").toBool())
        p.emphasized = true;
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------
// FeatureOverridesProvider
// ---------------------------------------------------------------------------

FeatureOverridesProvider::FeatureOverridesProvider(dqApp::Viewport* vp)
    : m_vp(vp)
{
    liveProviders().insert(this);
}

FeatureOverridesProvider::~FeatureOverridesProvider()
{
    liveProviders().erase(this);
}

// Ported from: FeatureOverrides.ts addFeatureOverrides (:21-25).
void FeatureOverridesProvider::addFeatureOverrides(
    dqCommon::FeatureOverrides& ovrs, void* /*context*/)
{
    for (auto const& [id, appearance] : m_elementOvrs)
        ovrs.overrideElement(dqBase::DqId(id), appearance);
    if (m_defaultOvrs.has_value())
        ovrs.setDefaultOverrides(*m_defaultOvrs);
}

// Ported from: FeatureOverrides.ts overrideElements (:27-32——选择集逐元素)。
void FeatureOverridesProvider::overrideElements(
    dqCommon::FeatureAppearance const& app)
{
    if (m_vp && m_vp->GetIModel())
        for (uint32_t id : m_vp->GetIModel()->GetSelectionSet().GetElements())
            m_elementOvrs[id] = app;
    sync();
}

// Ported from: FeatureOverrides.ts toJSON (:46-63——空表 + 无默认 → nullopt；
// 默认 override 尾插 "-default-" 条目)。
std::optional<std::vector<FeatureOverridesProvider::ElementOverride>>
FeatureOverridesProvider::toJSON() const
{
    if (m_elementOvrs.empty() && !m_defaultOvrs.has_value())
        return std::nullopt;

    std::vector<ElementOverride> out;
    for (auto const& [id, appearance] : m_elementOvrs) {
        ElementOverride eo;
        eo.id = std::to_string(id);
        eo.fsaJson = QString::fromUtf8(QJsonDocument(
                                           appearancePropsToJson(appearance.toJSON()))
                                           .toJson(QJsonDocument::Compact))
                         .toStdString();
        out.push_back(std::move(eo));
    }
    if (m_defaultOvrs.has_value()) {
        ElementOverride eo;
        eo.id = "-default-";
        eo.fsaJson = QString::fromUtf8(QJsonDocument(
                                           appearancePropsToJson(m_defaultOvrs->toJSON()))
                                           .toJson(QJsonDocument::Compact))
                         .toStdString();
        out.push_back(std::move(eo));
    }
    return out;
}

// Ported from: FeatureOverrides.ts overrideElementsByArray (:34-44——SavedView
// recall 的恢复面)。
void FeatureOverridesProvider::overrideElementsByArray(
    std::vector<ElementOverride> const& elementOvrs)
{
    for (auto const& eo : elementOvrs) {
        auto const props = appearancePropsFromJson(
            QJsonDocument::fromJson(QByteArray::fromStdString(eo.fsaJson)).object());
        auto const fsa = dqCommon::FeatureAppearance::fromJSON(&props);
        if (eo.id == "-default-")
            m_defaultOvrs = fsa;
        else
            m_elementOvrs[static_cast<uint32_t>(std::strtoul(eo.id.c_str(), nullptr, 10))] = fsa;
    }
    sync();
}

// Ported from: FeatureOverrides.ts clear (:65-69).
void FeatureOverridesProvider::clear()
{
    m_elementOvrs.clear();
    m_defaultOvrs = std::nullopt;
    sync();
}

// Ported from: FeatureOverrides.ts defaults setter (:71-74).
void FeatureOverridesProvider::setDefaults(dqCommon::FeatureAppearance const& app)
{
    m_defaultOvrs = app;
    sync();
}

// Ported from: FeatureOverrides.ts sync (:76——vp.setFeatureOverrideProvider
// Changed——M-O(2) I10 引擎面)。
void FeatureOverridesProvider::sync()
{
    if (m_vp)
        m_vp->SetFeatureOverrideProviderChanged();
}

// Ported from: FeatureOverrides.ts get (:78-80——findFeatureOverrideProvider
// (x => x instanceof Provider))。
FeatureOverridesProvider* FeatureOverridesProvider::get(dqApp::Viewport* vp)
{
    if (!vp)
        return nullptr;
    auto* base = vp->FindFeatureOverrideProvider(
        [](dqCommon::FeatureOverrideProvider* x) {
            return liveProviders().count(static_cast<FeatureOverridesProvider*>(x)) > 0;
        });
    return base ? static_cast<FeatureOverridesProvider*>(base) : nullptr;
}

// Ported from: FeatureOverrides.ts remove (:82-86).
void FeatureOverridesProvider::remove(dqApp::Viewport* vp)
{
    auto* provider = get(vp);
    if (provider)
        vp->DropFeatureOverrideProvider(provider);
}

// Ported from: FeatureOverrides.ts getOrCreate (:88-96).
FeatureOverridesProvider* FeatureOverridesProvider::getOrCreate(dqApp::Viewport* vp)
{
    auto* provider = get(vp);
    if (!provider) {
        provider = new FeatureOverridesProvider(vp);
        vp->AddFeatureOverrideProvider(provider);
    }
    return provider;
}

// ---------------------------------------------------------------------------
// FeatureOverridesSettings
// ---------------------------------------------------------------------------

FeatureOverridesSettings::FeatureOverridesSettings(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent), m_vp(vp)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(buildColorRow());
    layout->addWidget(buildTransparencyRow());
    layout->addWidget(buildStyleRow());
    layout->addWidget(buildWeightRow());
    layout->addWidget(buildCheckBoxes());
    layout->addWidget(buildButtons());
}

void FeatureOverridesSettings::updateAppearance()
{
    // Color（:373-382——勾选 = rgb override；未勾选 "apply to lines" 时
    // lineRgb = false 形（不覆盖线色），否则 lineRgb 走独立勾选）。
    m_props.rgb = std::nullopt;
    m_props.lineRgb = std::nullopt;
    m_props.lineRgbIsFalse = false;
    if (m_colorCb->isChecked())
        m_props.rgb = dqCommon::RgbColorProps{m_color.red(), m_color.green(), m_color.blue()};
    if (m_colorCb->isChecked() && m_applyColorToLines->isChecked())
        m_props.lineRgbIsFalse = true;
    else if (m_lineColorCb->isChecked())
        m_props.lineRgb = dqCommon::RgbColorProps{m_lineColor.red(), m_lineColor.green(), m_lineColor.blue()};

    // Transparency（:253-262——勾选值/255；apply-to-lines 时 lineTransp=false
    // 形；否则独立 lineTransp）。
    m_props.transparency = std::nullopt;
    m_props.lineTransparency = std::nullopt;
    m_props.lineTransparencyIsFalse = false;
    if (m_transpCb->isChecked())
        m_props.transparency = m_transpSpin->value() / 255.0;
    if (m_transpCb->isChecked() && m_applyTranspToLines->isChecked())
        m_props.lineTransparencyIsFalse = true;
    else if (m_lineTranspCb->isChecked())
        m_props.lineTransparency = m_lineTranspSpin->value() / 255.0;
    if (!m_transpCb->isChecked() && m_lineTranspCb->isChecked())
        m_props.lineTransparency = m_lineTranspSpin->value() / 255.0;
    m_props.viewDependentTransparency = m_viewDependent->isChecked();

    // Style（:187-190——Invalid = 不覆盖）。
    m_props.linePixels = std::nullopt;
    if (m_styleCombo->currentIndex() > 0)
        m_props.linePixels = static_cast<dqCommon::LinePixels>(
            m_styleCombo->itemData(m_styleCombo->currentIndex()).toUInt());

    // Weight（:289-292）。
    m_props.weight = m_weightCb->isChecked()
        ? std::optional<double>(m_weightSpin->value())
        : std::nullopt;

    m_props.ignoresMaterial = m_ignoreMaterial->isChecked();
    m_props.nonLocatable = m_nonLocatable->isChecked();
    m_props.emphasized = m_emphasized->isChecked();
}

dqCommon::FeatureAppearance FeatureOverridesSettings::currentAppearance() const
{
    return dqCommon::FeatureAppearance::fromJSON(&m_props);
}

QWidget* FeatureOverridesSettings::buildColorRow()
{
    // 参考 :322-385（Color + Apply to lines + Line Color 的联动禁用）。
    auto* row = new QWidget(this);
    auto* layout = new QVBoxLayout(row);
    auto makeColorRow = [&](QCheckBox*& cb, QPushButton*& btn, QString const& label) {
        auto* line = new QWidget(row);
        auto* h = new QHBoxLayout(line);
        h->setContentsMargins(0, 0, 0, 0);
        cb = new QCheckBox(label, line);
        btn = new QPushButton(tr("Pick..."), line);
        btn->setEnabled(false);
        h->addWidget(cb);
        h->addWidget(btn, 1);
        layout->addWidget(line);
        QObject::connect(cb, &QCheckBox::toggled, btn, &QPushButton::setEnabled);
    };
    makeColorRow(m_colorCb, m_colorBtn, tr("Color"));
    m_applyColorToLines = new QCheckBox(tr("Apply to lines"), row);
    m_applyColorToLines->setEnabled(false);
    layout->addWidget(m_applyColorToLines);
    makeColorRow(m_lineColorCb, m_lineColorBtn, tr("Line Color"));
    // Pick… 按钮 = createColorInput 的拾色器（:331-338——默认 "#ffffff"）。
    QObject::connect(m_colorBtn, &QPushButton::clicked, this, [this] {
        QColor const c = QColorDialog::getColor(m_color, this, tr("Color"));
        if (c.isValid())
            m_color = c;
    });
    QObject::connect(m_lineColorBtn, &QPushButton::clicked, this, [this] {
        QColor const c = QColorDialog::getColor(m_lineColor, this, tr("Line Color"));
        if (c.isValid())
            m_lineColor = c;
    });
    m_lineColorCb->setEnabled(false);
    QObject::connect(m_colorCb, &QCheckBox::toggled, m_applyColorToLines, &QCheckBox::setEnabled);
    // :364-365——apply-to-lines 勾选时禁用独立 line color，反之亦然。
    QObject::connect(m_applyColorToLines, &QCheckBox::toggled, m_lineColorCb, [this](bool on) {
        if (on) m_lineColorCb->setChecked(false);
        m_lineColorCb->setEnabled(!on);
    });
    QObject::connect(m_lineColorCb, &QCheckBox::toggled, this, [this](bool on) {
        if (on) m_applyColorToLines->setChecked(false);
    });
    return row;
}

QWidget* FeatureOverridesSettings::buildTransparencyRow()
{
    // 参考 :192-263（Transp + Apply to lines + Line Transp + View-dependent）。
    auto* row = new QWidget(this);
    auto* layout = new QVBoxLayout(row);
    auto makeSpinRow = [&](QCheckBox*& cb, QSpinBox*& spin, QString const& label) {
        auto* line = new QWidget(row);
        auto* h = new QHBoxLayout(line);
        h->setContentsMargins(0, 0, 0, 0);
        cb = new QCheckBox(label, line);
        spin = new QSpinBox(line);
        spin->setRange(0, 255);
        spin->setEnabled(false);
        h->addWidget(cb);
        h->addWidget(spin, 1);
        layout->addWidget(line);
        QObject::connect(cb, &QCheckBox::toggled, spin, &QSpinBox::setEnabled);
    };
    makeSpinRow(m_transpCb, m_transpSpin, tr("Transparency"));
    m_applyTranspToLines = new QCheckBox(tr("Apply to lines"), row);
    m_applyTranspToLines->setEnabled(false);
    layout->addWidget(m_applyTranspToLines);
    makeSpinRow(m_lineTranspCb, m_lineTranspSpin, tr("Line Transparency"));
    m_lineTranspCb->setEnabled(false);
    m_viewDependent = new QCheckBox(tr("View-dependent"), row);
    layout->addWidget(m_viewDependent);
    QObject::connect(m_transpCb, &QCheckBox::toggled, m_applyTranspToLines, &QCheckBox::setEnabled);
    QObject::connect(m_applyTranspToLines, &QCheckBox::toggled, m_lineTranspCb, [this](bool on) {
        if (on) m_lineTranspCb->setChecked(false);
        m_lineTranspCb->setEnabled(!on);
    });
    return row;
}

QWidget* FeatureOverridesSettings::buildStyleRow()
{
    // 参考 :297-320（LinePixels 11 项——Invalid 起头的"Not overridden"）。
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    m_styleCombo = new QComboBox(row);
    struct Entry {
        char const* name;
        dqCommon::LinePixels value;
    };
    static Entry const kEntries[] = {
        {"Not overridden", dqCommon::LinePixels::Invalid},
        {"Solid", dqCommon::LinePixels::Solid},
        {"Hidden Line", dqCommon::LinePixels::HiddenLine},
        {"Invisible", dqCommon::LinePixels::Invisible},
        {"Code1", dqCommon::LinePixels::Code1},
        {"Code2", dqCommon::LinePixels::Code2},
        {"Code3", dqCommon::LinePixels::Code3},
        {"Code4", dqCommon::LinePixels::Code4},
        {"Code5", dqCommon::LinePixels::Code5},
        {"Code6", dqCommon::LinePixels::Code6},
        {"Code7", dqCommon::LinePixels::Code7},
    };
    for (auto const& e : kEntries)
        m_styleCombo->addItem(QString::fromUtf8(e.name),
                              static_cast<uint>(e.value));
    h->addWidget(new QLabel(tr("Style"), row));
    h->addWidget(m_styleCombo, 1);
    return row;
}

QWidget* FeatureOverridesSettings::buildWeightRow()
{
    // 参考 :265-295（复选 + 1-31 数值）。
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    m_weightCb = new QCheckBox(tr("Weight"), row);
    m_weightSpin = new QSpinBox(row);
    m_weightSpin->setRange(1, 31);
    m_weightSpin->setValue(1);
    m_weightSpin->setEnabled(false);
    QObject::connect(m_weightCb, &QCheckBox::toggled, m_weightSpin, &QSpinBox::setEnabled);
    h->addWidget(m_weightCb);
    h->addWidget(m_weightSpin, 1);
    return row;
}

QWidget* FeatureOverridesSettings::buildCheckBoxes()
{
    // 参考 :119-138。
    auto* row = new QWidget(this);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    m_ignoreMaterial = new QCheckBox(tr("Ignore Material"), row);
    m_nonLocatable = new QCheckBox(tr("Non-locatable"), row);
    m_emphasized = new QCheckBox(tr("Emphasized"), row);
    layout->addWidget(m_ignoreMaterial);
    layout->addWidget(m_nonLocatable);
    layout->addWidget(m_emphasized);
    return row;
}

QWidget* FeatureOverridesSettings::buildButtons()
{
    // 参考 :140-166（Apply/Default/Clear——作用于选择集/默认/全清）。
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    auto* apply = new QPushButton(tr("Apply"), row);
    apply->setToolTip(tr("Apply overrides to selection set"));
    auto* defaults = new QPushButton(tr("Default"), row);
    defaults->setToolTip(tr("Set as default overrides"));
    auto* clear = new QPushButton(tr("Clear"), row);
    clear->setToolTip(tr("Remove all overrides"));
    h->addWidget(apply);
    h->addWidget(defaults);
    h->addWidget(clear);
    QObject::connect(apply, &QPushButton::clicked, this, [this] {
        updateAppearance();
        if (auto* provider = FeatureOverridesProvider::getOrCreate(m_vp))
            provider->overrideElements(currentAppearance());
    });
    QObject::connect(defaults, &QPushButton::clicked, this, [this] {
        updateAppearance();
        if (auto* provider = FeatureOverridesProvider::getOrCreate(m_vp))
            provider->setDefaults(currentAppearance());
    });
    QObject::connect(clear, &QPushButton::clicked, this, [this] {
        if (auto* provider = FeatureOverridesProvider::getOrCreate(m_vp))
            provider->clear();
    });
    return row;
}

// ---------------------------------------------------------------------------
// FeatureOverridesPanel
// ---------------------------------------------------------------------------

FeatureOverridesPanel::FeatureOverridesPanel(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent, Qt::Popup)
{
    // 参考 _open (:405——new Settings(this._vp, this._parent)) + onViewChanged
    // → Provider.remove (:400-403——视图更换时旧 provider 随面板生命周期撤下;
    // DanQing 面板存活期以弹出宿主为界，getOrCreate 按需重建)。
    auto* layout = new QVBoxLayout(this);
    m_settings = new FeatureOverridesSettings(vp, this);
    layout->addWidget(m_settings);
    setWindowTitle(tr("Override feature symbology"));
    resize(280, 420);
}

}  // namespace Gui
