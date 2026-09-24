// Ported from: itwinjs-core display-test-app ViewAttributes.ts:302-327（View Flags
// 组 + Camera + Monochrome + renderMode 下拉）+ Viewer.ts:327-335（下拉入口）。
#include "ViewSettingsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <dqApp/Application.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqCommon/ViewFlags.h>

namespace Gui {
namespace {

dqApp::Viewport* activeViewport()
{
    return dqApp::Application::Get().GetViewManager().GetActiveViewport();
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

    // Monochrome（ViewAttributes.ts addMonochrome:386-419——monochrome viewFlag 位）。
    // TODO: Monochrome color input + "Scaled" checkbox not ported — see ViewAttributes.ts:386-419 (addMonochrome).
    auto* mono = new QCheckBox(QStringLiteral("Monochrome"), this);
    mono->setObjectName(QStringLiteral("Monochrome"));
    connect(mono, &QCheckBox::toggled, this, [this](bool on) {
        applyFlags([on](dqCommon::ViewFlagsProperties& p) { p.monochrome = on; });
    });
    layout->addWidget(mono);

    // 置灰分区标注（DTA 面板的其余分区：Environment/BackgroundMap/Edges/AO/Thematic）。
    const char* disabledSections[] = {
        "Environment editor", "Background Map", "Edge Display",
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
        else if (name == "Monochrome") cb->setChecked(props.monochrome);
    }
}
}  // namespace Gui
