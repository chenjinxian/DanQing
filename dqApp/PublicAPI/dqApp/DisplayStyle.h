// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Display style state (frontend wrapper)
// Ported from: itwinjs-core core/frontend/src/DisplayStyleState.ts
//              (wraps core/common/src/DisplayStyleSettings.ts)
//
// Faithful 1:1 structure: in itwinjs, DisplayStyle3dState (frontend) wraps a
// DisplayStyle3dSettings (common) and adds change events + iModel binding.
// dqApp::DisplayStyle accordingly owns a dqCommon::DisplayStyle3dSettings and
// fires events on mutation. All appearance data (ViewFlags, backgroundColor,
// monochromeColor, environment, lights, whiteOnWhiteReversal, ...) lives in the
// faithful dqCommon settings — dqApp adds only the change-event layer.
#pragma once

#include "Export.h"

#include <dqBase/DqEvent.h>
#include <dqCommon/DisplayStyleSettings.h>
#include <dqCommon/GlobeMode.h>
#include <dqCommon/ViewFlags.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace dqApp {

class BackgroundMapGeometry;
class IModelConnection;

// ---------------------------------------------------------------------------
// DisplayStyle — frontend display-style state wrapping dqCommon settings.
// Ported from: itwinjs-core DisplayStyle3dState (DisplayStyleState.ts).
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT DisplayStyle {
public:
    // Out-of-line ctor/dtor: unique_ptr<BackgroundMapGeometry>（私有头不完全类型）
    // 的成员初始化/销毁需要完整类型——定义都在 DisplayStyle.cpp。
    DisplayStyle();
    ~DisplayStyle();

    // Change events (← itwinjs-core DisplayStyleSettings events).
    dqBase::DqEvent<> OnViewFlagsChanged;
    dqBase::DqEvent<> OnBackgroundColorChanged;
    dqBase::DqEvent<> OnEnvironmentChanged;
    dqBase::DqEvent<> OnLightSettingsChanged;
    dqBase::DqEvent<> OnMonochromeColorChanged;
    dqBase::DqEvent<> OnWhiteOnWhiteReversalChanged;
    dqBase::DqEvent<> OnAnalysisFractionChanged;
    dqBase::DqEvent<> OnTimePointChanged;
    dqBase::DqEvent<> OnExcludedElementsChanged;
    dqBase::DqEvent<> OnSubCategoryOverridesChanged;
    dqBase::DqEvent<> OnModelAppearanceOverrideChanged;
    dqBase::DqEvent<> OnBackgroundMapChanged;
    dqBase::DqEvent<> OnClipStyleChanged;
    dqBase::DqEvent<> OnThematicChanged;
    dqBase::DqEvent<> OnHiddenLineChanged;
    dqBase::DqEvent<> OnAmbientOcclusionChanged;
    dqBase::DqEvent<> OnSolarShadowsChanged;
    dqBase::DqEvent<> OnAnalysisStyleChanged;
    dqBase::DqEvent<> OnRenderTimelineChanged;
    dqBase::DqEvent<> OnScheduleScriptChanged;
    dqBase::DqEvent<> OnPlanProjectionChanged;

    // Direct access to the faithful dqCommon settings
    // (← itwinjs-core DisplayStyle3dState.settings).
    dqCommon::DisplayStyle3dSettings const& getSettings() const noexcept { return m_settings; }
    dqCommon::DisplayStyle3dSettings& getSettings() noexcept { return m_settings; }

    // ── ViewFlags ───────────────────────────────────────────────
    // ← itwinjs-core DisplayStyle3dState.viewFlags
    dqCommon::ViewFlags const& getViewFlags() const noexcept { return m_settings.getViewFlags(); }
    void setViewFlags(dqCommon::ViewFlags const& flags) {
        if (m_settings.getViewFlags().equals(flags)) return;
        m_settings.setViewFlags(flags);
        OnViewFlagsChanged.Raise();
    }

    // ── Background color (uint32 tbgr adapter over ColorDef) ────
    // ← itwinjs-core DisplayStyle3dState.backgroundColor (ColorDef; default black)
    uint32_t getBackgroundColor() const noexcept { return m_settings.getBackgroundColor().getTbgr(); }
    void setBackgroundColor(uint32_t tbgr) {
        m_settings.setBackgroundColor(dqCommon::ColorDef::fromTbgr(tbgr));
        OnBackgroundColorChanged.Raise();
    }

    // ── Monochrome color (uint32 tbgr adapter) ──────────────────
    uint32_t getMonochromeColor() const noexcept { return m_settings.getMonochromeColor().getTbgr(); }
    void setMonochromeColor(uint32_t tbgr) {
        m_settings.setMonochromeColor(dqCommon::ColorDef::fromTbgr(tbgr));
        OnMonochromeColorChanged.Raise();
    }

    // ── Environment (sky / ground plane) ────────────────────────
    // ← itwinjs-core DisplayStyle3dState.environment
    dqCommon::Environment const& getEnvironment() const noexcept { return m_settings.getEnvironment(); }
    void setEnvironment(dqCommon::Environment const& env) {
        m_settings.setEnvironment(env);
        OnEnvironmentChanged.Raise();
    }
    // ← itwinjs-core DisplayStyle3dState.toggleSkyBox(display)
    void toggleSkyBox(bool display) {
        m_settings.toggleSkyBox(display);
        OnEnvironmentChanged.Raise();
    }
    // ← itwinjs-core DisplayStyle3dState.toggleGroundPlane(display)
    void toggleGroundPlane(bool display) {
        m_settings.toggleGroundPlane(display);
        OnEnvironmentChanged.Raise();
    }

    // ── Light settings ──────────────────────────────────────────
    // ← itwinjs-core DisplayStyle3dState.lights
    dqCommon::LightSettings const& GetLightSettings() const noexcept { return m_settings.getLights(); }
    void setLightSettings(dqCommon::LightSettings const& lights) {
        m_settings.setLights(lights);
        OnLightSettingsChanged.Raise();
    }

    // ── White-on-white reversal ─────────────────────────────────
    // ← itwinjs-core: rendering reads viewFlags.whiteOnWhiteReversal (default true).
    // Routed through ViewFlags rather than a duplicate field.
    bool getWhiteOnWhiteReversal() const noexcept {
        return m_settings.getViewFlags().whiteOnWhiteReversal();
    }
    void setWhiteOnWhiteReversal(bool enabled) {
        if (m_settings.getViewFlags().whiteOnWhiteReversal() == enabled) return;
        auto props = m_settings.getViewFlags().Properties();
        props.whiteOnWhiteReversal = enabled;
        m_settings.setViewFlags(dqCommon::ViewFlags(props));
        OnWhiteOnWhiteReversalChanged.Raise();
    }

    // ── Analysis fraction ───────────────────────────────────────
    double getAnalysisFraction() const noexcept { return m_settings.getAnalysisFraction(); }
    void setAnalysisFraction(double fraction) {
        if (m_settings.getAnalysisFraction() == fraction) return;
        m_settings.setAnalysisFraction(fraction);
        OnAnalysisFractionChanged.Raise();
    }

    // ── Time point ──────────────────────────────────────────────
    double getTimePoint() const noexcept { return m_settings.getTimePoint().value_or(0.0); }
    void setTimePoint(double time) {
        m_settings.setTimePoint(time);
        OnTimePointChanged.Raise();
    }

    // ── Clone (data only; events not cloned) ────────────────────
    // Output-parameter form because DisplayStyle has a deleted copy constructor
    // (DqEvent holds a mutex).
    void cloneDataTo(DisplayStyle& target) const { target.m_settings = m_settings; }

    // ── Background map geometry ─────────────────────────────────
    // Ported from: itwinjs-core DisplayStyleState.getBackgroundMapGeometry
    //              (DisplayStyleState.ts:763-778)：ecefLocation 门 + 缓存
    // （globeMode/bimElevationBias 变键重建）。样式设置图未接线——bias 取
    // groundBias 默认 0、globeMode 取参考默认 Ellipsoid（同
    // BackgroundMapGeometry.cpp getGlobalGeometryAndHeightRange 的登记）。
    // 返回值归本对象的缓存所有（借用语义，同参考返回缓存实例）。
    class BackgroundMapGeometry const* getBackgroundMapGeometry() const;
    // iModel 绑定（参考 DisplayStyleState 经 ElementState 持有 iModel；DanQing 的
    // DisplayStyle 是 ViewState 的值成员，由 ViewState::SetIModel 系挂——见
    // ViewState.cpp:1524 及三处克隆点）。
    void setIModel(IModelConnection const* im) noexcept { m_iModel = im; }

private:
    dqCommon::DisplayStyle3dSettings m_settings;
    // getBackgroundMapGeometry 的缓存（参考 _backgroundMapGeometry
    // (DisplayStyleState.ts:717-721)：globeMode/bimElevationBias 变键重建）。
    // mutable —— getBackgroundMapGeometry 逻辑上是只读查询。
    mutable std::optional<double> m_bmgBiasCache;
    mutable std::optional<dqCommon::GlobeMode> m_bmgGlobeModeCache;
    mutable std::unique_ptr<BackgroundMapGeometry> m_bmgCache;
    IModelConnection const* m_iModel = nullptr;  // not owned
};

}  // namespace dqApp
