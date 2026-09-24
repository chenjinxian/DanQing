// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render plan (view rendering configuration)
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderPlan.ts
//
// The RenderPlan captures all the information needed to render a view:
// view flags, background color, lights, analysis settings, etc.
// It is created from a ViewState and passed to the RenderTarget.
#pragma once

#include "Export.h"

#include <dqCommon/FeatureOverrides.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/LightSettings.h>
#include <dqCommon/ViewFlags.h>

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderPlan — complete rendering configuration for a view
// Ported from: itwinjs-core RenderPlan
// ---------------------------------------------------------------------------
struct DQ_RENDER_EXPORT RenderPlan {
    // View flags (render mode, edges, shadows, etc.)
    dqCommon::ViewFlags viewFlags;

    // Background color (RGBA)
    uint32_t backgroundColor = 0xFFFFFFFF;

    // Light settings
    float sunDirection[3] = {0.3f, 0.5f, 0.8f};
    float sunIntensity = 0.7f;
    float ambientColor[3] = {0.3f, 0.3f, 0.35f};

    // Full typed light settings (solar/ambient/hemisphere/portrait/fresnel).
    // Ported from: itwinjs-core RenderPlan.lights. Default-constructed =
    // itwinjs LightSettings.fromJSON(undefined) defaults (solar 1.0, ambient
    // black/0.2, hemisphere 0, portrait 0.3, specular 1.0). Drives
    // LightingUniforms (u_lightSettings[16]) via TargetUniforms::updateRenderPlan.
    dqCommon::LightSettings lights;

    // Analysis
    float analysisFraction = 0.0f;

    // Time point (for schedule scripts)
    double timePoint = 0.0;

    // Monochrome
    bool monochromeMode = false;
    uint32_t monochromeColor = 0xFF000000;

    // White-on-white reversal
    bool whiteOnWhiteReversal = true;

    // Feature symbology overrides
    // (stored by pointer to avoid copying large collections)
    dqCommon::FeatureOverrides const* featureOverrides = nullptr;

    // View volume — feeds Target.changeRenderPlan → FrustumUniforms.changeFrustum
    // (lookIn + ortho(0,depth)/frustum() = the reference's u_proj/u_mv source).
    // Ported from: itwinjs-core RenderPlan.ts:48 (is3d), :66 (frustum), :67 (fraction).
    bool is3d = true;
    dqCommon::Frustum frustum;
    double fraction = 0.0;

    // Check if this plan equals another (for change detection)
    bool equals(RenderPlan const& rhs) const {
        if (!viewFlags.equals(rhs.viewFlags)) return false;
        if (backgroundColor != rhs.backgroundColor) return false;
        for (int i = 0; i < 3; ++i) {
            if (sunDirection[i] != rhs.sunDirection[i]) return false;
            if (ambientColor[i] != rhs.ambientColor[i]) return false;
        }
        if (sunIntensity != rhs.sunIntensity) return false;
        if (!lights.equals(rhs.lights)) return false;
        if (analysisFraction != rhs.analysisFraction) return false;
        if (timePoint != rhs.timePoint) return false;
        if (monochromeMode != rhs.monochromeMode) return false;
        if (monochromeColor != rhs.monochromeColor) return false;
        if (whiteOnWhiteReversal != rhs.whiteOnWhiteReversal) return false;
        if (is3d != rhs.is3d) return false;
        if (fraction != rhs.fraction) return false;
        if (!frustum.equals(rhs.frustum)) return false;
        // Feature overrides pointer comparison is sufficient
        // (overrides are rebuilt when they change)
        if (featureOverrides != rhs.featureOverrides) return false;
        return true;
    }

    bool operator==(RenderPlan const& rhs) const { return equals(rhs); }
    bool operator!=(RenderPlan const& rhs) const { return !equals(rhs); }
};

END_DQ_RENDER_NAMESPACE
