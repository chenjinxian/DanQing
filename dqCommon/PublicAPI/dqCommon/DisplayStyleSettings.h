// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Display style settings
// Ported from: itwinjs-core core/common/src/DisplayStyleSettings.ts
//
// Pure data container for display style properties.
// Events are added by dqApp::DisplayStyle (which wraps this).
#pragma once

#include "AmbientOcclusion.h"
#include "AnalysisStyle.h"
#include "ClipStyle.h"
#include "ColorDef.h"
#include "Environment.h"
#include "Export.h"
#include "FeatureSymbology.h"
#include "HiddenLine.h"
#include "LightSettings.h"
#include "RenderSchedule.h"
#include "SolarCalculate.h"
#include "SubCategoryAppearance.h"
#include "ThematicDisplay.h"
#include "DqCommon.h"
#include "ViewFlags.h"
#include "WhiteOnWhiteReversalSettings.h"

#include <dqBase/DqId.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Monochrome rendering mode.
// Ported from: itwinjs-core MonochromeMode
enum class MonochromeMode : uint8_t {
    Flat = 0,
    Scaled = 1,
};

// JSON persistence for DisplayStyleSettings.
struct DisplayStyleSettingsProps {
    std::optional<ViewFlagProps> viewflags;
    std::optional<uint32_t> backgroundColor;
    std::optional<uint32_t> monochromeColor;
    std::optional<MonochromeMode> monochromeMode;
    std::optional<double> analysisFraction;
    std::optional<std::string> renderTimeline;
    std::optional<double> timePoint;
    std::optional<ClipStyleProps> clipStyle;
    std::optional<WhiteOnWhiteReversalProps> whiteOnWhiteReversal;
    // M-U U-b：analysisStyle/scheduleScript 数据面归位（:318 登记勾销——
    // DisplayStyleSettings.ts :53-54 TODO 清偿）。
    std::optional<AnalysisStyleProps> analysisStyle;
    std::optional<RenderSchedule::ScriptProps> scheduleScript;
    // TODO: subCategoryOvr, modelOvr, excludedElements, backgroundMap,
    //       mapImagery, contextRealityModels, planarClipOvr
};

// JSON persistence for DisplayStyle3dSettings.
struct DisplayStyle3dSettingsProps : DisplayStyleSettingsProps {
    std::optional<EnvironmentProps> environment;
    std::optional<ThematicDisplayProps> thematic;
    std::optional<HiddenLineSettingsProps> hline;
    std::optional<AmbientOcclusion::Props> ao;
    std::optional<LightSettingsProps> lights;
    // TODO: contours, solarShadows, planProjections (not yet ported)
};

// SubCategory override entry.
struct SubCategoryOverrideEntry {
    dqBase::DqId subCategoryId;
    SubCategoryOverride override;
};

// Model appearance override entry.
struct ModelAppearanceOverrideEntry {
    dqBase::DqId modelId;
    FeatureAppearance appearance;
};

// ---------------------------------------------------------------------------
// DisplayStyleSettings — base display style settings
// Ported from: itwinjs-core DisplayStyleSettings (lines 100-650)
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT DisplayStyleSettings {
public:
    DisplayStyleSettings() = default;

    // ── ViewFlags ─────────────────────────────────────────────────
    const ViewFlags& getViewFlags() const noexcept { return m_viewFlags; }
    void setViewFlags(const ViewFlags& flags) { m_viewFlags = flags; }

    // ── Background color ──────────────────────────────────────────
    ColorDef getBackgroundColor() const noexcept { return m_backgroundColor; }
    void setBackgroundColor(ColorDef color) { m_backgroundColor = color; }

    // ── Monochrome ────────────────────────────────────────────────
    ColorDef getMonochromeColor() const noexcept { return m_monochromeColor; }
    void setMonochromeColor(ColorDef color) { m_monochromeColor = color; }
    MonochromeMode getMonochromeMode() const noexcept { return m_monochromeMode; }
    void setMonochromeMode(MonochromeMode mode) { m_monochromeMode = mode; }

    // ── Analysis ──────────────────────────────────────────────────
    // Ported from: itwinjs-core DisplayStyleSettings.analysisFraction
    //   (:741-754)——getter 钳位 [0,1]（_json 值防御性再钳）；setter 存钳位值
    //   （事件层在 dqApp::DisplayStyle——settings 是纯数据容器）。
    double getAnalysisFraction() const noexcept
    {
        return std::max(0.0, std::min(1.0, m_analysisFraction));
    }
    void setAnalysisFraction(double fraction)
    {
        m_analysisFraction = std::max(0.0, std::min(1.0, fraction));
    }

    // ── Analysis style ────────────────────────────────────────────
    // Ported from: itwinjs-core DisplayStyleSettings.analysisStyle (:725-739)。
    std::optional<AnalysisStyle> const& getAnalysisStyle() const noexcept
    {
        return m_analysisStyle;
    }
    bool hasAnalysisStyle() const noexcept { return m_analysisStyle.has_value(); }
    void setAnalysisStyle(std::optional<AnalysisStyle> style) { m_analysisStyle = std::move(style); }

    // ── Schedule script ───────────────────────────────────────────
    // Ported from: itwinjs-core DisplayStyleSettings.scheduleScriptProps
    //   (:698-709)——raw JSON 透传（参考无 equals 短路）；Script 对象解析在
    //   dqApp::DisplayStyle::getScheduleScript 消费面。
    std::optional<RenderSchedule::ScriptProps> const& getScheduleScriptProps() const noexcept
    {
        return m_scheduleScriptProps;
    }
    void setScheduleScriptProps(std::optional<RenderSchedule::ScriptProps> props)
    {
        m_scheduleScriptProps = std::move(props);
    }

    // ── Render timeline ───────────────────────────────────────────
    const std::string& getRenderTimeline() const noexcept { return m_renderTimeline; }
    void setRenderTimeline(const std::string& id) { m_renderTimeline = id; }
    bool hasRenderTimeline() const noexcept { return !m_renderTimeline.empty(); }

    // ── Time point ────────────────────────────────────────────────
    std::optional<double> getTimePoint() const noexcept { return m_timePoint; }
    void setTimePoint(std::optional<double> tp) { m_timePoint = tp; }

    // ── Clip style ────────────────────────────────────────────────
    const ClipStyle& getClipStyle() const noexcept { return m_clipStyle; }
    void setClipStyle(const ClipStyle& style) { m_clipStyle = style; }

    // ── White-on-white reversal ───────────────────────────────────
    const WhiteOnWhiteReversalSettings& getWhiteOnWhiteReversal() const noexcept { return m_whiteOnWhiteReversal; }
    void setWhiteOnWhiteReversal(const WhiteOnWhiteReversalSettings& settings) { m_whiteOnWhiteReversal = settings; }

    // ── SubCategory overrides ─────────────────────────────────────
    const std::unordered_map<uint64_t, SubCategoryOverride>& getSubCategoryOverrides() const noexcept { return m_subCategoryOverrides; }
    bool hasSubCategoryOverride() const noexcept { return !m_subCategoryOverrides.empty(); }
    void overrideSubCategory(const dqBase::DqId& id, const SubCategoryOverride& ovr);
    void dropSubCategoryOverride(const dqBase::DqId& id);
    const SubCategoryOverride* getSubCategoryOverride(const dqBase::DqId& id) const;

    // ── Model appearance overrides ────────────────────────────────
    const std::unordered_map<uint64_t, FeatureAppearance>& getModelAppearanceOverrides() const noexcept { return m_modelAppearanceOverrides; }
    bool hasModelAppearanceOverride() const noexcept { return !m_modelAppearanceOverrides.empty(); }
    void overrideModelAppearance(const dqBase::DqId& modelId, const FeatureAppearance& appearance);
    void dropModelAppearanceOverride(const dqBase::DqId& modelId);
    const FeatureAppearance* getModelAppearanceOverride(const dqBase::DqId& modelId) const;

    // ── Excluded elements ─────────────────────────────────────────
    const std::unordered_set<uint64_t>& getExcludedElements() const noexcept { return m_excludedElements; }
    void addExcludedElement(const dqBase::DqId& id);
    void dropExcludedElement(const dqBase::DqId& id);
    void clearExcludedElements();
    bool isExcluded(const dqBase::DqId& id) const;

    // ── JSON ──────────────────────────────────────────────────────
    DisplayStyleSettingsProps toJSON() const;
    void applyOverrides(const DisplayStyleSettingsProps& props);

    // ── 3d check ──────────────────────────────────────────────────
    virtual bool is3d() const noexcept { return false; }

protected:
    ViewFlags m_viewFlags;
    ColorDef m_backgroundColor = ColorDef::from(0, 0, 0);
    ColorDef m_monochromeColor = ColorDef::from(255, 255, 255);
    MonochromeMode m_monochromeMode = MonochromeMode::Flat;
    double m_analysisFraction = 0.0;
    std::optional<AnalysisStyle> m_analysisStyle;
    std::optional<RenderSchedule::ScriptProps> m_scheduleScriptProps;
    std::string m_renderTimeline;
    std::optional<double> m_timePoint;
    ClipStyle m_clipStyle;
    WhiteOnWhiteReversalSettings m_whiteOnWhiteReversal = WhiteOnWhiteReversalSettings::defaults();
    std::unordered_map<uint64_t, SubCategoryOverride> m_subCategoryOverrides;
    std::unordered_map<uint64_t, FeatureAppearance> m_modelAppearanceOverrides;
    std::unordered_set<uint64_t> m_excludedElements;
};

// ---------------------------------------------------------------------------
// DisplayStyle3dSettings — 3d-specific display style settings
// Ported from: itwinjs-core DisplayStyle3dSettings (lines 660-950)
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT DisplayStyle3dSettings : public DisplayStyleSettings {
public:
    DisplayStyle3dSettings() = default;

    bool is3d() const noexcept override { return true; }

    // ── Environment ───────────────────────────────────────────────
    const Environment& getEnvironment() const noexcept { return m_environment; }
    void setEnvironment(const Environment& env) { m_environment = env; }
    void toggleSkyBox(bool display) { m_environment.displaySky = display; }
    void toggleGroundPlane(bool display) { m_environment.displayGround = display; }

    // ── Thematic display ──────────────────────────────────────────
    const ThematicDisplay& getThematic() const noexcept { return m_thematic; }
    void setThematic(const ThematicDisplay& td) { m_thematic = td; }

    // ── Hidden line ───────────────────────────────────────────────
    const HiddenLineSettings& getHiddenLineSettings() const noexcept { return m_hiddenLine; }
    void setHiddenLineSettings(const HiddenLineSettings& settings) { m_hiddenLine = settings; }

    // ── Ambient occlusion ─────────────────────────────────────────
    const AmbientOcclusion::Settings& getAmbientOcclusionSettings() const noexcept { return m_ambientOcclusion; }
    void setAmbientOcclusionSettings(const AmbientOcclusion::Settings& settings) { m_ambientOcclusion = settings; }

    // ── Lights ────────────────────────────────────────────────────
    const LightSettings& getLights() const noexcept { return m_lights; }
    void setLights(const LightSettings& lights) { m_lights = lights; }

    // ── Solar time（M-U U-c）──────────────────────────────────────
    // Set the solar time point and the solar light direction derived from it.
    // Ported from: itwinjs-core DisplayStyle3dSettings.setSunTime
    //   (:1339-1353)——calculateSolarDirection(new Date(timePoint), cartoCenter)
    //   → lights.clone({solar: {direction, timePoint}})。IModel 分支（ecef
    //   查找 + Philadelphia 回退）在 dqApp::DisplayStyle::setSunTime 门面。
    void setSunTime(double timePoint, Cartographic const& location)
    {
        auto const direction = calculateSolarDirection(timePoint, location);
        m_lights.solar.direction = direction;
        m_lights.solar.timePoint = timePoint;
    }

    // Clear the solar time point stored in lights.solarLight.
    // Note: does not affect the solar light direction.
    // Ported from: itwinjs-core DisplayStyle3dSettings.clearSunTime (:1355-1364)
    void clearSunTime() noexcept
    {
        m_lights.solar.timePoint = std::nullopt;
    }

    // The current sun time, or absent if none.
    // Ported from: itwinjs-core DisplayStyle3dSettings.sunTime (:1327-1332)
    std::optional<double> getSunTime() const noexcept { return m_lights.solar.timePoint; }

    // ── JSON ──────────────────────────────────────────────────────
    DisplayStyle3dSettingsProps toJSON3d() const;
    void applyOverrides3d(const DisplayStyle3dSettingsProps& props);

protected:
    Environment m_environment;
    ThematicDisplay m_thematic;
    HiddenLineSettings m_hiddenLine = HiddenLineSettings::defaults();
    AmbientOcclusion::Settings m_ambientOcclusion = AmbientOcclusion::Settings::defaults();
    LightSettings m_lights;
};

END_DQ_COMMON_NAMESPACE
