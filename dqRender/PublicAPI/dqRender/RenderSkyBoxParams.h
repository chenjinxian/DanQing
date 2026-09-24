// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Sky box parameter types
// Ported from: itwinjs-core core/frontend/src/internal/render/RenderSkyBoxParams.ts
//
// Discriminated union of sky box parameter variants.  The active variant is
// selected by the SkyBoxType tag, mirroring itwinjs's TypeScript discriminated
// union (RenderSkyGradientParams | RenderSkySphereParams | RenderSkyCubeParams).
#pragma once

#include "dqRender/rhi/Handle.h"

#include <dqCommon/SkyBox.h>  // SkyGradient
#include <dqGeom/Vector3d.h>

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SkyBoxType — discriminator for sky box parameter variants
// ---------------------------------------------------------------------------
enum class SkyBoxType : uint8_t {
    Gradient,
    Sphere,
    Cube,
};

// ---------------------------------------------------------------------------
// RenderSkyGradientParams — gradient-based sky
// (Ported from: itwinjs-core RenderSkyBoxParams.ts RenderSkyGradientParams:
//  { type: "gradient"; gradient: SkyGradient; zOffset: number })
//
// Carries the full SkyGradient (twoColor flag + zenith/sky/ground/nadir colors
// + sky/ground exponents) so the SkySphereViewportQuadGeometry ctor can branch
// 2-color vs 4-color exactly like the reference (CachedGeometry.ts:686-716).
// Earlier this stored only topColor/bottomColor, forcing the 2-color path
// always (audit D9.1).
// ---------------------------------------------------------------------------
struct RenderSkyGradientParams {
    SkyBoxType type = SkyBoxType::Gradient;
    dqCommon::SkyGradient gradient;
    float zOffset = 0.0f;
    // Ported from: itwinjs-core SkySphere.ts:145-180 (u_groundColor/u_nadirColor
    // graphic uniforms). When the background map is on, the map-facing hemisphere
    // of the sky sphere is painted skyColor (the ground/nadir stops are replaced).
    // DanQing-only field (the TS reference reads plan.backgroundMapOn off the
    // target; DanQing threads it through the params since the geometry has no plan).
    bool backgroundMapOn = false;
};

// ---------------------------------------------------------------------------
// RenderSkySphereParams — spherical sky texture
// (Ported from: itwinjs-core RenderSkyBoxParams.ts RenderSkySphereParams)
// ---------------------------------------------------------------------------
struct RenderSkySphereParams {
    SkyBoxType type = SkyBoxType::Sphere;
    rhi::TextureHandle texture;
    float rotation = 0.0f;
    float zOffset = 0.0f;
};

// ---------------------------------------------------------------------------
// RenderSkyCubeParams — cube-mapped sky texture
// (Ported from: itwinjs-core RenderSkyBoxParams.ts RenderSkyCubeParams)
// ---------------------------------------------------------------------------
struct RenderSkyCubeParams {
    SkyBoxType type = SkyBoxType::Cube;
    rhi::TextureHandle texture;
};

// ---------------------------------------------------------------------------
// SkySphereGlobeParams — the plan's globe-mode state driving the sky quad's
// globe branch（CachedGeometry.ts:597-651）。
// Ported from: itwinjs-core RenderPlan（RenderPlan.ts:106 isGlobeMode3D /
//              :132-141 upVector / :108-109 frustum+frustFraction）。
// ---------------------------------------------------------------------------
struct SkySphereGlobeParams {
    // plan.isGlobeMode3D（RenderPlan.ts:106 — GlobeMode.Ellipsoid === view.globeMode）。
    bool isGlobeMode3D = false;
    // plan.upVector（RenderPlan.ts:132-141 — view.getUpVector(视锥中心)，
    // ViewState.ts:1246-1255；非地理定位/点在项目范围内时调用侧已归约为 unitZ）。
    dqGeom::Vector3d upVector{0.0, 0.0, 1.0};
    // FrustumUniformType === Perspective（FrustumUniforms.ts:18-22）→ u_worldEye
    // 用真实相机位置（SkySphere.ts:240-250）；否则正交伪相机（:251-264）。
    bool perspective = false;
    // plan.planFraction（RenderPlan.ts:109 — vp.viewingSpace.frustFraction），
    // 仅透视路径使用（scale = 1/(1-planFraction)）。
    double planFraction = 1.0;
};

END_DQ_RENDER_NAMESPACE
