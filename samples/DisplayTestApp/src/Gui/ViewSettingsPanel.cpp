// Ported from: itwinjs-core display-test-app ViewAttributes.ts:302-327（View Flags
// 组 + Camera + Monochrome + renderMode 下拉）+ Viewer.ts:327-335（下拉入口）。
#include "ViewSettingsPanel.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWidget>

#include <dqApp/Application.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqCommon/ViewFlags.h>

namespace Gui {
namespace {

dqApp::Viewport* activeViewport()
{
    return dqApp::Application::Get().GetViewManager().GetActiveViewport();
}

// tbgr(0xTTBBGGRR) ↔ QColor 互转（DisplayStyle 的 monochromeColor 载体是
// ColorDef.tbgr——DisplayStyle.h:87）。
uint32_t QColorToTbgr(QColor const& c)
{
    return (static_cast<uint32_t>(c.alpha()) << 24)
         | (static_cast<uint32_t>(c.blue()) << 16)
         | (static_cast<uint32_t>(c.green()) << 8)
         | static_cast<uint32_t>(c.red());
}

QColor TbgrToQColor(uint32_t tbgr)
{
    return QColor(static_cast<int>(tbgr & 0xFF),
                  static_cast<int>((tbgr >> 8) & 0xFF),
                  static_cast<int>((tbgr >> 16) & 0xFF),
                  static_cast<int>((tbgr >> 24) & 0xFF));
}

// DTA renderMode 下拉（ViewAttributes.ts addRenderMode :425-428）。dqCommon::RenderMode
// 现有 4 值（CrossingEdges/HiddenLineVisibleEdges 未移植）。
const struct { const char* name; dqCommon::RenderMode mode; } kModes[] = {
    { "Wireframe", dqCommon::RenderMode::Wireframe },
    { "Solid Fill", dqCommon::RenderMode::SolidFill },
    { "Hidden Line", dqCommon::RenderMode::HiddenLine },
    { "Smooth Shade", dqCommon::RenderMode::SmoothShade },
};
}  // namespace

ViewSettingsPanel::ViewSettingsPanel(QWidget* parent)
    : QFrame(parent, Qt::Popup)
{
    // DTA 面板样式（index.css .toolMenu）：#f0f0f0 底 + 灰边框。Popup 不设背景时
    // 透明叠在深色 3D 视口上。两处硬约束：
    // ① 应用加载 freecad.qss（main.cpp:62-68）后 QPalette 被 QSS 引擎完全忽略
    //   ——须走样式表；qss 的弹窗规则是 QFrame[class="popup"]（FreeCAD 约定）；
    // ② 用户系统为深色应用模式（无 qss 命中时 QSS 引擎默认深底）。
    // 故：挂 class=popup 属性（吃 qss 规则）+ 面板级样式表（优先级高于应用级
    // qss，兜底）+ 显式浅色调色板（无 qss 环境如无头测试）。
    setProperty("class", "popup");
    setObjectName(QStringLiteral("DTA.ViewSettingsPanel"));
    setFrameShape(QFrame::StyledPanel);
    setStyleSheet(QStringLiteral(
        "#DTA.ViewSettingsPanel { background-color: #f0f0f0; border: 1px solid #b0b0b0; }"
        "#DTA.ViewSettingsPanel QLabel { color: #1a1a1a; background: transparent; }"
        "#DTA.ViewSettingsPanel QLabel:disabled { color: #7a7a7a; }"
        "#DTA.ViewSettingsPanel QCheckBox { color: #1a1a1a; background: transparent; }"
        "#DTA.ViewSettingsPanel QCheckBox:disabled { color: #7a7a7a; }"
        "#DTA.ViewSettingsPanel QComboBox { color: #1a1a1a; background: #f7f7f7; }"));
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(0xf0, 0xf0, 0xf0));       // .toolMenu 背景色
    pal.setColor(QPalette::Base, QColor(0xf7, 0xf7, 0xf7));
    pal.setColor(QPalette::AlternateBase, QColor(0xf0, 0xf0, 0xf0));
    pal.setColor(QPalette::Text, QColor(0x1a, 0x1a, 0x1a));
    pal.setColor(QPalette::WindowText, QColor(0x1a, 0x1a, 0x1a));
    pal.setColor(QPalette::ButtonText, QColor(0x1a, 0x1a, 0x1a));
    pal.setColor(QPalette::Button, QColor(0xf7, 0xf7, 0xf7));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x7a, 0x7a, 0x7a));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0x7a, 0x7a, 0x7a));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x7a, 0x7a, 0x7a));
    setPalette(pal);
    setAutoFillBackground(true);
    setMinimumWidth(230);   // .toolMenu width 210px + 边距

    auto* layout = new QVBoxLayout(this);

    // Display Style 下拉置灰标注（ViewAttributes.ts populate——blank 单样式）。
    auto* dsLabel = new QLabel(QStringLiteral("Display Style (not yet implemented)"), this);
    dsLabel->setEnabled(false);
    layout->addWidget(dsLabel);

    // Render Mode 下拉（真）。
    m_renderMode = new QComboBox(this);
    m_renderMode->setObjectName(QStringLiteral("RenderMode"));
    for (auto const& m : kModes)
        m_renderMode->addItem(QString::fromLatin1(m.name));
    connect(m_renderMode, &QComboBox::currentTextChanged, this, [this](QString const& text) {
        for (auto const& m : kModes)
            if (text == QLatin1String(m.name)) {
                applyFlags([mode = m.mode](dqCommon::ViewFlagsProperties& p) { p.renderMode = mode; });
            }
    });
    layout->addWidget(m_renderMode);

    // View Flags 复选组（ViewAttributes.ts:302-313 顺序，12 项；
    // Camera→Monochrome 按参考 :315-316 顺序在循环后追加）。
    struct Flag { const char* label; bool dqCommon::ViewFlagsProperties::*field; };
    const Flag kFlags[] = {
        { "ACS Triad", &dqCommon::ViewFlagsProperties::acsTriad },
        { "Grid", &dqCommon::ViewFlagsProperties::grid },
        { "Fill", &dqCommon::ViewFlagsProperties::fill },
        { "Materials", &dqCommon::ViewFlagsProperties::materials },
        { "Textures", &dqCommon::ViewFlagsProperties::textures },
        { "Constructions", &dqCommon::ViewFlagsProperties::constructions },
        { "Transparency", &dqCommon::ViewFlagsProperties::transparency },
        { "Line Weights", &dqCommon::ViewFlagsProperties::weights },
        { "Line Styles", &dqCommon::ViewFlagsProperties::styles },
        { "Clip Volume", &dqCommon::ViewFlagsProperties::clipVolume },
        { "Force Surface Discard", &dqCommon::ViewFlagsProperties::forceSurfaceDiscard },
        { "White-on-white Reversal", &dqCommon::ViewFlagsProperties::whiteOnWhiteReversal },
    };
    for (auto const& f : kFlags) {
        auto* cb = new QCheckBox(QString::fromLatin1(f.label), this);
        cb->setObjectName(QString::fromLatin1(f.label));
        connect(cb, &QCheckBox::toggled, this, [this, field = f.field](bool on) {
            applyFlags([field, on](dqCommon::ViewFlagsProperties& p) { p.*field = on; });
        });
        layout->addWidget(cb);
    }

    // Camera（ViewAttributes.ts:367——非 viewFlags 位，走 ViewState3d 相机开关）。
    // 顺序对齐参考 ViewAttributes.ts:315-316：Camera 在 Monochrome 之前。
    auto* cam = new QCheckBox(QStringLiteral("Camera"), this);
    cam->setObjectName(QStringLiteral("Camera"));
    connect(cam, &QCheckBox::toggled, this, [](bool on) {
        auto* vp = activeViewport();
        if (!vp) return;
        auto* v3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
        if (!v3d) return;
        if (on) v3d->EnableCamera(); else v3d->TurnCameraOff();
        // 参考 ViewAttributes.ts:367-373 切换后 this.sync(true) → vp.synchWithView
        // （cameraOn 属 ViewPose3d::equalState 比较项）——接撤销栈。
        vp->synchWithView();
    });
    layout->addWidget(cam);

    // Monochrome（ViewAttributes.ts addMonochrome:386-419——monochrome viewFlag 位
    // + Color 输入 + "Scaled" 复选；M-O(1) I1 补齐后两个子项，行可见性随
    // viewFlags.monochrome——参考 _updates push :410-418）。
    m_monochromeRow = new QWidget(this);
    m_monochromeRow->setObjectName(QStringLiteral("MonochromeRow"));
    auto* monoLayout = new QHBoxLayout(m_monochromeRow);
    monoLayout->setContentsMargins(0, 0, 0, 0);
    auto* colorLabel = new QLabel(QStringLiteral("Color"), m_monochromeRow);
    m_monochromeColorButton = new QPushButton(m_monochromeRow);
    m_monochromeColorButton->setObjectName(QStringLiteral("MonochromeColor"));
    m_monochromeColorButton->setFixedWidth(40);
    connect(m_monochromeColorButton, &QPushButton::clicked, this, [this]() {
        // createColorInput 的取色对话框等价物（frontend-devtools 组件——
        // 浏览器 <input type=color>；Qt 对应物 QColorDialog::getColor）。
        auto* vp = activeViewport();
        uint32_t const initialTbgr
            = (vp && vp->GetView()) ? vp->GetView()->GetDisplayStyle().getMonochromeColor() : 0xFF000000;
        QColor const picked = QColorDialog::getColor(TbgrToQColor(initialTbgr), this, tr("Monochrome Color"));
        if (picked.isValid())
            applyMonochromeColor(picked);
    });
    m_scaledCheckbox = new QCheckBox(QStringLiteral("Scaled"), m_monochromeRow);
    m_scaledCheckbox->setObjectName(QStringLiteral("MonochromeScaled"));
    connect(m_scaledCheckbox, &QCheckBox::toggled, this, [](bool on) {
        // ViewAttributes.ts:400-402 — monochromeMode = Scaled : Flat。
        auto* vp = activeViewport();
        if (!vp || !vp->GetView()) return;
        vp->GetView()->GetDisplayStyle().setMonochromeMode(
            on ? dqCommon::MonochromeMode::Scaled : dqCommon::MonochromeMode::Flat);
        vp->SetupFromView();
    });
    monoLayout->addWidget(colorLabel);
    monoLayout->addWidget(m_monochromeColorButton);
    monoLayout->addWidget(m_scaledCheckbox);
    monoLayout->addStretch(1);
    layout->addWidget(m_monochromeRow);
    m_monochromeRow->setVisible(false);   // 默认关（syncFromViewport 按位回显）

    auto* mono = new QCheckBox(QStringLiteral("Monochrome"), this);
    mono->setObjectName(QStringLiteral("Monochrome"));
    connect(mono, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.monochrome = on; });
        // 子行可见性随位（ViewAttributes.ts:410-418 updates push）。
        m_monochromeRow->setVisible(on);
    });
    layout->addWidget(mono);

    // Edge Display 开关（M-L(3) 接线级 #4——渲染侧 M-I(4) 已通，只差开关）。
    // Ported from: ViewAttributes.addEdgeDisplay (:835-1008) — "Visible Edges" →
    // viewFlags.visibleEdges (:863-869)、"Hidden Edges" → viewFlags.hiddenEdges
    // (:878-884)（参考的 hline 色宽样式覆写编辑器 addHiddenLineEditor :906-1008
    // 仍为移植级——登记）。参考切换后 this.sync() 同步面板；本面板的回读在
    // syncFromViewport（弹出时）。
    auto* visEdges = new QCheckBox(QStringLiteral("Visible Edges"), this);
    visEdges->setObjectName(QStringLiteral("Visible Edges"));
    connect(visEdges, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.visibleEdges = on; });
    });
    layout->addWidget(visEdges);
    auto* hidEdges = new QCheckBox(QStringLiteral("Hidden Edges"), this);
    hidEdges->setObjectName(QStringLiteral("Hidden Edges"));
    connect(hidEdges, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.hiddenEdges = on; });
    });
    layout->addWidget(hidEdges);

    // 置灰分区标注（DTA 面板的其余分区：Environment/BackgroundMap/边线样式编辑器/AO/Thematic）。
    const char* disabledSections[] = {
        "Environment editor", "Background Map", "Edge style editor (hline overrides)",
        "Ambient Occlusion", "Thematic Display",
    };
    for (auto* s : disabledSections) {
        auto* l = new QLabel(QString::fromLatin1(s) + QStringLiteral(" (not yet implemented)"), this);
        l->setEnabled(false);
        layout->addWidget(l);
    }
}

void ViewSettingsPanel::applyFlags(std::function<void(dqCommon::ViewFlagsProperties&)> mod)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView()) return;
    auto& style = vp->GetView()->GetDisplayStyle();
    auto props = style.getViewFlags().Properties();
    mod(props);
    style.setViewFlags(dqCommon::ViewFlags(props));
    vp->SetupFromView();
}

void ViewSettingsPanel::applyMonochromeColor(QColor const& color)
{
    // ViewAttributes.ts:393-396 — `settings.monochromeColor = ColorDef.create(
    // color); this.sync()`（sync → 视口失效重绘；DanQing 对应 SetupFromView）。
    auto* vp = activeViewport();
    if (!vp || !vp->GetView()) return;
    vp->GetView()->GetDisplayStyle().setMonochromeColor(QColorToTbgr(color));
    m_monochromeColorButton->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #808080;")
                                              .arg(color.name()));
    vp->SetupFromView();
}

void ViewSettingsPanel::syncFromViewport()
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView()) return;
    auto const props = vp->GetView()->GetDisplayStyle().getViewFlags().Properties();

    const QSignalBlocker blocker(m_renderMode);
    for (int i = 0; i < m_renderMode->count(); ++i)
        if (kModes[i].mode == props.renderMode) { m_renderMode->setCurrentIndex(i); break; }

    for (auto* cb : findChildren<QCheckBox*>()) {
        const QSignalBlocker cb2(cb);
        const auto name = cb->objectName();
        if (name == "Camera") {
            auto* v3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
            if (!v3d) continue;
            cb->setChecked(v3d->IsCameraOn());
            continue;
        }
        if (name == "ACS Triad") cb->setChecked(props.acsTriad);
        else if (name == "Grid") cb->setChecked(props.grid);
        else if (name == "Fill") cb->setChecked(props.fill);
        else if (name == "Materials") cb->setChecked(props.materials);
        else if (name == "Textures") cb->setChecked(props.textures);
        else if (name == "Constructions") cb->setChecked(props.constructions);
        else if (name == "Transparency") cb->setChecked(props.transparency);
        else if (name == "Line Weights") cb->setChecked(props.weights);
        else if (name == "Line Styles") cb->setChecked(props.styles);
        else if (name == "Clip Volume") cb->setChecked(props.clipVolume);
        else if (name == "Force Surface Discard") cb->setChecked(props.forceSurfaceDiscard);
        else if (name == "White-on-white Reversal") cb->setChecked(props.whiteOnWhiteReversal);
        else if (name == "Monochrome") {
            cb->setChecked(props.monochrome);
            // 子行可见性/色样/Scaled 回显（ViewAttributes.ts:410-418 updates push）。
            m_monochromeRow->setVisible(props.monochrome);
            QColor const mono = TbgrToQColor(vp->GetView()->GetDisplayStyle().getMonochromeColor());
            m_monochromeColorButton->setStyleSheet(
                QStringLiteral("background-color: %1; border: 1px solid #808080;").arg(mono.name()));
            // Scaled 回显须自带 blocker（循环只挡当前迭代 cb——写 m_scaledCheckbox
            // 时它在后续迭代才被挡，此处直写会触发其 toggled 写回 displayStyle）。
            QSignalBlocker const scaledBlocker(m_scaledCheckbox);
            m_scaledCheckbox->setChecked(
                dqCommon::MonochromeMode::Scaled == vp->GetView()->GetDisplayStyle().getMonochromeMode());
            continue;
        }
        else if (name == "Visible Edges") cb->setChecked(props.visibleEdges);
        else if (name == "Hidden Edges") cb->setChecked(props.hiddenEdges);
    }
}
}  // namespace Gui
