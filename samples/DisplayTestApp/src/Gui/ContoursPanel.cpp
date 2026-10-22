// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Contour display 下拉面板实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/Contours.ts
#include "ContoursPanel.h"

#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqBase/CompressedId64Set.h>
#include <dqBase/DqId.h>
#include <dqCommon/ColorDef.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace Gui {
namespace {

QFrame* makeHRule(QWidget* parent)
{
    // <hr style.borderColor="grey">（:107-112/:138-141/:149-151/:165-167）
    auto* rule = new QFrame(parent);
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Sunken);
    return rule;
}

QLabel* makeBoldLabel(QString const& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("font-weight: bold"));
    return label;
}

// LinePixels 条目表（addStyle :403-414——Solid/Code1-7/Hidden Line/Invisible）。
struct StyleEntry {
    char const* name;
    dqCommon::LinePixels value;
};
constexpr StyleEntry kStyleEntries[] = {
    {"Solid", dqCommon::LinePixels::Solid},
    {"Code1", dqCommon::LinePixels::Code1},
    {"Code2", dqCommon::LinePixels::Code2},
    {"Code3", dqCommon::LinePixels::Code3},
    {"Code4", dqCommon::LinePixels::Code4},
    {"Code5", dqCommon::LinePixels::Code5},
    {"Code6", dqCommon::LinePixels::Code6},
    {"Code7", dqCommon::LinePixels::Code7},
    {"Hidden Line", dqCommon::LinePixels::HiddenLine},
    {"Invisible", dqCommon::LinePixels::Invisible},
};

QComboBox* createStyleCombo(QWidget* parent, char const* objectName)
{
    // addStyle（:402-428）——ComboBox name "Style " + 十条目。
    auto* combo = new QComboBox(parent);
    combo->setObjectName(QString::fromLatin1(objectName));
    for (auto const& e : kStyleEntries) {
        combo->addItem(QString::fromLatin1(e.name),
                       QVariant::fromValue(static_cast<uint>(e.value)));
    }
    return combo;
}

// updateSliderValue（frontend-devtools）——HTML range 赋值落 [min,max]×step
// 网格（浏览器钳制）→ qRound 到 ×2 整数域再钳位。
void updateSliderValue(QSlider* slider, QLabel* readout, double value)
{
    const int v = qRound(value * 2.0);
    slider->setValue(std::clamp(v, slider->minimum(), slider->maximum()));
    readout->setText(QString::number(slider->value() / 2.0));
}

// select.value = pattern.toString()（:274-275）——无匹配条目 → -1
// （select 置空语义）。
void selectComboByData(QComboBox* combo, dqCommon::LinePixels pattern)
{
    combo->setCurrentIndex(combo->findData(QVariant::fromValue(static_cast<uint>(pattern))));
}

}  // namespace

ContoursPanel::ContoursPanel(dqApp::Viewport* vp, QWidget* parent)
    : QWidget(parent), m_vp(vp)
{
    // :59-65——toolMenu 容器（className "toolMenu" → objectName；宽
    // winSize.width(318)*0.98）。
    setObjectName(QStringLiteral("contoursSettings"));
    setMinimumWidth(312);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);

    // :75-84——display contours 复选（经 A 道门面 withDisplayContours 语义）。
    m_checkbox = new QCheckBox(QStringLiteral("Display contours in the viewport"), this);
    m_checkbox->setObjectName(QStringLiteral("cbx_toggleDisplayContours"));
    layout->addWidget(m_checkbox);

    // :86-105——Contour Group combobox（条目 0-4、值 0、label bold）。
    {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(makeBoldLabel(QStringLiteral("Contour Group: "), row));
        m_combobox = new QComboBox(row);
        m_combobox->setObjectName(QStringLiteral("viewAttr_renderingStyle"));
        for (int i = 0; i <= 4; ++i)
            m_combobox->addItem(QString::number(i), i);
        m_combobox->setCurrentIndex(0);
        rl->addWidget(m_combobox);
        rl->addStretch(1);
        layout->addWidget(row);
    }

    // :107-112——双 <hr>。
    layout->addWidget(makeHRule(this));
    layout->addWidget(makeHRule(this));

    // :180-203——Add/Apply/Delete 三钮（居中 buttonDiv；DOM 序在组编辑器之前）。
    {
        auto* buttonRow = new QWidget(this);
        auto* bl = new QHBoxLayout(buttonRow);
        bl->setContentsMargins(0, 0, 0, 0);
        auto* addButton = new QPushButton(QStringLiteral("Add"), buttonRow);
        addButton->setObjectName(QStringLiteral("contours_add"));
        addButton->setToolTip(QStringLiteral("Add a new contour grouping"));
        auto* applyButton = new QPushButton(QStringLiteral("Apply"), buttonRow);
        applyButton->setObjectName(QStringLiteral("contours_apply"));
        applyButton->setToolTip(QStringLiteral("Apply contour settings for this definition"));
        auto* deleteButton = new QPushButton(QStringLiteral("Delete"), buttonRow);
        deleteButton->setObjectName(QStringLiteral("contours_delete"));
        deleteButton->setToolTip(QStringLiteral("Delete contour settings for this definition"));
        bl->addStretch(1);
        bl->addWidget(addButton);
        bl->addWidget(applyButton);
        bl->addWidget(deleteButton);
        bl->addStretch(1);
        layout->addWidget(buttonRow);

        QObject::connect(addButton, &QPushButton::clicked, this, [this]() { addContourDef(); });      // :184
        QObject::connect(applyButton, &QPushButton::clicked, this, [this]() { applyContourDef(); });  // :191
        QObject::connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteContourDef(); });// :198
    }

    // :67-73/:205——_curGroupDiv（toolMenu2；组编辑器容器，显隐受
    // toggleCurrentGroup 门控）。
    m_curGroupDiv = new QWidget(this);
    m_curGroupDiv->setObjectName(QStringLiteral("curGroupDiv"));
    auto* cg = new QVBoxLayout(m_curGroupDiv);
    cg->setContentsMargins(0, 0, 0, 0);

    // :114-124——Name textbox（tooltip=placeholder 语义）。
    {
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 3);  // indent1/bottomSpace1
        rl->addWidget(makeBoldLabel(QStringLiteral("Name: "), row));
        m_nameTextBox = new QLineEdit(row);
        m_nameTextBox->setObjectName(QStringLiteral("contours_name"));
        m_nameTextBox->setToolTip(QStringLiteral("Enter an optional name to identify this grouping"));
        rl->addWidget(m_nameTextBox);
        cg->addWidget(row);
    }

    // :126-136——Subcategory Ids textbox。
    {
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 3);
        rl->addWidget(makeBoldLabel(QStringLiteral("Subcategory Ids: "), row));
        m_subCatTextBox = new QLineEdit(row);
        m_subCatTextBox->setObjectName(QStringLiteral("contours_subCatIds"));
        m_subCatTextBox->setToolTip(
            QStringLiteral("Enter comma-separated list of Subcategory Ids to associate this contour styling with"));
        rl->addWidget(m_subCatTextBox);
        cg->addWidget(row);
    }

    cg->addWidget(makeHRule(m_curGroupDiv));  // :138-140

    // :142-148——Show Element 复选（handler no-op :147）。
    m_dispElemCkbx = new QCheckBox(QStringLiteral("Show Element"), m_curGroupDiv);
    m_dispElemCkbx->setObjectName(QStringLiteral("cbx_toggleShowElement"));
    m_dispElemCkbx->setToolTip(QStringLiteral("Display element where contours are not applied"));
    cg->addWidget(m_dispElemCkbx);
    cg->addWidget(makeHRule(m_curGroupDiv));  // :149-151

    // Major Contours 分区（:153-163）。
    cg->addWidget(makeBoldLabel(QStringLiteral("Major Contours"), m_curGroupDiv));
    {
        // :159/:342-358——addInterval(true)：value 5、min 1、step 1、
        // parseAsFloat、name "Major Count "。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Major Count "), row));
        m_majorIntervalCount = new QLineEdit(row);
        m_majorIntervalCount->setObjectName(QStringLiteral("major_interval"));
        m_majorIntervalCount->setText(QStringLiteral("5"));
        rl->addWidget(m_majorIntervalCount);
        cg->addWidget(row);
    }
    {
        // :160/:360-379——addColor(true)：label "Color"、value "#000000"。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 5, 0, 5);
        rl->addWidget(new QLabel(QStringLiteral("Color"), row));
        m_majorColor = new QLineEdit(row);
        m_majorColor->setObjectName(QStringLiteral("major_color"));
        m_majorColor->setText(QStringLiteral("#000000"));
        rl->addWidget(m_majorColor);
        cg->addWidget(row);
    }
    {
        // :161/:381-400——addWidth(true)：min 1.0/max 8.5/step 0.5/value 1.5
        //   ×readout（right）→ QSlider 2..17 + readout（EQUIVALENCE 头注）。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral(" Width "), row));
        m_majorWidth = new QSlider(Qt::Horizontal, row);
        m_majorWidth->setObjectName(QStringLiteral("major_width"));
        m_majorWidth->setRange(2, 17);   // 1.0 .. 8.5 step 0.5（×2 整数域）
        m_majorWidth->setValue(3);       // 1.5（:385）
        rl->addWidget(m_majorWidth);
        m_majorWidthReadoutPlaceholder = new QLabel(QStringLiteral("1.5"), row);
        rl->addWidget(m_majorWidthReadoutPlaceholder);
        cg->addWidget(row);
        QObject::connect(m_majorWidth, &QSlider::valueChanged, this, [this](int v) {
            m_majorWidthReadoutPlaceholder->setText(QString::number(v / 2.0));
        });
    }
    {
        // :162/:402-428——addStyle(Solid, true)。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 5);
        rl->addWidget(new QLabel(QStringLiteral("Style "), row));
        m_majorLineStyle = createStyleCombo(row, "major_style");
        rl->addWidget(m_majorLineStyle);
        rl->addStretch(1);
        cg->addWidget(row);
    }

    cg->addWidget(makeHRule(m_curGroupDiv));  // :165-167

    // Minor Contours 分区（:168-178）。
    cg->addWidget(makeBoldLabel(QStringLiteral("Minor Contours"), m_curGroupDiv));
    {
        // :174——addInterval(false)：value 1、min 0、name "Minor Interval "。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Minor Interval "), row));
        m_minorInterval = new QLineEdit(row);
        m_minorInterval->setObjectName(QStringLiteral("minor_interval"));
        m_minorInterval->setText(QStringLiteral("1"));
        rl->addWidget(m_minorInterval);
        cg->addWidget(row);
    }
    {
        // :175——addColor(false)。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 5, 0, 5);
        rl->addWidget(new QLabel(QStringLiteral("Color"), row));
        m_minorColor = new QLineEdit(row);
        m_minorColor->setObjectName(QStringLiteral("minor_color"));
        m_minorColor->setText(QStringLiteral("#000000"));
        rl->addWidget(m_minorColor);
        cg->addWidget(row);
    }
    {
        // :176——addWidth(false)。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral(" Width "), row));
        m_minorWidth = new QSlider(Qt::Horizontal, row);
        m_minorWidth->setObjectName(QStringLiteral("minor_width"));
        m_minorWidth->setRange(2, 17);
        m_minorWidth->setValue(3);
        rl->addWidget(m_minorWidth);
        m_minorWidthReadoutPlaceholder = new QLabel(QStringLiteral("1.5"), row);
        rl->addWidget(m_minorWidthReadoutPlaceholder);
        cg->addWidget(row);
        QObject::connect(m_minorWidth, &QSlider::valueChanged, this, [this](int v) {
            m_minorWidthReadoutPlaceholder->setText(QString::number(v / 2.0));
        });
    }
    {
        // :177——addStyle(Solid, false)。
        auto* row = new QWidget(m_curGroupDiv);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(5, 0, 0, 5);
        rl->addWidget(new QLabel(QStringLiteral("Style "), row));
        m_minorLineStyle = createStyleCombo(row, "minor_style");
        rl->addWidget(m_minorLineStyle);
        rl->addStretch(1);
        cg->addWidget(row);
    }

    layout->addWidget(m_curGroupDiv);  // :205

    // --- handler 接线 ---
    // :79-83——display 复选：withDisplayContours + sync。
    QObject::connect(m_checkbox, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_vp->GetView() == nullptr)
            return;
        dqApp::DisplayStyle& style = m_vp->GetView()->GetDisplayStyle();
        style.setContours(style.getContours().withDisplayContours(checked));
        sync();
    });
    // :101-103——组切换：loadContourDef(parseInt(value))。
    QObject::connect(m_combobox, &QComboBox::currentIndexChanged, this, [this](int index) {
        loadContourDef(index);
    });

    // :208-212——ctor 尾：toggleCurrentGroup(false) + loadContourDef(0) +
    //   checkbox.checked = settings.contours.displayContours（直赋不触发
    //   handler → QSignalBlocker）。
    toggleCurrentGroup(false);
    loadContourDef(m_currentContourIndex);
    {
        const bool displayContours =
            (m_vp->GetView() != nullptr)
                ? m_vp->GetView()->GetDisplayStyle().getContours().displayContours
                : false;
        QSignalBlocker block(m_checkbox);
        m_checkbox->setChecked(displayContours);
    }
}

// :219-222
dqCommon::ContourDisplayProps ContoursPanel::getContourDisplayProps() const
{
    return m_vp->GetView()->GetDisplayStyle().getContours().toJSON();
}

// :224-227——Number.parseFloat：前导数值前缀解析（"2.5x"→2.5）、无前缀 →
//   NaN → undefined。strtod 与 parseFloat 同为前缀解析语义。
std::optional<double> ContoursPanel::tryParseFloat(QString const& value)
{
    std::string const s = value.toStdString();
    if (s.empty())
        return std::nullopt;
    char* end = nullptr;
    const double n = std::strtod(s.c_str(), &end);
    if (end == s.c_str())
        return std::nullopt;
    return n;
}

// :229-251——UI → ContourGroupProps 组装。
dqCommon::ContourGroupProps ContoursPanel::getTerrainProps() const
{
    // :230-243——两样式（色/宽/型）。
    auto toStyleProps = [](QLineEdit* colorEdit, QSlider* widthSlider,
                           QComboBox* styleCombo) {
        dqCommon::ContourStyleProps style;
        // :230-231/:237-238——tryComputeTbgrFromString → ColorDef.fromJSON
        //   （undefined→黑，ColorDef.ts:86-88 default:0）→ fromColorDef。
        //   参考 `majorColor ? ... : undefined` 的 undefined 臂不可达
        //   （fromJSON 恒返对象——恒真）。
        auto const tbgr = dqCommon::ColorDef::tryComputeTbgrFromString(
            colorEdit->text().toStdString());
        style.color = dqCommon::RgbColor::fromColorDef(
                           dqCommon::ColorDef::fromTbgr(tbgr.value_or(0)))
                           .toJSON();
        // :238——slider.value（HTML 值串恒数值——×2 域直读等价）。
        style.pixelWidth = widthSlider->value() / 2.0;
        // :239——parseFloat(select.value)：select 值恒为条目数值（十条目全
        //   枚举、parseFloat 恒成功无 undefined 臂）→ currentData 直读。
        style.pattern = static_cast<dqCommon::LinePixels>(styleCombo->currentData().toUInt());
        return style;
    };

    dqCommon::ContourGroupProps props;
    // :233——sortAndCompress(split(","))：无效元素被忽略（compressIds
    //   CompressedId64Set.ts:108-109 "ignore garbage and invalid Ids"）——
    //   空文本/垃圾 token 均落空集 = default group。
    {
        dqBase::DqIdSet ids;
        const QStringList parts = m_subCatTextBox->text().split(QChar(','), Qt::KeepEmptyParts);
        for (QString const& part : parts) {
            std::string const s = part.toStdString();
            // Id64.isValidId64 合法性门（DqId::IsWellFormedString=其移植）。
            if (!dqBase::DqId::IsWellFormedString(s))
                continue;
            ids.insert(dqBase::DqId::FromString(s));
        }
        props.subCategories = dqBase::CompressedId64Set::SortAndCompress(ids);
    }
    // :234
    props.name = m_nameTextBox->text().toStdString();
    // :235-249
    dqCommon::ContourProps contourDef;
    contourDef.majorStyle = toStyleProps(m_majorColor, m_majorWidth, m_majorLineStyle);
    contourDef.minorStyle = toStyleProps(m_minorColor, m_minorWidth, m_minorLineStyle);
    // :246——majorIntervalCount: tryParseFloat(...)（number|undefined →
    //   props 的 int 承载——直截）。
    if (auto const v = tryParseFloat(m_majorIntervalCount->text())) {
        contourDef.majorIntervalCount = static_cast<int>(*v);
    }
    contourDef.minorInterval = tryParseFloat(m_minorInterval->text());
    contourDef.showGeometry = m_dispElemCkbx->isChecked();
    props.contourDef = contourDef;
    return props;
}

// :253-279
void ContoursPanel::loadContourDef(int index)
{
    m_currentContourIndex = index;

    if (m_vp->GetView() == nullptr)
        return;  // assert(view.is3d()) → 早退（EQUIVALENCE 头注）

    std::vector<dqCommon::ContourGroup> const& groups =
        m_vp->GetView()->GetDisplayStyle().getContours().groups;
    if (m_currentContourIndex > static_cast<int>(groups.size()) - 1) {
        toggleCurrentGroup(false);
        return;
    }

    toggleCurrentGroup(true);

    dqCommon::ContourGroup const& group =
        groups[static_cast<size_t>(m_currentContourIndex)];
    // :266-267——[...subCats].join(",")（undefined → []）。
    {
        QStringList ids;
        for (dqBase::DqId const id :
             dqBase::CompressedId64Set::DecompressArray(group.subCategories))
            ids << QString::fromStdString(id.ToString());
        m_subCatTextBox->setText(ids.join(QChar(',')));
    }
    m_nameTextBox->setText(QString::fromStdString(group.name));  // :268
    // :269——`?? Contour.fromJSON({})` 臂不可达（contourDef 恒定义——值成员）。
    dqCommon::Contour const& curContourDef = group.contourDef;
    m_majorColor->setText(QString::fromStdString(curContourDef.majorStyle.color.toHexString()));  // :270
    m_minorColor->setText(QString::fromStdString(curContourDef.minorStyle.color.toHexString()));  // :271
    updateSliderValue(m_majorWidth, m_majorWidthReadoutPlaceholder,
                      curContourDef.majorStyle.pixelWidth);                                       // :272
    updateSliderValue(m_minorWidth, m_minorWidthReadoutPlaceholder,
                      curContourDef.minorStyle.pixelWidth);                                       // :273
    selectComboByData(m_majorLineStyle, curContourDef.majorStyle.pattern);                        // :274
    selectComboByData(m_minorLineStyle, curContourDef.minorStyle.pattern);                        // :275
    m_majorIntervalCount->setText(QString::number(curContourDef.majorIntervalCount));             // :276
    m_minorInterval->setText(QString::number(curContourDef.minorInterval));                       // :277
    m_dispElemCkbx->setChecked(curContourDef.showGeometry);                                       // :278 ?? true 臂不可达
}

// :281-300
void ContoursPanel::addContourDef()
{
    if (m_vp->GetView() == nullptr)
        return;

    dqCommon::ContourDisplayProps contoursJson = getContourDisplayProps();
    std::vector<dqCommon::ContourGroupProps> groups =
        contoursJson.groups.value_or(std::vector<dqCommon::ContourGroupProps>{});

    if (static_cast<int>(groups.size()) + 1 > 5) {
        return;
    }

    groups.push_back(dqCommon::ContourGroupProps{});  // push({})——默认组
    contoursJson.groups = groups;
    m_vp->GetView()->GetDisplayStyle().setContours(
        dqCommon::ContourDisplay::fromJSON(&contoursJson));

    const int newNdx = static_cast<int>(groups.size()) - 1;
    loadContourDef(newNdx);
    {   // :297——程序化 select.value 赋值不触发 onchange → QSignalBlocker。
        QSignalBlocker block(m_combobox);
        m_combobox->setCurrentIndex(newNdx);
    }

    sync();
}

// :302-317
void ContoursPanel::applyContourDef()
{
    if (m_vp->GetView() == nullptr)
        return;

    dqCommon::ContourDisplayProps contoursJson = getContourDisplayProps();
    std::vector<dqCommon::ContourGroupProps> groups =
        contoursJson.groups.value_or(std::vector<dqCommon::ContourGroupProps>{});

    if (m_currentContourIndex > static_cast<int>(groups.size()) - 1) {
        return;
    }

    groups[static_cast<size_t>(m_currentContourIndex)] = getTerrainProps();
    contoursJson.groups = groups;
    m_vp->GetView()->GetDisplayStyle().setContours(
        dqCommon::ContourDisplay::fromJSON(&contoursJson));

    sync();
}

// :319-340
void ContoursPanel::deleteContourDef()
{
    if (m_vp->GetView() == nullptr)
        return;

    dqCommon::ContourDisplayProps contoursJson = getContourDisplayProps();
    std::vector<dqCommon::ContourGroupProps> groups =
        contoursJson.groups.value_or(std::vector<dqCommon::ContourGroupProps>{});

    if (m_currentContourIndex > static_cast<int>(groups.size()) - 1) {
        return;
    }

    groups.erase(groups.begin() + m_currentContourIndex);  // splice(index, 1)

    contoursJson.groups = groups;
    m_vp->GetView()->GetDisplayStyle().setContours(
        dqCommon::ContourDisplay::fromJSON(&contoursJson));

    m_currentContourIndex = 0;
    {   // :336——同 addContourDef 的 QSignalBlocker 注。
        QSignalBlocker block(m_combobox);
        m_combobox->setCurrentIndex(0);
    }
    loadContourDef(m_currentContourIndex);

    sync();
}

// :48-50
void ContoursPanel::toggleCurrentGroup(bool enable)
{
    m_curGroupDiv->setVisible(enable);
}

// :430-432
void ContoursPanel::sync()
{
    m_vp->synchWithView();
}

}  // namespace Gui
