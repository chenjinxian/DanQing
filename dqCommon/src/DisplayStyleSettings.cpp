// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Display style settings implementation
// Ported from: itwinjs-core core/common/src/DisplayStyleSettings.ts
#include "dqCommon/DisplayStyleSettings.h"

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqBase;

// ---------------------------------------------------------------------------
// DisplayStyleSettings
// ---------------------------------------------------------------------------

void DisplayStyleSettings::overrideSubCategory(const DqId& id, const SubCategoryOverride& ovr)
{
    m_subCategoryOverrides[id.GetValue()] = ovr;
}

void DisplayStyleSettings::dropSubCategoryOverride(const DqId& id)
{
    m_subCategoryOverrides.erase(id.GetValue());
}

const SubCategoryOverride* DisplayStyleSettings::getSubCategoryOverride(const DqId& id) const
{
    auto it = m_subCategoryOverrides.find(id.GetValue());
    return it != m_subCategoryOverrides.end() ? &it->second : nullptr;
}

void DisplayStyleSettings::overrideModelAppearance(const DqId& modelId, const FeatureAppearance& appearance)
{
    m_modelAppearanceOverrides[modelId.GetValue()] = appearance;
}

void DisplayStyleSettings::dropModelAppearanceOverride(const DqId& modelId)
{
    m_modelAppearanceOverrides.erase(modelId.GetValue());
}

const FeatureAppearance* DisplayStyleSettings::getModelAppearanceOverride(const DqId& modelId) const
{
    auto it = m_modelAppearanceOverrides.find(modelId.GetValue());
    return it != m_modelAppearanceOverrides.end() ? &it->second : nullptr;
}

void DisplayStyleSettings::addExcludedElement(const DqId& id)
{
    m_excludedElements.insert(id.GetValue());
}

void DisplayStyleSettings::dropExcludedElement(const DqId& id)
{
    m_excludedElements.erase(id.GetValue());
}

void DisplayStyleSettings::clearExcludedElements()
{
    m_excludedElements.clear();
}

bool DisplayStyleSettings::isExcluded(const DqId& id) const
{
    return m_excludedElements.count(id.GetValue()) > 0;
}

DisplayStyleSettingsProps DisplayStyleSettings::toJSON() const
{
    DisplayStyleSettingsProps p;
    p.viewflags = m_viewFlags.toJSON();
    p.backgroundColor = m_backgroundColor.getTbgr();
    p.monochromeColor = m_monochromeColor.getTbgr();
    p.monochromeMode = m_monochromeMode;
    p.analysisFraction = m_analysisFraction;
    if (!m_renderTimeline.empty()) p.renderTimeline = m_renderTimeline;
    p.timePoint = m_timePoint;
    p.clipStyle = m_clipStyle.toJSON();
    p.whiteOnWhiteReversal = m_whiteOnWhiteReversal.toJSON();
    return p;
}

void DisplayStyleSettings::applyOverrides(const DisplayStyleSettingsProps& props)
{
    if (props.viewflags) {
        // 合并语义（1:1 _applyOverrides :1012-1016）：
        //   ViewFlags.fromJSON({...this.viewFlags.toJSON(), ...overrides.viewflags})
        // —— 现值 toJSON 只携带偏离位；覆写仅覆盖在场位，缺席位经偏离标记保持
        //   现值（无偏离时 fromJSON 回落默认=现值）。M-Q Q-a：此前整替
        //   fromJSON(&props) 使缺席位（grid/weights/…）回默认——真语义缺口。
        ViewFlagProps merged = m_viewFlags.toJSON();
        ViewFlagProps const& o = *props.viewflags;
        if (o.noConstruct) merged.noConstruct = o.noConstruct;
        if (o.noDim) merged.noDim = o.noDim;
        if (o.noPattern) merged.noPattern = o.noPattern;
        if (o.noWeight) merged.noWeight = o.noWeight;
        if (o.noStyle) merged.noStyle = o.noStyle;
        if (o.noTransp) merged.noTransp = o.noTransp;
        if (o.noFill) merged.noFill = o.noFill;
        if (o.grid) merged.grid = o.grid;
        if (o.acs) merged.acs = o.acs;
        if (o.noTexture) merged.noTexture = o.noTexture;
        if (o.noMaterial) merged.noMaterial = o.noMaterial;
        if (o.noCameraLights) merged.noCameraLights = o.noCameraLights;
        if (o.noSourceLights) merged.noSourceLights = o.noSourceLights;
        if (o.noSolarLight) merged.noSolarLight = o.noSolarLight;
        if (o.visEdges) merged.visEdges = o.visEdges;
        if (o.hidEdges) merged.hidEdges = o.hidEdges;
        if (o.shadows) merged.shadows = o.shadows;
        if (o.clipVol) merged.clipVol = o.clipVol;
        if (o.monochrome) merged.monochrome = o.monochrome;
        if (o.backgroundMap) merged.backgroundMap = o.backgroundMap;
        if (o.ambientOcclusion) merged.ambientOcclusion = o.ambientOcclusion;
        if (o.thematicDisplay) merged.thematicDisplay = o.thematicDisplay;
        if (o.wiremesh) merged.wiremesh = o.wiremesh;
        if (o.forceSurfaceDiscard) merged.forceSurfaceDiscard = o.forceSurfaceDiscard;
        if (o.noWhiteOnWhiteReversal) merged.noWhiteOnWhiteReversal = o.noWhiteOnWhiteReversal;
        if (o.renderMode) merged.renderMode = o.renderMode;
        m_viewFlags = ViewFlags::fromJSON(&merged);
    }
    if (props.backgroundColor) m_backgroundColor = ColorDef::fromTbgr(*props.backgroundColor);
    if (props.monochromeColor) m_monochromeColor = ColorDef::fromTbgr(*props.monochromeColor);
    if (props.monochromeMode) m_monochromeMode = *props.monochromeMode;
    if (props.analysisFraction) m_analysisFraction = *props.analysisFraction;
    if (props.renderTimeline) m_renderTimeline = *props.renderTimeline;
    if (props.timePoint) m_timePoint = *props.timePoint;
    if (props.clipStyle) m_clipStyle = ClipStyle::fromJSON(&*props.clipStyle);
    if (props.whiteOnWhiteReversal) m_whiteOnWhiteReversal = WhiteOnWhiteReversalSettings::fromJSON(&*props.whiteOnWhiteReversal);
}

// ---------------------------------------------------------------------------
// DisplayStyle3dSettings
// ---------------------------------------------------------------------------

DisplayStyle3dSettingsProps DisplayStyle3dSettings::toJSON3d() const
{
    DisplayStyle3dSettingsProps p;
    // copy base
    static_cast<DisplayStyleSettingsProps&>(p) = toJSON();
    // 3d-specific
    p.environment = m_environment.toJSON();
    p.thematic = m_thematic.toJSON();
    p.hline = HiddenLineSettingsProps{m_hiddenLine.visible.toJSON(), m_hiddenLine.hidden.toJSON(), m_hiddenLine.transparencyThreshold};
    p.ao = m_ambientOcclusion.toJSON();
    p.lights = m_lights.toJSON();  // M-Q Q-a：toJSON 接线（LightSettings::toJSON 已在）
    return p;
}

void DisplayStyle3dSettings::applyOverrides3d(const DisplayStyle3dSettingsProps& props)
{
    // Apply base
    applyOverrides(props);
    // 3d-specific
    if (props.environment) m_environment = Environment::fromJSON(&*props.environment);
    if (props.thematic) m_thematic = ThematicDisplay::fromJSON(&*props.thematic);
    if (props.hline) m_hiddenLine = HiddenLineSettings::fromJSON(*props.hline);
    if (props.ao) m_ambientOcclusion = AmbientOcclusion::Settings::fromJSON(&*props.ao);
    // Ported from: itwinjs-core DisplayStyleSettings constructor —
    // `this._json3d.lights = LightSettings.fromJSON(this._json3d.lights)`
    // (M-M(1): saved display styles with non-default light rigs were silently
    // dropped before this; defaults are otherwise LightSettings{} == reference).
    if (props.lights) m_lights = LightSettings::fromJSON(*props.lights);
    // TODO: contours, solarShadows, planProjections
}

END_DQ_COMMON_NAMESPACE
