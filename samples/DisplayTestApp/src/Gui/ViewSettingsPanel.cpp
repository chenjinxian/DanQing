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

    // ── Edge Display 分区（ViewAttributes.ts:835-1008——M-O(4) P6）──
    // Transparency Threshold slider（:852-862——0.0-1.0 step 0.05）。
    {
        auto* tt = new QSlider(Qt::Horizontal, this);
        tt->setObjectName(QStringLiteral("TransparencyThreshold"));
        tt->setRange(0, 20);  // 0.0-1.0 step 0.05 ×100
        tt->setValue(20);     // 缺省 1.0
        connect(tt, &QSlider::valueChanged, this, [this, tt](int value) {
            double const t = value / 20.0;
            dqCommon::HiddenLineSettingsProps props;
            props.transThreshold = t;
            overrideEdgeSettings(props);
        });
        m_transThreshold = tt;
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(new QLabel(QStringLiteral("Transparency Threshold"), row));
        rl->addWidget(tt);
        layout->addWidget(row);
    }
    // Smooth Polyface Edges 复选（:865-869——tileAdmin.edgeOptions.smooth）。
    {
        auto* cb = new QCheckBox(QStringLiteral("Smooth Polyface Edges"), this);
        cb->setObjectName(QStringLiteral("SmoothEdges"));
        connect(cb, &QCheckBox::toggled, this,
                [this](bool on) { setSmoothPolyfaceEdges(on); });
        m_smoothEdges = cb;
        layout->addWidget(cb);
    }
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
        m_visPattern->addItem(QStringLiteral("Invalid"));  // LinePixels::Invalid 显示名
        m_visPattern->addItem(QStringLiteral("Solid"));
        m_visPattern->addItem(QStringLiteral("Code1"));
        m_visPattern->addItem(QStringLiteral("HiddenLine"));
        connect(m_visPattern, &QComboBox::currentIndexChanged, this,
                [this](int index) {
                    auto* vp = activeViewport();
                    if (!vp || !vp->GetView())
                        return;
                    // index → LinePixels（:980 parseInt——值序）。
                    dqCommon::LinePixels const pix[] = {
                        dqCommon::LinePixels::Invalid,
                        dqCommon::LinePixels::Solid,
                        dqCommon::LinePixels::Code1,
                        dqCommon::LinePixels::HiddenLine,
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
        m_hidPattern->addItem(QStringLiteral("Invalid"));
        m_hidPattern->addItem(QStringLiteral("Solid"));
        m_hidPattern->addItem(QStringLiteral("Code1"));
        m_hidPattern->addItem(QStringLiteral("HiddenLine"));
        connect(m_hidPattern, &QComboBox::currentIndexChanged, this,
                [this](int index) {
                    auto* vp = activeViewport();
                    if (!vp || !vp->GetView())
                        return;
                    dqCommon::LinePixels const pix[] = {
                        dqCommon::LinePixels::Invalid,
                        dqCommon::LinePixels::Solid,
                        dqCommon::LinePixels::Code1,
                        dqCommon::LinePixels::HiddenLine,
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

    // ── Environment 分区（EnvironmentEditor.ts:69-329——M-O(4) P7）──
    {
        // Sky Box / Ground Plane 复选（:306-329 addEnvAttribute——withDisplay）。
        auto* skyCb = new QCheckBox(QStringLiteral("Sky Box"), this);
        skyCb->setObjectName(QStringLiteral("SkyBox"));
        connect(skyCb, &QCheckBox::toggled, this,
                [this](bool on) { setEnvironmentDisplay(/*sky=*/true, on); });
        layout->addWidget(skyCb);
        auto* groundCb = new QCheckBox(QStringLiteral("Ground Plane"), this);
        groundCb->setObjectName(QStringLiteral("GroundPlane"));
        connect(groundCb, &QCheckBox::toggled, this,
                [this](bool on) { setEnvironmentDisplay(/*sky=*/false, on); });
        layout->addWidget(groundCb);

        // Background Color（:69-80——displayStyle.backgroundColor + sync）。
        {
            auto* row = new QWidget(this);
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
                vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
            });
            rl->addWidget(btn);
            layout->addWidget(row);
        }

        // 2/4 色 radio（:99-117——twoColor 位；2 色态隐 sky/ground 色与
        // 双 exponent[参考 :108-113 显隐门——恒显简化，位值语义不变]）。
        auto* colorMode = new QWidget(this);
        auto* cml = new QHBoxLayout(colorMode);
        cml->setContentsMargins(0, 0, 0, 0);
        auto* two = new QRadioButton(QStringLiteral("2 Colors"), colorMode);
        two->setObjectName(QStringLiteral("SkyTwoColors"));
        auto* four = new QRadioButton(QStringLiteral("4 Colors"), colorMode);
        four->setObjectName(QStringLiteral("SkyFourColors"));
        four->setChecked(true);  // 缺省 twoColor=false
        connect(two, &QRadioButton::toggled, this, [this](bool on) {
            if (!on)
                return;
            dqCommon::SkyBoxProps env;
            env.twoColor = true;
            updateSkyEnvironment(env);
        });
        connect(four, &QRadioButton::toggled, this, [this](bool on) {
            if (!on)
                return;
            dqCommon::SkyBoxProps env;
            env.twoColor = false;
            updateSkyEnvironment(env);
        });
        cml->addWidget(two);
        cml->addWidget(four);
        layout->addWidget(colorMode);

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
            auto* row = new QWidget(this);
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
            layout->addWidget(row);
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
            auto* row = new QWidget(this);
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
            layout->addWidget(row);
        }

        // Reset（:296-304——Environment.defaults().withDisplay({sky:true})）。
        auto* resetBtn = new QPushButton(QStringLiteral("Reset"), this);
        resetBtn->setObjectName(QStringLiteral("EnvReset"));
        connect(resetBtn, &QPushButton::clicked, this,
                [this]() { resetEnvironment(); });
        layout->addWidget(resetBtn);
    }

    // 置灰分区标注（DTA 面板的其余分区：BackgroundMap/AO/Thematic）。
    const char* disabledSections[] = {
        "Background Map", "Ambient Occlusion", "Thematic Display",
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
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
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
