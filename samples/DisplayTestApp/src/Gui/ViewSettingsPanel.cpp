// Ported from: itwinjs-core display-test-app ViewAttributes.ts:302-327（View Flags
// 组 + Camera + Monochrome + renderMode 下拉）+ Viewer.ts:327-335（下拉入口）。
#include "ViewSettingsPanel.h"

#include "RenderingStyles.h"  // M-Q Q-c：Rendering Style 14 预设下拉
#include "ThematicDisplayEditor.h"  // M-S S-g：Thematic Display 编辑区

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QRadioButton>
#include <QSlider>
#include <cmath>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include <dqApp/Application.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqCommon/ViewFlags.h>
#include <dqRender/tile/TileAdmin.h>  // M-O(4) P6：edgeOptions 权威源

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

// DTA renderMode 下拉（ViewAttributes.ts addRenderMode :421-437——恰 4 entries；
// 参考 RenderMode 枚举即 4 值[ViewFlags.ts:18-43]，M-O(4) P4 勘误结案）。
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

    // Display Style 下拉（ViewAttributes.ts:1026-1066 populate——M-O(4) P5）。
    // EQUIVALENCE（§11.10）：参考经 ECSQL queryProps 枚举 iModel 的
    // DisplayStyle3dState 元素表（:1032-1033）；DanQing dump 数据面无该表
    // （imodel.json 不采集 displayStyle 元素）——entries = 当前样式单条
    // （切换通道已立：选中 → ViewState::SetDisplayStyle + InvalidateScene，
    // 参考 :1054-1059 的 handler 语义）；采集面落地时扩枚举。
    auto* dsCombo = new QComboBox(this);
    dsCombo->setObjectName(QStringLiteral("DisplayStyle"));
    dsCombo->addItem(QStringLiteral("Current"));
    connect(dsCombo, &QComboBox::currentTextChanged, this, [](QString const&) {
        // 单条目域内无切换面（选中即当前样式——no-op；通道面在引擎锁覆盖）。
    });
    layout->addWidget(dsCombo);

    // Render Mode 下拉（真）。序 = 参考 ViewAttributes.ts:275-276 构造序
    // （addRenderMode 先于 addRenderingStyles——2026-10-07 审计 V-1：原颠倒）。
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

    // Rendering Style 下拉（ViewAttributes.ts addRenderingStyles:261-281——
    // "Rendering Style: " 14 项、value=index、handler=applyRenderingStyle；
    // 3d only 显隐——DanQing 面板即 3d 视口场景，门随视图态）。
    auto* rsCombo = new QComboBox(this);
    rsCombo->setObjectName(QStringLiteral("RenderingStyle"));
    for (RenderingStyle const& style : renderingStyles())
        rsCombo->addItem(QString::fromStdString(style.name));
    connect(rsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [](int index) {
                auto* vp = activeViewport();
                if (!vp || index < 0)
                    return;
                applyRenderingStyle(*vp, static_cast<size_t>(index));
            });
    layout->addWidget(rsCombo);

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

    // Monochrome（ViewAttributes.ts addMonochrome:386-419——位开关 + Color
    // 取色 + "Scaled" 复选）。行内布局 = 参考 :387-407（Color/Scaled 与开关
    // 同行右浮——2026-10-07 审计 V-11：原独立行在开关之上）。
    m_monochromeRow = new QWidget(this);
    m_monochromeRow->setObjectName(QStringLiteral("MonochromeRow"));
    auto* monoLayout = new QHBoxLayout(m_monochromeRow);
    monoLayout->setContentsMargins(0, 0, 0, 0);
    auto* mono = new QCheckBox(QStringLiteral("Monochrome"), m_monochromeRow);
    mono->setObjectName(QStringLiteral("Monochrome"));
    connect(mono, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.monochrome = on; });
        // 子控件可见性随位（ViewAttributes.ts:410-418 updates push）。
        m_monochromeColorButton->setVisible(on);
        m_scaledCheckbox->setVisible(on);
    });
    auto* colorLabel = new QLabel(QStringLiteral("Color"), m_monochromeRow);
    colorLabel->setObjectName(QStringLiteral("MonochromeColorLabel"));
    m_monochromeColorButton = new QPushButton(m_monochromeRow);
    m_monochromeColorButton->setObjectName(QStringLiteral("MonochromeColor"));
    m_monochromeColorButton->setFixedWidth(40);
    m_monochromeColorButton->setVisible(false);   // 默认关（syncFromViewport 按位回显）
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
    m_scaledCheckbox->setVisible(false);
    connect(m_scaledCheckbox, &QCheckBox::toggled, this, [](bool on) {
        // ViewAttributes.ts:400-402 — monochromeMode = Scaled : Flat。
        auto* vp = activeViewport();
        if (!vp || !vp->GetView()) return;
        vp->GetView()->GetDisplayStyle().setMonochromeMode(
            on ? dqCommon::MonochromeMode::Scaled : dqCommon::MonochromeMode::Flat);
        vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});  // 同 V-12
    });
    monoLayout->addWidget(mono);
    monoLayout->addStretch(1);
    monoLayout->addWidget(colorLabel);
    monoLayout->addWidget(m_monochromeColorButton);
    monoLayout->addWidget(m_scaledCheckbox);
    layout->addWidget(m_monochromeRow);

    // 分区内序 = 参考 :852-889：Threshold → Smooth → Visible Edges → vis
    // 编辑器 → Hidden Edges → hid 编辑器（2026-10-07 审计 V-2：原开关在前）。
    auto* visEdges = new QCheckBox(QStringLiteral("Visible Edges"), this);
    visEdges->setObjectName(QStringLiteral("Visible Edges"));
    connect(visEdges, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.visibleEdges = on; });
        // :873 —— Visible 关 → Hidden 置灰；:874/:884 —— 两编辑器随开关显隐
        //（2026-10-07 审计 V-3/V-4：原先无门控）。
        if (auto* hid = findChild<QCheckBox*>("Hidden Edges"))
            hid->setEnabled(on);
        if (auto* ed = findChild<QWidget*>("VisibleEdgeEditor"))
            ed->setVisible(on);
    });
    layout->addWidget(visEdges);
    auto* hidEdges = new QCheckBox(QStringLiteral("Hidden Edges"), this);
    hidEdges->setObjectName(QStringLiteral("Hidden Edges"));
    connect(hidEdges, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.hiddenEdges = on; });
        // :884/:907 —— hidden 编辑器随开关显隐（审计 V-4）。
        if (auto* ed = findChild<QWidget*>("HiddenEdgeEditor"))
            ed->setVisible(on);
    });
    layout->addWidget(hidEdges);

    // Visible 边编辑器（:879 + :914-1008 addHiddenLineEditor(false)——Color
    // [visible 专属 :927-949]/Weight 1-31 [:951-976]/Pattern [:978-981]）。
    {
        auto* visBox = new QWidget(this);
        visBox->setObjectName(QStringLiteral("VisibleEdgeEditor"));
        auto* vl = new QVBoxLayout(visBox);
        vl->setContentsMargins(10, 0, 0, 0);
        auto* colorRow = new QWidget(visBox);
        auto* cl = new QHBoxLayout(colorRow);
        cl->setContentsMargins(0, 0, 0, 0);
        m_visColorCb = new QCheckBox(QStringLiteral("Color"), colorRow);
        m_visColorButton = new QPushButton(colorRow);
        m_visColorButton->setObjectName(QStringLiteral("VisibleEdgeColor"));
        m_visColorButton->setFixedWidth(40);
        connect(m_visColorButton, &QPushButton::clicked, this, [this]() {
            auto* vp = activeViewport();
            uint32_t const initialTbgr = (vp && vp->GetView())
                ? vp->GetView()->GetDisplayStyle()
                      .getSettings()
                      .getHiddenLineSettings()
                      .visible.color.value_or(dqCommon::ColorDef::white)
                      .getTbgr()
                : 0xFFFFFFFFu;
            QColor const initial = TbgrToQColor(initialTbgr);
            QColor const picked = QColorDialog::getColor(initial, this);
            if (!picked.isValid())
                return;
            // 参考 :945 handler——overrideColor(getSettings() 现值 + 新色)。
            auto* vp2 = activeViewport();
            if (!vp2 || !vp2->GetView())
                return;
            auto const& style
                = vp2->GetView()->GetDisplayStyle().getSettings()
                      .getHiddenLineSettings().visible;
            dqCommon::HiddenLineSettingsProps props;
            dqCommon::HiddenLineStyleProps sp = style.toJSON();
            sp.color = QColorToTbgr(picked);
            sp.ovrColor = true;
            props.visible = sp;
            overrideEdgeSettings(props);
        });
        connect(m_visColorCb, &QCheckBox::toggled, this, [this](bool on) {
            m_visColorButton->setEnabled(on);
            if (!on) {
                // 参考 :948——overrideColor(undefined) 清覆写。
                auto* vp = activeViewport();
                if (!vp || !vp->GetView())
                    return;
                auto const& style = vp->GetView()->GetDisplayStyle()
                                        .getSettings()
                                        .getHiddenLineSettings().visible;
                dqCommon::HiddenLineSettingsProps props;
                dqCommon::HiddenLineStyleProps sp = style.toJSON();
                sp.color = std::nullopt;
                sp.ovrColor = false;
                props.visible = sp;
                overrideEdgeSettings(props);
            }
        });
        cl->addWidget(m_visColorCb);
        cl->addWidget(m_visColorButton);
        vl->addWidget(colorRow);

        auto* widthRow = new QWidget(visBox);
        auto* wl = new QHBoxLayout(widthRow);
        wl->setContentsMargins(0, 0, 0, 0);
        m_visWidthCb = new QCheckBox(QStringLiteral("Weight"), widthRow);
        m_visWidth = new QSpinBox(widthRow);
        m_visWidth->setObjectName(QStringLiteral("VisibleEdgeWeight"));
        m_visWidth->setRange(1, 31);
        m_visWidth->setValue(1);
        connect(m_visWidth, &QSpinBox::valueChanged, this, [this](int value) {
            auto* vp = activeViewport();
            if (!vp || !vp->GetView())
                return;
            auto const& style = vp->GetView()->GetDisplayStyle()
                                    .getSettings()
                                    .getHiddenLineSettings().visible;
            dqCommon::HiddenLineSettingsProps props;
            dqCommon::HiddenLineStyleProps sp = style.toJSON();
            sp.width = value;
            props.visible = sp;
            overrideEdgeSettings(props);
        });
        connect(m_visWidthCb, &QCheckBox::toggled, this, [this](bool on) {
            m_visWidth->setEnabled(on);
            if (!on) {
                // 参考 :976——overrideWidth(undefined) 清覆写。
                auto* vp = activeViewport();
                if (!vp || !vp->GetView())
                    return;
                auto const& style = vp->GetView()->GetDisplayStyle()
                                        .getSettings()
                                        .getHiddenLineSettings().visible;
                dqCommon::HiddenLineSettingsProps props;
                dqCommon::HiddenLineStyleProps sp = style.toJSON();
                sp.width = std::nullopt;
                props.visible = sp;
                overrideEdgeSettings(props);
            } else {
                // 勾选时取 spin 当前值（参考 :976 widthCb.checked 分支）。
                m_visWidth->valueChanged(m_visWidth->value());
            }
        });
        wl->addWidget(m_visWidthCb);
        wl->addWidget(m_visWidth);
        vl->addWidget(widthRow);

        m_visPattern = new QComboBox(visBox);
        m_visPattern->setObjectName(QStringLiteral("VisibleEdgePattern"));
        // 11 项序 = FeatureOverrides.addStyle :298-310（2026-10-07 审计 V-5：
        // 原 4 项自造序）。index → LinePixels 同序映射。
        m_visPattern->addItem(QStringLiteral("Not overridden"));
        m_visPattern->addItem(QStringLiteral("Solid"));
        m_visPattern->addItem(QStringLiteral("Hidden Line"));
        m_visPattern->addItem(QStringLiteral("Invisible"));
        m_visPattern->addItem(QStringLiteral("Code1"));
        m_visPattern->addItem(QStringLiteral("Code2"));
        m_visPattern->addItem(QStringLiteral("Code3"));
        m_visPattern->addItem(QStringLiteral("Code4"));
        m_visPattern->addItem(QStringLiteral("Code5"));
        m_visPattern->addItem(QStringLiteral("Code6"));
        m_visPattern->addItem(QStringLiteral("Code7"));
        connect(m_visPattern, &QComboBox::currentIndexChanged, this,
                [this](int index) {
                    auto* vp = activeViewport();
                    if (!vp || !vp->GetView())
                        return;
                    // index → LinePixels（:980 parseInt——值序）。
                    dqCommon::LinePixels const pix[] = {
                        dqCommon::LinePixels::Invalid,
                        dqCommon::LinePixels::Solid,
                        dqCommon::LinePixels::HiddenLine,
                        dqCommon::LinePixels::Invisible,
                        dqCommon::LinePixels::Code1,
                        dqCommon::LinePixels::Code2,
                        dqCommon::LinePixels::Code3,
                        dqCommon::LinePixels::Code4,
                        dqCommon::LinePixels::Code5,
                        dqCommon::LinePixels::Code6,
                        dqCommon::LinePixels::Code7,
                    };
                    auto const& style = vp->GetView()->GetDisplayStyle()
                                            .getSettings()
                                            .getHiddenLineSettings().visible;
                    dqCommon::HiddenLineSettingsProps props;
                    dqCommon::HiddenLineStyleProps sp = style.toJSON();
                    sp.pattern = pix[static_cast<size_t>(index)];
                    props.visible = sp;
                    overrideEdgeSettings(props);
                });
        vl->addWidget(m_visPattern);
        layout->addWidget(visBox);
    }
    // Hidden 边编辑器（:888 + addHiddenLineEditor(true)——无 Color 段
    // [:927 forHiddenEdges 专属门]）。
    {
        auto* hidBox = new QWidget(this);
        hidBox->setObjectName(QStringLiteral("HiddenEdgeEditor"));
        auto* hl2 = new QVBoxLayout(hidBox);
        hl2->setContentsMargins(10, 0, 0, 0);
        auto* widthRow = new QWidget(hidBox);
        auto* wl = new QHBoxLayout(widthRow);
        wl->setContentsMargins(0, 0, 0, 0);
        m_hidWidthCb = new QCheckBox(QStringLiteral("Weight"), widthRow);
        m_hidWidth = new QSpinBox(widthRow);
        m_hidWidth->setObjectName(QStringLiteral("HiddenEdgeWeight"));
        m_hidWidth->setRange(1, 31);
        m_hidWidth->setValue(1);
        connect(m_hidWidth, &QSpinBox::valueChanged, this, [this](int value) {
            auto* vp = activeViewport();
            if (!vp || !vp->GetView())
                return;
            auto const& style = vp->GetView()->GetDisplayStyle()
                                    .getSettings()
                                    .getHiddenLineSettings().hidden;
            dqCommon::HiddenLineSettingsProps props;
            dqCommon::HiddenLineStyleProps sp = style.toJSON();
            sp.width = value;
            props.hidden = sp;
            overrideEdgeSettings(props);
        });
        connect(m_hidWidthCb, &QCheckBox::toggled, this, [this](bool on) {
            m_hidWidth->setEnabled(on);
            auto* vp = activeViewport();
            if (!vp || !vp->GetView())
                return;
            auto const& style = vp->GetView()->GetDisplayStyle()
                                    .getSettings()
                                    .getHiddenLineSettings().hidden;
            dqCommon::HiddenLineSettingsProps props;
            dqCommon::HiddenLineStyleProps sp = style.toJSON();
            sp.width = on ? std::optional<int>(m_hidWidth->value())
                          : std::nullopt;
            props.hidden = sp;
            overrideEdgeSettings(props);
        });
        wl->addWidget(m_hidWidthCb);
        wl->addWidget(m_hidWidth);
        hl2->addWidget(widthRow);

        m_hidPattern = new QComboBox(hidBox);
        m_hidPattern->setObjectName(QStringLiteral("HiddenEdgePattern"));
        // 11 项序 = FeatureOverrides.addStyle :298-310（审计 V-5）。
        m_hidPattern->addItem(QStringLiteral("Not overridden"));
        m_hidPattern->addItem(QStringLiteral("Solid"));
        m_hidPattern->addItem(QStringLiteral("Hidden Line"));
        m_hidPattern->addItem(QStringLiteral("Invisible"));
        m_hidPattern->addItem(QStringLiteral("Code1"));
        m_hidPattern->addItem(QStringLiteral("Code2"));
        m_hidPattern->addItem(QStringLiteral("Code3"));
        m_hidPattern->addItem(QStringLiteral("Code4"));
        m_hidPattern->addItem(QStringLiteral("Code5"));
        m_hidPattern->addItem(QStringLiteral("Code6"));
        m_hidPattern->addItem(QStringLiteral("Code7"));
        connect(m_hidPattern, &QComboBox::currentIndexChanged, this,
                [this](int index) {
                    auto* vp = activeViewport();
                    if (!vp || !vp->GetView())
                        return;
                    dqCommon::LinePixels const pix[] = {
                        dqCommon::LinePixels::Invalid,
                        dqCommon::LinePixels::Solid,
                        dqCommon::LinePixels::HiddenLine,
                        dqCommon::LinePixels::Invisible,
                        dqCommon::LinePixels::Code1,
                        dqCommon::LinePixels::Code2,
                        dqCommon::LinePixels::Code3,
                        dqCommon::LinePixels::Code4,
                        dqCommon::LinePixels::Code5,
                        dqCommon::LinePixels::Code6,
                        dqCommon::LinePixels::Code7,
                    };
                    auto const& style = vp->GetView()->GetDisplayStyle()
                                            .getSettings()
                                            .getHiddenLineSettings().hidden;
                    dqCommon::HiddenLineSettingsProps props;
                    dqCommon::HiddenLineStyleProps sp = style.toJSON();
                    sp.pattern = pix[static_cast<size_t>(index)];
                    props.hidden = sp;
                    overrideEdgeSettings(props);
                });
        hl2->addWidget(m_hidPattern);
        layout->addWidget(hidBox);
    }

    // ── Environment 分区（EnvironmentEditor.ts:69-329——M-O(4) P7。
    // 2026-10-07 审计 V-8/V-9/V-10 对齐：序 = SkyBox → Background Color →
    // 渐变组[2/4 radio → 四色 → 双指数 → Export/Reset] → Ground Plane（:220
    // 末位）；showSkyboxControls 门（:88-93——skybox 开 → 渐变组显 + 背景
    // 色隐 / 关 → 反相）；2 Colors 隐 Sky/Ground 色钮 + 双指数（:108-113，
    // Zenith/Nadir 保留）；Export 按钮（:193-205）。──
    {
        // 渐变组容器（eeDiv——displaySky 门控整体显隐）。
        auto* gradGroup = new QWidget(this);
        gradGroup->setObjectName(QStringLiteral("EnvGradientGroup"));
        auto* gl = new QVBoxLayout(gradGroup);
        gl->setContentsMargins(0, 0, 0, 0);

        // showSkyboxControls（:88-93——skybox 开 → 渐变组显 + 背景色隐）。
        auto showSkyboxControls = [this, gradGroup](bool enabled) {
            gradGroup->setVisible(enabled);
            if (auto* bg = findChild<QWidget*>("EnvBackgroundColorRow"))
                bg->setVisible(!enabled);
        };

        auto* skyCb = new QCheckBox(QStringLiteral("Sky Box"), this);
        skyCb->setObjectName(QStringLiteral("SkyBox"));
        connect(skyCb, &QCheckBox::toggled, this,
                [this, showSkyboxControls](bool on) {
                    setEnvironmentDisplay(/*sky=*/true, on);
                    showSkyboxControls(on);
                });
        layout->addWidget(skyCb);

        // Background Color（:69-80——displayStyle.backgroundColor + sync）。
        {
            auto* row = new QWidget(this);
            row->setObjectName(QStringLiteral("EnvBackgroundColorRow"));
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->addWidget(new QLabel(QStringLiteral("Background Color"), row));
            auto* btn = new QPushButton(row);
            btn->setObjectName(QStringLiteral("EnvBackgroundColor"));
            btn->setFixedWidth(40);
            connect(btn, &QPushButton::clicked, this, [this]() {
                auto* vp = activeViewport();
                uint32_t const initialTbgr = (vp && vp->GetView())
                    ? vp->GetView()->GetDisplayStyle().getBackgroundColor()
                    : 0xFF000000u;
                QColor const picked = QColorDialog::getColor(
                    TbgrToQColor(initialTbgr), this);
                if (!picked.isValid() || !vp || !vp->GetView())
                    return;
                vp->GetView()->GetDisplayStyle().setBackgroundColor(
                    QColorToTbgr(picked));
                // :77-78 handler → sync()（:331-333 synchWithView 无参——入
                // 撤销栈；审计 V-7：原 noSaveInUndo=true）。
                vp->synchWithView();
            });
            rl->addWidget(btn);
            layout->addWidget(row);
        }

        // 2/4 色 radio（:99-117——twoColor 位 + 相关控件显隐门）。
        {
            auto* colorMode = new QWidget(gradGroup);
            auto* cml = new QHBoxLayout(colorMode);
            cml->setContentsMargins(0, 0, 0, 0);
            auto* two = new QRadioButton(QStringLiteral("2 Colors"), colorMode);
            two->setObjectName(QStringLiteral("SkyTwoColors"));
            auto* four = new QRadioButton(QStringLiteral("4 Colors"), colorMode);
            four->setObjectName(QStringLiteral("SkyFourColors"));
            four->setChecked(true);  // 缺省 twoColor=false
            auto applyTwoColorVisibility = [this](bool twoColors) {
                // :108-113 —— 2 色隐 Sky/Ground 色钮 + 双指数（Zenith/Nadir 保留）。
                for (char const* name : { "EnvSkyColorRow", "EnvGroundColorRow",
                                          "EnvSkyExponentRow", "EnvGroundExponentRow" })
                    if (auto* w = findChild<QWidget*>(name))
                        w->setVisible(!twoColors);
            };
            connect(two, &QRadioButton::toggled, this,
                    [this, applyTwoColorVisibility](bool on) {
                if (!on)
                    return;
                dqCommon::SkyBoxProps env;
                env.twoColor = true;
                updateSkyEnvironment(env);
                applyTwoColorVisibility(true);
            });
            connect(four, &QRadioButton::toggled, this,
                    [this, applyTwoColorVisibility](bool on) {
                if (!on)
                    return;
                dqCommon::SkyBoxProps env;
                env.twoColor = false;
                updateSkyEnvironment(env);
                applyTwoColorVisibility(false);
            });
            cml->addWidget(two);
            cml->addWidget(four);
            gl->addWidget(colorMode);
        }

        // 四色（:124-157——Zenith/Nadir 行 + Sky/Ground 行）。
        struct SkyColorEntry {
            char const* label;
            char const* objectName;
            std::optional<uint32_t> dqCommon::SkyBoxProps::*field;
        };
        SkyColorEntry const skyColors[] = {
            {"Zenith Color", "EnvZenithColor", &dqCommon::SkyBoxProps::zenithColor},
            {"Nadir Color", "EnvNadirColor", &dqCommon::SkyBoxProps::nadirColor},
            {"Sky Color", "EnvSkyColor", &dqCommon::SkyBoxProps::skyColor},
            {"Ground Color", "EnvGroundColor", &dqCommon::SkyBoxProps::groundColor},
        };
        for (auto const& e : skyColors) {
            auto* row = new QWidget(gradGroup);
            // 行 objectName（Sky/Ground 两行受 2 色门）。
            row->setObjectName(QString::fromLatin1(e.objectName) + QStringLiteral("Row"));
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->addWidget(new QLabel(QString::fromLatin1(e.label), row));
            auto* btn = new QPushButton(row);
            btn->setObjectName(QString::fromLatin1(e.objectName));
            btn->setFixedWidth(40);
            connect(btn, &QPushButton::clicked, this,
                    [this, field = e.field, e]() {
                        QColor const picked = QColorDialog::getColor(
                            QColor(255, 255, 255), this);
                        if (!picked.isValid())
                            return;
                        dqCommon::SkyBoxProps env;
                        (env.*field) = QColorToTbgr(picked);
                        updateSkyEnvironment(env);
                    });
            rl->addWidget(btn);
            gl->addWidget(row);
        }

        // Sky/Ground Exponent 双 slider（:159-181——0-20 step 0.25 ×4）。
        struct SkyExpEntry {
            char const* label;
            char const* objectName;
            std::optional<double> dqCommon::SkyBoxProps::*field;
        };
        SkyExpEntry const skyExps[] = {
            {"Sky Exponent", "EnvSkyExponent", &dqCommon::SkyBoxProps::skyExponent},
            {"Ground Exponent", "EnvGroundExponent",
             &dqCommon::SkyBoxProps::groundExponent},
        };
        for (auto const& e : skyExps) {
            auto* row = new QWidget(gradGroup);
            row->setObjectName(QString::fromLatin1(e.objectName) + QStringLiteral("Row"));
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->addWidget(new QLabel(QString::fromLatin1(e.label), row));
            auto* sl = new QSlider(Qt::Horizontal, row);
            sl->setObjectName(QString::fromLatin1(e.objectName));
            sl->setRange(0, 80);  // 0-20 step 0.25 ×4
            sl->setValue(16);     // 缺省 4.0
            connect(sl, &QSlider::valueChanged, this,
                    [this, field = e.field](int value) {
                        dqCommon::SkyBoxProps env;
                        (env.*field) = value / 4.0;
                        updateSkyEnvironment(env);
                    });
            rl->addWidget(sl);
            gl->addWidget(row);
        }

        // Export / Reset（:193-205 Export 在前；:296-304 Reset）。
        {
            auto* btnRow = new QWidget(gradGroup);
            auto* bl = new QHBoxLayout(btnRow);
            bl->setContentsMargins(0, 0, 0, 0);
            auto* exportBtn = new QPushButton(QStringLiteral("Export"), btnRow);
            exportBtn->setObjectName(QStringLiteral("EnvExport"));
            connect(exportBtn, &QPushButton::clicked, this, [this]() {
                // :193-205 —— alert(JSON.stringify(gradient))；Qt 对应物 =
                // QMessageBox 模态（浏览器 alert 的最近形态）。
                auto* vp = activeViewport();
                if (!vp || !vp->GetView() || !vp->GetView()->AsViewState3d())
                    return;
                dqCommon::SkyBoxProps const props = vp->GetView()->AsViewState3d()
                                       ->GetDisplayStyle()
                                       .getEnvironment()
                                       .sky.gradient.toJSON();
                QString text = QStringLiteral(
                    "{ groundColor: %1, nadirColor: %2, skyColor: %3, zenithColor: %4 }")
                    .arg(props.groundColor.value_or(0))
                    .arg(props.nadirColor.value_or(0))
                    .arg(props.skyColor.value_or(0))
                    .arg(props.zenithColor.value_or(0));
                QMessageBox::information(this, QStringLiteral("Skybox gradient"), text);
            });
            auto* resetBtn = new QPushButton(QStringLiteral("Reset"), btnRow);
            resetBtn->setObjectName(QStringLiteral("EnvReset"));
            connect(resetBtn, &QPushButton::clicked, this,
                    [this]() { resetEnvironment(); });
            bl->addWidget(exportBtn);
            bl->addWidget(resetBtn);
            gl->addWidget(btnRow);
        }

        layout->addWidget(gradGroup);
        // 3d 态初始：displaySky 缺省 true → 渐变组显 + 背景色隐（:82/:88-93
        // ——is3d 时背景色 display:none；syncFromViewport 按现值刷新）。
        showSkyboxControls(true);

        // Ground Plane（:220——参考分区末位）。
        auto* groundCb = new QCheckBox(QStringLiteral("Ground Plane"), this);
        groundCb->setObjectName(QStringLiteral("GroundPlane"));
        connect(groundCb, &QCheckBox::toggled, this,
                [this](bool on) { setEnvironmentDisplay(/*sky=*/false, on); });
        layout->addWidget(groundCb);
    }

    // ── Thematic Display 编辑区（M-S S-g——ThematicDisplay.ts:37-777 全量；
    // DTA 内序最末[ViewAttributes.ts:327 addThematicDisplay 在 AO 之后]）。
    m_thematicEditor = new ThematicDisplayEditor(this);
    layout->addWidget(m_thematicEditor);

    // 置灰分区标注（DTA 面板的其余分区：BackgroundMap/AO；Thematic 已激活[S-g]）。
    const char* disabledSections[] = {
        "Background Map", "Ambient Occlusion",
    };
    for (auto* s : disabledSections) {
        auto* l = new QLabel(QString::fromLatin1(s) + QStringLiteral(" (not yet implemented)"), this);
        l->setEnabled(false);
        layout->addWidget(l);
    }
}

// 11 项 Pattern 序（FeatureOverrides.addStyle :298-310）的值→索引（审计 V-6）。
static int linePixelsToPatternIndex(std::optional<dqCommon::LinePixels> pattern)
{
    if (!pattern.has_value())
        return 0;   // Not overridden
    dqCommon::LinePixels const kOrder[] = {
        dqCommon::LinePixels::Invalid,
        dqCommon::LinePixels::Solid,
        dqCommon::LinePixels::HiddenLine,
        dqCommon::LinePixels::Invisible,
        dqCommon::LinePixels::Code1,
        dqCommon::LinePixels::Code2,
        dqCommon::LinePixels::Code3,
        dqCommon::LinePixels::Code4,
        dqCommon::LinePixels::Code5,
        dqCommon::LinePixels::Code6,
        dqCommon::LinePixels::Code7,
    };
    for (int i = 0; i < 11; ++i)
        if (kOrder[i] == *pattern)
            return i;
    return 0;
}

void ViewSettingsPanel::applyFlags(std::function<void(dqCommon::ViewFlagsProperties&)> mod)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView()) return;
    auto& style = vp->GetView()->GetDisplayStyle();
    auto props = style.getViewFlags().Properties();
    mod(props);
    style.setViewFlags(dqCommon::ViewFlags(props));
    // ViewAttributes.sync（:820-822——synchWithView({noSaveInUndo:true})，含
    // invalidateController；审计 V-12：原 SetupFromView 跳过 controller 同步——
    // 重绘由 OnViewFlagsChanged 监听兜底，此处对齐参考一步到位）。
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
}

// Ported from: ViewAttributes.ts:829-833 overrideEdgeSettings（M-O(4) P6）。
void ViewSettingsPanel::overrideEdgeSettings(
    dqCommon::HiddenLineSettingsProps const& props)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto* v3d = vp->GetView()->AsViewState3d();
    if (v3d == nullptr)
        return;
    auto& settings = v3d->GetDisplayStyle().getSettings();
    settings.setHiddenLineSettings(settings.getHiddenLineSettings().override(props));
    // sync（:820-822——vp.synchWithView({noSaveInUndo:true})）。
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
}

// Ported from: ViewAttributes.ts:865-869 Smooth Polyface Edges（M-O(4) P6——
// tileAdmin.edgeOptions.smooth + invalidateScene + sync）。
void ViewSettingsPanel::setSmoothPolyfaceEdges(bool enabled)
{
    auto& admin = dqRender::TileAdmin::instance();
    dqRender::EdgeOptions options = admin.edgeOptions();
    options.smooth = enabled;
    admin.setEdgeOptions(options);
    auto* vp = activeViewport();
    if (!vp)
        return;
    vp->InvalidateScene();
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
}

// Ported from: EnvironmentEditor.ts:258-271 updateEnvironment（M-O(4) P7——
// 渐变字段段合并 {...current.sky.toJSON(), ...newEnv} → SkyBox.createGradient
// → environment 替换 + sync）。
void ViewSettingsPanel::updateSkyEnvironment(dqCommon::SkyBoxProps const& newEnv)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto* v3d = vp->GetView()->AsViewState3d();
    if (v3d == nullptr)
        return;
    // 段合并（参考展开合并——缺席字段保持现值）。
    dqCommon::Environment env = v3d->GetDisplayStyle().getEnvironment().clone();
    dqCommon::SkyBoxProps merged = env.sky.gradient.toJSON();
    if (newEnv.twoColor.has_value())
        merged.twoColor = newEnv.twoColor;
    if (newEnv.skyColor.has_value())
        merged.skyColor = newEnv.skyColor;
    if (newEnv.groundColor.has_value())
        merged.groundColor = newEnv.groundColor;
    if (newEnv.zenithColor.has_value())
        merged.zenithColor = newEnv.zenithColor;
    if (newEnv.nadirColor.has_value())
        merged.nadirColor = newEnv.nadirColor;
    if (newEnv.skyExponent.has_value())
        merged.skyExponent = newEnv.skyExponent;
    if (newEnv.groundExponent.has_value())
        merged.groundExponent = newEnv.groundExponent;
    env.sky.gradient = dqCommon::SkyGradient::fromJSON(&merged);
    v3d->GetDisplayStyle().setEnvironment(env);
    // EnvironmentEditor.sync（:331-333——synchWithView 无参 = 入撤销栈；
    // 审计 V-7：原 noSaveInUndo=true 与参考 undo 语义相反）。
    vp->synchWithView();
}

// Ported from: EnvironmentEditor.ts:296-304 resetEnvironmentEditor（M-O(4) P7
// ——Environment.defaults().withDisplay({sky: true})）。
void ViewSettingsPanel::resetEnvironment()
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto* v3d = vp->GetView()->AsViewState3d();
    if (v3d == nullptr)
        return;
    dqCommon::Environment env = dqCommon::Environment::defaults().clone();
    env.displaySky = true;  // withDisplay({ sky: true })
    v3d->GetDisplayStyle().setEnvironment(env);
    vp->synchWithView();   // 同 V-7：入撤销栈
}

// Ported from: EnvironmentEditor.ts:306-316 addEnvAttribute 的 withDisplay
// 半边（M-O(4) P7——sky/ground 显隐位）。
void ViewSettingsPanel::setEnvironmentDisplay(bool sky, bool enabled)
{
    auto* vp = activeViewport();
    if (!vp || !vp->GetView())
        return;
    auto& style = vp->GetView()->GetDisplayStyle();
    if (sky)
        style.toggleSkyBox(enabled);
    else
        style.toggleGroundPlane(enabled);
    vp->synchWithView();   // 同 V-7：入撤销栈
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
    // :393-396 sync → synchWithView({noSaveInUndo:true})（审计 V-12）。
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
            // 子控件可见性/色样/Scaled 回显（ViewAttributes.ts:410-418 updates push）。
            m_monochromeColorButton->setVisible(props.monochrome);
            m_scaledCheckbox->setVisible(props.monochrome);
            if (auto* cl = findChild<QLabel*>("MonochromeColorLabel"))
                cl->setVisible(props.monochrome);
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
        else if (name == "Sky Box" || name == "Ground Plane") {
            auto* v3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;
            if (!v3d)
                continue;
            auto const& env = v3d->GetDisplayStyle().getEnvironment();
            if (name == "Sky Box") {
                cb->setChecked(env.displaySky);
                continue;
            }
            cb->setChecked(env.displayGround);
            continue;
        }
        else if (name == "SmoothEdges") {
            cb->setChecked(dqRender::TileAdmin::instance().edgeOptions().smooth);
            continue;
        }
    }

    // ── Edge Display 回读（2026-10-07 审计 V-6：原仅回两个开关）──
    if (auto* v3d = vp->GetView()->AsViewState3d()) {
        auto const& hline = v3d->GetDisplayStyle().getSettings().getHiddenLineSettings();
        auto const hprops = hline.toJSON();
        // Threshold（缺省 1.0）。
        if (m_transThreshold) {
            QSignalBlocker b(m_transThreshold);
            double const t = hprops.transThreshold.value_or(1.0);
            m_transThreshold->setValue(static_cast<int>(std::lround(t * 20.0)));
        }
        // 开关门（V-3/V-4 的回读半边：编辑器显隐 + Hidden 置灰随 Visible）。
        bool const visOn = props.visibleEdges;
        bool const hidOn = props.hiddenEdges;
        if (auto* ed = findChild<QWidget*>("VisibleEdgeEditor"))
            ed->setVisible(visOn);
        if (auto* ed = findChild<QWidget*>("HiddenEdgeEditor"))
            ed->setVisible(hidOn);
        if (auto* hid = findChild<QCheckBox*>("Hidden Edges"))
            hid->setEnabled(visOn);
        // visible 编辑器（Color 复选+色样 / Weight 复选+值 / Pattern）。
        {
            auto const sp = hline.visible.toJSON();
            if (m_visColorCb) {
                QSignalBlocker b(m_visColorCb);
                m_visColorCb->setChecked(sp.ovrColor.value_or(false));
            }
            if (m_visColorButton) {
                m_visColorButton->setEnabled(sp.ovrColor.value_or(false));
                QColor const c = TbgrToQColor(sp.color.value_or(0xFFFFFFFFu));
                m_visColorButton->setStyleSheet(
                    QStringLiteral("background-color: %1; border: 1px solid #808080;")
                        .arg(c.name()));
            }
            if (m_visWidthCb) {
                QSignalBlocker b(m_visWidthCb);
                m_visWidthCb->setChecked(sp.width.has_value());
            }
            if (m_visWidth) {
                QSignalBlocker b(m_visWidth);
                m_visWidth->setEnabled(sp.width.has_value());
                m_visWidth->setValue(sp.width.value_or(1));
            }
            if (m_visPattern) {
                QSignalBlocker b(m_visPattern);
                m_visPattern->setCurrentIndex(
                    linePixelsToPatternIndex(sp.pattern));
            }
        }
        // hidden 编辑器（Weight / Pattern——无 Color 段）。
        {
            auto const sp = hline.hidden.toJSON();
            if (m_hidWidthCb) {
                QSignalBlocker b(m_hidWidthCb);
                m_hidWidthCb->setChecked(sp.width.has_value());
            }
            if (m_hidWidth) {
                QSignalBlocker b(m_hidWidth);
                m_hidWidth->setEnabled(sp.width.has_value());
                m_hidWidth->setValue(sp.width.value_or(1));
            }
            if (m_hidPattern) {
                QSignalBlocker b(m_hidPattern);
                m_hidPattern->setCurrentIndex(
                    linePixelsToPatternIndex(sp.pattern));
            }
        }

        // ── Environment 回读（V-6：渐变组/背景色换显 + 2 色门 + 双指数现值）──
        {
            auto const& env = v3d->GetDisplayStyle().getEnvironment();
            auto const grad = env.sky.gradient.toJSON();
            bool const skyOn = env.displaySky;
            if (auto* gg = findChild<QWidget*>("EnvGradientGroup"))
                gg->setVisible(skyOn);
            if (auto* bg = findChild<QWidget*>("EnvBackgroundColorRow"))
                bg->setVisible(!skyOn);
            bool const twoColor = grad.twoColor.value_or(false);
            if (auto* two = findChild<QRadioButton*>("SkyTwoColors"))
                two->setChecked(twoColor);
            if (auto* four = findChild<QRadioButton*>("SkyFourColors"))
                four->setChecked(!twoColor);
            for (char const* nm : { "EnvSkyColorRow", "EnvGroundColorRow",
                                    "EnvSkyExponentRow", "EnvGroundExponentRow" })
                if (auto* w = findChild<QWidget*>(nm))
                    w->setVisible(!twoColor);
            if (auto* sl = findChild<QSlider*>("EnvSkyExponent")) {
                QSignalBlocker b(sl);
                sl->setValue(static_cast<int>(std::lround(
                    grad.skyExponent.value_or(4.0) * 4.0)));
            }
            if (auto* sl = findChild<QSlider*>("EnvGroundExponent")) {
                QSignalBlocker b(sl);
                sl->setValue(static_cast<int>(std::lround(
                    grad.groundExponent.value_or(4.0) * 4.0)));
            }
        }

        // ── Thematic Display 回读（M-S S-g——ThematicDisplay.ts:654-664 的
        // _update 闭包语义：is3d 显隐门[DanQing 恒 3d] + checkbox=vf 位 +
        // 控件显隐 + 全控件回读）──
        if (m_thematicEditor) {
            bool const on = v3d->getViewFlags().thematicDisplay();
            m_thematicEditor->syncEnabledState(on);
            if (on)
                m_thematicEditor->updateThematicDisplayUI();
        }
    }
}
}  // namespace Gui
