// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DisplayStyle implementation
// Ported from: itwinjs-core core/frontend/src/DisplayStyleState.ts
#include "dqApp/DisplayStyle.h"

#include "BackgroundMapGeometry.h"
#include "dqApp/IModelConnection.h"

namespace dqApp {

DisplayStyle::DisplayStyle() = default;
DisplayStyle::~DisplayStyle() = default;

// Ported from: itwinjs-core DisplayStyleState.getBackgroundMapGeometry
//              (DisplayStyleState.ts:763-778).
BackgroundMapGeometry const* DisplayStyle::getBackgroundMapGeometry() const
{
    // :764-765 — if (!this._hasEarthLocation) return undefined;
    // _hasEarthLocation = undefined !== iModel.ecefLocation。DanQing 的
    // EcefLocation 是值成员——"ecefLocation defined" 对应非零原点（同
    // getGlobalGeometryAndHeightRange 的登记，BackgroundMapGeometry.cpp:436-447）。
    if (!m_iModel)
        return nullptr;
    auto const& ecef = m_iModel->GetEcefLocation();
    bool const hasEarthLocation = (ecef.origin.x != 0.0 || ecef.origin.y != 0.0 || ecef.origin.z != 0.0);
    if (!hasEarthLocation)
        return nullptr;

    // :767-770 — backgroundMapElevationBias：applyTerrain=false（blank 默认——
    // BackgroundMapSettings 未接入样式图）→ backgroundMapSettings.groundBias
    // = 0（BackgroundMapSettings.ts:142 默认）。
    double const bimElevationBias = 0.0;

    // :772 — globeMode = this.globeMode（settings.backgroundMap.globeMode；
    // 未接入样式图——取参考默认 Ellipsoid，BackgroundMapSettings.ts:38）。
    dqCommon::GlobeMode const globeMode = dqCommon::GlobeMode::Ellipsoid;

    // :773-776 — 缓存：globeMode/bimElevationBias 变键重建。
    if (!m_bmgCache || m_bmgGlobeModeCache != globeMode
        || !m_bmgBiasCache.has_value() || *m_bmgBiasCache != bimElevationBias) {
        m_bmgCache = std::make_unique<BackgroundMapGeometry>(bimElevationBias, globeMode, m_iModel);
        m_bmgBiasCache = bimElevationBias;
        m_bmgGlobeModeCache = globeMode;
    }
    return m_bmgCache.get();
}

}  // namespace dqApp
