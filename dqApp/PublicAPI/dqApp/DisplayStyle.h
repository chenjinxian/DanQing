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
    dqBase::DqEvent<> OnMonochromeModeChanged;
    dqBase::DqEvent<> OnWhiteOnWhiteReversalChanged;
    dqBase::DqEvent<> OnAnalysisFractionChanged;
    dqBase::DqEvent<> OnTimePointChanged;
    dqBase::DqEvent<> OnExcludedElementsChanged;
    dqBase::DqEvent<> OnSubCategoryOverridesChanged;
    dqBase::DqEvent<> OnModelAppearanceOverrideChanged;
    dqBase::DqEvent<> OnBackgroundMapChanged;
    dqBase::DqEvent<> OnClipStyleChanged;
    dqBase::DqEvent<> OnThematicChanged;
    dqBase::DqEvent<> OnContoursChanged;  // M-X X-a1（DisplayStyleSettings.ts:517 onContoursChanged）
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

    // ── Monochrome mode (Flat / Scaled) ────────────────────────
    // ← itwinjs-core DisplayStyleSettings.monochromeMode（DisplayStyleSettings.ts
    //   的 MonochromeMode 枚举——Flat=色替换、Scaled=按亮度缩放到单色系）。
    //   RenderPlan.ts:114 从 settings 读入 plan；M-O(1) I1 随 Color/Scaled
    //   UI 子项接通（ViewAttributes.ts:400-402）。
    dqCommon::MonochromeMode getMonochromeMode() const noexcept { return m_settings.getMonochromeMode(); }
    void setMonochromeMode(dqCommon::MonochromeMode mode) {
        m_settings.setMonochromeMode(mode);
        OnMonochromeModeChanged.Raise();
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

    // ── Solar time（M-U U-c）────────────────────────────────────
    // ← itwinjs-core DisplayStyleState.setSunTime（DisplayStyleState.ts:989-992
    //   `this.settings.setSunTime(time, this.iModel)`）——参考 settings 侧
    //   IModel 分支：ecefLocation → Cartographic.fromEcef(origin)；缺席 →
    //   Philadelphia 默认（-75.17035, 39.954927, 0——SolarCalculate.test.ts
    //   同款坐标原样）。灯光赋值经 settings.setSunTime（direction+timePoint），
    //   事件 = onLightSettingsChanged（参考 lights setter 的 raise 面）。
    // 定义在 DisplayStyle.cpp（IModelConnection 完整类型面）。
    void setSunTime(double timePoint);
    // ← itwinjs-core DisplayStyle3dSettings.clearSunTime（DisplayStyleSettings.ts
    //   :1355-1364——只清 timePoint，不动 direction）。
    void clearSunTime();
    std::optional<double> getSunTime() const noexcept { return m_settings.getSunTime(); }

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
    // ← itwinjs-core DisplayStyleSettings.analysisFraction setter（:748-754）——
    // equals 短路比较系 **钳位后存值 vs 原始入参**（参考怪癖原样：越界入参
    // 每发必 raise）；短路命中后 raise **先于** 赋值（监听器事件内读旧值）。
    double getAnalysisFraction() const noexcept { return m_settings.getAnalysisFraction(); }
    void setAnalysisFraction(double fraction) {
        if (m_settings.getAnalysisFraction() == fraction) return;
        OnAnalysisFractionChanged.Raise();      // 参考序：raise 先于赋值
        m_settings.setAnalysisFraction(fraction);
    }

    // ── Analysis style ──────────────────────────────────────────
    // ← itwinjs-core DisplayStyleSettings.analysisStyle setter（:729-739）——
    // 对象同一性短路（DanQing 值语义以 equals 承载——EQUIVALENCE：TS 同一性
    // 系不可变对象优化，可观察行为=内容相等不 raise）；raise 先于赋值。
    std::optional<dqCommon::AnalysisStyle> const& getAnalysisStyle() const noexcept
    {
        return m_settings.getAnalysisStyle();
    }
    void setAnalysisStyle(std::optional<dqCommon::AnalysisStyle> style) {
        auto const& current = m_settings.getAnalysisStyle();
        bool const same = (!current.has_value() && !style.has_value()) ||
                          (current.has_value() && style.has_value() && current->equals(*style));
        if (same) return;
        OnAnalysisStyleChanged.Raise();         // 参考序：raise 先于赋值
        m_settings.setAnalysisStyle(std::move(style));
    }

    // ── Schedule script ─────────────────────────────────────────
    // ← itwinjs-core DisplayStyleState.scheduleScript（:377-399）——对象同一性
    // 短路（C++ unique_ptr get() 同值比较——等价 TS ===）；raise 先于赋值；
    // settings.scheduleScriptProps = script?.toJSON()。renderTimeline 回退腿
    // （loadScriptReferenceFromTimeline）系 RPC 数据面——E3 登记（dump 面无
    // RenderTimeline 元素表）。
    dqCommon::RenderSchedule::Script const* getScheduleScript() const
    {
        if (!m_scheduleScriptParsed) {
            m_scheduleScriptParsed = true;
            if (auto const& props = m_settings.getScheduleScriptProps(); props.has_value())
                m_scheduleScript = dqCommon::RenderSchedule::Script::fromJSON(*props);
        }
        return m_scheduleScript.get();
    }
    void setScheduleScript(std::unique_ptr<dqCommon::RenderSchedule::Script> script) {
        if (script.get() == m_scheduleScript.get() && m_scheduleScriptParsed)
            return;
        OnScheduleScriptChanged.Raise();        // 参考序：raise 先于赋值
        m_scheduleScriptParsed = true;
        m_scheduleScript = std::move(script);
        if (m_scheduleScript)
            m_settings.setScheduleScriptProps(m_scheduleScript->toJSON());
        else
            m_settings.setScheduleScriptProps(std::nullopt);
    }

    // ── Clip style ──────────────────────────────────────────────
    // ← itwinjs-core DisplayStyleState.clipStyle（settings.clipStyle 的 frontend
    //   门面——M-P P-B：Viewport.clipStyle 的读写面；setter 经
    //   DisplayStyleSettings.clipStyle 赋值 + onClipStyleChanged（DisplayStyle
    //   Settings.ts setter 语义），Viewport 既有监听（Viewport.cpp:649）接失效链）。
    dqCommon::ClipStyle const& getClipStyle() const noexcept { return m_settings.getClipStyle(); }
    void setClipStyle(dqCommon::ClipStyle const& style) {
        m_settings.setClipStyle(style);
        OnClipStyleChanged.Raise();
    }

    // ── Thematic display ────────────────────────────────────────
    // ← itwinjs-core DisplayStyleSettings.thematic setter（DisplayStyleSettings.ts
    //   :1221-1227——equals 短路 → **先 raise**（参考序：监听器在事件内读
    //   settings 得见旧值）→ 赋值）+ DisplayStyleState.applyOverrides 的
    //   projectExtents 补齐链（:1004-1016——消费面=ViewSettingsPanel 编辑器
    //   写回环与 overrideDisplayStyle 的 Height 无 range 默认）。
    // 事件无载荷（DqEvent<> 门面形态——参考携带新值形参，Viewport 监听面
    // [Viewport.cpp:678]不消费形参，EQUIVALENCE 登记：发散=事件载荷缺席，
    // 验证法=ThematicFacade 锁的监听器读旧值断言[先 raise 序可观测]）。
    dqCommon::ThematicDisplay const& getThematic() const noexcept { return m_settings.getThematic(); }
    void setThematic(dqCommon::ThematicDisplay const& td) {
        if (td.equals(m_settings.getThematic())) return;
        OnThematicChanged.Raise();      // 参考序：raise 先于赋值
        m_settings.setThematic(td);
    }

    // ── Contour display（M-X X-a1——同 thematic 门面形）─────────────
    // ← itwinjs-core DisplayStyle3dSettings.contours setter
    //   （DisplayStyleSettings.ts:1231-1239——equals 短路 → **先 raise**
    //   onContoursChanged[:517] → 赋值 + _json3d.contours 写出[写出段归
    //   dqCommon toJSON3d]）。事件无载荷（DqEvent<> 门面形态——参考携带
    //   新值形参，Viewport 监听面[Viewport.ts:1408]不消费形参，
    //   EQUIVALENCE 同 setThematic 注）。
    dqCommon::ContourDisplay const& getContours() const noexcept { return m_settings.getContours(); }
    void setContours(dqCommon::ContourDisplay const& contours) {
        if (contours.equals(m_settings.getContours())) return;
        OnContoursChanged.Raise();      // 参考序：raise 先于赋值
        m_settings.setContours(contours);
    }
    // iModel 访问（projectExtents 补齐链消费——DisplayStyleState.ts:1010）。
    IModelConnection const* getIModel() const noexcept { return m_iModel; }

    // ── Ambient occlusion（M-T T-b——同 thematic 门面形）─────────────
    // ← itwinjs-core DisplayStyleSettings.ambientOcclusionSettings setter
    //   （onAmbientOcclusionChanged——OnAmbientOcclusionChanged 事件与
    //   Viewport 监听面[Viewport.cpp:690]本件前无 raise 方，同族清偿）。
    dqCommon::AmbientOcclusion::Settings const& getAmbientOcclusionSettings() const noexcept {
        return m_settings.getAmbientOcclusionSettings();
    }
    void setAmbientOcclusionSettings(dqCommon::AmbientOcclusion::Settings const& ao) {
        if (ao.equals(m_settings.getAmbientOcclusionSettings())) return;
        OnAmbientOcclusionChanged.Raise();  // 参考序（同 setThematic 注）
        m_settings.setAmbientOcclusionSettings(ao);
    }

    // ── Time point ──────────────────────────────────────────────
    // ← itwinjs-core DisplayStyleSettings.timePoint setter（:718-723）——
    // 同一性（!==）短路 + raise 先于赋值；optional 承载 undefined。
    std::optional<double> getTimePoint() const noexcept { return m_settings.getTimePoint(); }
    void setTimePoint(std::optional<double> time) {
        if (m_settings.getTimePoint() == time) return;
        OnTimePointChanged.Raise();             // 参考序：raise 先于赋值
        m_settings.setTimePoint(time);
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
    // ← itwinjs-core DisplayStyleState._scriptReference（:378 读面）——
    // settings.scheduleScriptProps 的解析缓存（懒建）。
    mutable bool m_scheduleScriptParsed = false;
    mutable std::unique_ptr<dqCommon::RenderSchedule::Script> m_scheduleScript;
    // getBackgroundMapGeometry 的缓存（参考 _backgroundMapGeometry
    // (DisplayStyleState.ts:717-721)：globeMode/bimElevationBias 变键重建）。
    // mutable —— getBackgroundMapGeometry 逻辑上是只读查询。
    mutable std::optional<double> m_bmgBiasCache;
    mutable std::optional<dqCommon::GlobeMode> m_bmgGlobeModeCache;
    mutable std::unique_ptr<BackgroundMapGeometry> m_bmgCache;
    IModelConnection const* m_iModel = nullptr;  // not owned
};

}  // namespace dqApp
