// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — feature symbology override panel
// Ported from: itwinjs-core display-test-app FeatureOverrides.ts
//              (Provider :14-97 + Settings :99-386 + FeatureOverridesPanel
//              :388-408——Viewer.ts:405-409 工具栏下拉"Override feature
//              symbology"，作用于选择集)。
#pragma once

#include <dqCommon/FeatureSymbology.h>
#include <dqCommon/FeatureOverrides.h>

#include <QWidget>

#include <map>
#include <optional>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QPushButton;
class QColor;

namespace dqApp {
class Viewport;
}

namespace Gui {

// ---------------------------------------------------------------------------
// FeatureOverridesProvider — selection-set-driven appearance overrides.
// Ported from: itwinjs-core FeatureOverrides.ts Provider (:14-97——DTA 的
// FeatureOverrideProvider 实现；引擎注册面 = Viewport::AddFeatureOverride
// Provider [M-O(2) I10]——参考 vp.addFeatureOverrideProvider :92)。
// ---------------------------------------------------------------------------
class FeatureOverridesProvider final : public dqCommon::FeatureOverrideProvider {
public:
    explicit FeatureOverridesProvider(dqApp::Viewport* vp);
    ~FeatureOverridesProvider() override;

    // Ported from: FeatureOverrides.ts addFeatureOverrides (:21-25——逐元素
    // override + defaults 的 setDefaultOverrides)。
    void addFeatureOverrides(dqCommon::FeatureOverrides& ovrs, void* context) override;

    // Ported from: FeatureOverrides.ts overrideElements (:27-32——选择集逐
    // 元素置 appearance)。
    void overrideElements(dqCommon::FeatureAppearance const& app);

    // ← toJSON (:46-63) 的元素条目形态：{ id, fsa(JSON 字串) }；默认 override
    // 以保留 id 段 "-default-" 标记（I9 SavedView 的持久化消费面）。
    struct ElementOverride {
        std::string id;       // 元素 id（十进制）或 "-default-"
        std::string fsaJson;  // FeatureAppearanceProps 的 JSON 字串
    };
    // Ported from: FeatureOverrides.ts toJSON (:46-63——空表 + 无默认 →
    // nullopt)。
    std::optional<std::vector<ElementOverride>> toJSON() const;
    // Ported from: FeatureOverrides.ts overrideElementsByArray (:34-44——
    // SavedView recall 的恢复面)。
    void overrideElementsByArray(std::vector<ElementOverride> const& elementOvrs);

    // Ported from: FeatureOverrides.ts clear (:65-69)。
    void clear();
    // Ported from: FeatureOverrides.ts defaults setter (:71-74)。
    void setDefaults(dqCommon::FeatureAppearance const& app);

    static FeatureOverridesProvider* get(dqApp::Viewport* vp);
    static void remove(dqApp::Viewport* vp);
    // Ported from: FeatureOverrides.ts getOrCreate (:88-96)。
    static FeatureOverridesProvider* getOrCreate(dqApp::Viewport* vp);

private:
    // Ported from: FeatureOverrides.ts sync (:76——vp.setFeatureOverride
    // ProviderChanged)。
    void sync();

    std::map<uint32_t, dqCommon::FeatureAppearance> m_elementOvrs;
    std::optional<dqCommon::FeatureAppearance> m_defaultOvrs;
    dqApp::Viewport* m_vp;
};

// ---------------------------------------------------------------------------
// FeatureOverridesSettings — the Settings control set.
// Ported from: itwinjs-core FeatureOverrides.ts Settings (:99-386——
// Color/Line Color + apply-to-lines 联动、Transp/Line Transp 0-255 +
// View-dependent、Style 下拉（LinePixels 11 项）、Weight 1-31、
// Ignore Material/Non-locatable/Emphasized、Apply/Default/Clear 三钮)。
// 控件状态 ↔ FeatureAppearanceProps 的 updateAppearance 往返
// （:178-190——props 字段级改写后 fromJSON 重建）。
// ---------------------------------------------------------------------------
class FeatureOverridesSettings final : public QWidget {
    Q_OBJECT
public:
    FeatureOverridesSettings(dqApp::Viewport* vp, QWidget* parent);

    // 当前控件面聚合的 appearance（updateAppearance 的聚合等价——Apply 前
    // 由 Provider.overrideElements/setDefaults 消费）。
    dqCommon::FeatureAppearance currentAppearance() const;

private:
    // 参考控件族装配（Color :322-385 / Transparencies :192-263 / Weight
    // :265-295 / Style :297-320 / 三复选 :119-138 / 按钮排 :140-166）。
    QWidget* buildColorRow();
    QWidget* buildTransparencyRow();
    QWidget* buildStyleRow();
    QWidget* buildWeightRow();
    QWidget* buildCheckBoxes();
    QWidget* buildButtons();

    void updateAppearance();

    dqApp::Viewport* m_vp;
    dqCommon::FeatureAppearanceProps m_props;
    // 拾色器状态（参考 createColorInput 的 "#ffffff" 默认值 :334）。
    QColor m_color{Qt::white};
    QColor m_lineColor{Qt::white};

    QCheckBox* m_colorCb = nullptr;
    QPushButton* m_colorBtn = nullptr;
    QCheckBox* m_applyColorToLines = nullptr;
    QCheckBox* m_lineColorCb = nullptr;
    QPushButton* m_lineColorBtn = nullptr;
    QCheckBox* m_transpCb = nullptr;
    QSpinBox* m_transpSpin = nullptr;
    QCheckBox* m_applyTranspToLines = nullptr;
    QCheckBox* m_lineTranspCb = nullptr;
    QSpinBox* m_lineTranspSpin = nullptr;
    QCheckBox* m_viewDependent = nullptr;
    QComboBox* m_styleCombo = nullptr;
    QCheckBox* m_weightCb = nullptr;
    QSpinBox* m_weightSpin = nullptr;
    QCheckBox* m_ignoreMaterial = nullptr;
    QCheckBox* m_nonLocatable = nullptr;
    QCheckBox* m_emphasized = nullptr;
};

// ---------------------------------------------------------------------------
// FeatureOverridesPanel — toolbar drop-down host.
// Ported from: itwinjs-core FeatureOverrides.ts FeatureOverridesPanel
// (:388-408——open=Settings 挂载；onViewChanged→Provider.remove :400-403)。
// DanQing 宿主形态 = 弹出面板（ViewSettingsPanel 先例——DTA ToolBarDropDown
// 的 Qt 等价）。
// ---------------------------------------------------------------------------
class FeatureOverridesPanel final : public QWidget {
    Q_OBJECT
public:
    FeatureOverridesPanel(dqApp::Viewport* vp, QWidget* parent);

private:
    FeatureOverridesSettings* m_settings;
};

}  // namespace Gui
