// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Render pass shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/RenderPass.ts
//
// Render pass constants used by shaders to distinguish which render pass
// is currently executing. The u_renderPass uniform holds the active pass
// as a float; kRenderPass_* constants are used for comparison in shaders.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Render pass constants (included by other shaders via ShaderBuilder)
// ---------------------------------------------------------------------------
// Uniform:    u_renderPass (float) — current render pass value
// Constants:  kRenderPass_* (float) — named render pass values
//
// The values correspond to itwinjs-core RenderPass enum:
//   Background = 0, OpaqueLayers = 1, OpaqueLinear = 2, OpaquePlanar = 3,
//   OpaqueGeneral = 5, Classification = 6, Translucent = 8, HiddenEdge = 9,
//   Hilite = 10, WorldOverlay = 12, ViewOverlay = 13, PlanarClassification = 19
//
// Note: HiddenEdge is aliased to OpaqueGeneral from the shader POV.
// OverlayLayers and TranslucentLayers are aliased to OpaqueLayers.
//
// Ported from: itwinjs-core RenderPass.ts addRenderPass
static char const* kRenderPassConstants = R"glsl(
uniform float u_renderPass;

// Ported from: itwinjs-core RenderPass.ts renderPasses array
// Constant names match itwinjs-core exactly: kRenderPass_${name}
const float kRenderPass_Background          = 0.0;
const float kRenderPass_Layers              = 1.0;  // OpaqueLayers (shaders treat all layer passes the same)
const float kRenderPass_OpaqueLinear        = 2.0;
const float kRenderPass_OpaquePlanar        = 3.0;
const float kRenderPass_OpaqueGeneral       = 5.0;
const float kRenderPass_Classification      = 6.0;
const float kRenderPass_Translucent         = 8.0;
const float kRenderPass_HiddenEdge          = 9.0;  // raw enum value (aliased to 5 in C++ before upload)
const float kRenderPass_Hilite              = 10.0;
const float kRenderPass_WorldOverlay        = 12.0;
const float kRenderPass_ViewOverlay         = 13.0;
const float kRenderPass_PlanarClassification = 19.0;
)glsl";

END_DQ_RENDER_NAMESPACE
