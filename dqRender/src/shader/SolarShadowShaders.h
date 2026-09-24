// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Solar shadow (EVSM) shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/SolarShadowMapping.ts
//
// Exponential Variance Shadow Mapping (EVSM) for soft shadow rendering.
// Uses warpDepth, chebyshevUpperBound, and shadowMapEVSM for shadow evaluation.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Solar shadow vertex shader
// ---------------------------------------------------------------------------
// Attributes: a_position (vec3), a_normal (vec3)
// Uniforms:   u_mvp (mat4), u_shadowProj (mat4)
// Varyings:   v_shadowPos (vec3), v_normal (vec3)
// Ported from: itwinjs-core SolarShadowMapping.ts computeShadowPos / computePosition
static char const* kSolarShadowMapVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_mvp;
uniform mat4 u_shadowProj;

out vec3 v_shadowPos;
out vec3 v_normal;

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    vec4 shadowProj = u_shadowProj * vec4(a_position, 1.0);
    v_shadowPos = shadowProj.xyz / shadowProj.w;
    v_shadowPos.z = 1.0 - v_shadowPos.z;
    v_normal = a_normal;
}
)glsl";

// ---------------------------------------------------------------------------
// Solar shadow fragment shader
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core SolarShadowMapping.ts
//   - warpDepth: exponential warping for 32-bit float (exponent=42.0)
//   - chebyshevUpperBound: one-tailed Chebyshev inequality for soft shadows
//   - shadowMapEVSM: samples shadow texture, warps depth, computes variance
//   - applySolarShadowMap: bounds-checks shadow position, applies EVSM blend
//
// Key differences from previous version:
//   - Uses TEXTURE() macro instead of texture() directly
//   - Out-of-bounds returns baseColor (not vec4(1.0))
//   - Shadow multiplies into baseColor (not replaces it)
//   - Uses u_surfaceFlags[kSurfaceBitIndex_HasNormals] instead of u_hasNormals
static char const* kSolarShadowMapFrag = R"glsl(
#version 410 core

in vec3 v_shadowPos;

uniform sampler2D s_shadowSampler;
uniform vec4 u_shadowParams;      // rgb = shadow color, a = bias
uniform vec3 u_sunDir;
uniform float u_evsmExponent;     // 42.0 for 32-bit, 5.545 for 16-bit
uniform uint u_surfaceFlags[4];   // for HasNormals check

in vec3 v_normal;

// baseColor is set by the calling shader (ComputeBaseColor slot)
// The applySolarShadowMap slot receives it as a parameter.

const float kVSMBias = 0.1;

// Surface flag bit indices
// Ported from: itwinjs-core Surface.ts kSurfaceBitIndex_*
const int kSurfaceBitIndex_HasNormals = 1;

// Applies exponential warp to shadow map depth, input depth should be in [0, 1]
// Ported from: itwinjs-core SolarShadowMapping.ts warpDepth
vec2 warpDepth(float depth, float exponent) {
    depth = 2.0 * depth - 1.0; // Rescale depth into [-1, 1]
    float pos =  exp( exponent * depth);
    float neg = -exp(-exponent * depth);
    return vec2(pos, neg);
}

// One-tailed Chebyshev inequality for probabilistic upper bound
// Ported from: itwinjs-core SolarShadowMapping.ts chebyshevUpperBound
float chebyshevUpperBound(vec2 moments, float mean, float minVariance) {
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, minVariance);

    // Compute probabilistic upper bound
    float d = mean - moments.x;
    float pMax = variance / (variance + (d * d));

    return (mean <= moments.x ? 1.0 : pMax);
}

// Sample shadow texture, warp depth, compute variance contributions
// Ported from: itwinjs-core SolarShadowMapping.ts shadowMapEVSM
float shadowMapEVSM(vec3 shadowPos) {
    vec2 warpedDepth = warpDepth(shadowPos.z, u_evsmExponent);
    // Ported from: itwinjs-core SolarShadowMapping.ts line 56 — uses TEXTURE macro
    vec4 occluder = TEXTURE(s_shadowSampler, shadowPos.xy);

    // Derivative of warping at depth
    vec2 depthScale = kVSMBias * 0.01 * u_evsmExponent * warpedDepth;
    vec2 minVariance = depthScale * depthScale;

    float posContrib = chebyshevUpperBound(occluder.xz, warpedDepth.x, minVariance.x);
    float negContrib = chebyshevUpperBound(occluder.yw, warpedDepth.y, minVariance.y);
    return min(posContrib, negContrib);
}

// applySolarShadowMap — bounds-check shadow position and apply EVSM blend
// Ported from: itwinjs-core SolarShadowMapping.ts applySolarShadowMap (line 64-73)
// Receives baseColor as parameter, returns modified color.
vec4 applySolarShadowMap(vec4 baseColor) {
    // Out of bounds → return original color (not vec4(1.0))
    // Ported from: itwinjs-core SolarShadowMapping.ts line 69-70
    if (v_shadowPos.x < 0.0 || v_shadowPos.x > 1.0 || v_shadowPos.y < 0.0 || v_shadowPos.y > 1.0 || v_shadowPos.z < 0.0 || v_shadowPos.z > 1.0) {
        return baseColor;
    }
    float visible = 1.0;
    // Ported from: itwinjs-core SolarShadowMapping.ts line 71
    // Uses u_surfaceFlags[kSurfaceBitIndex_HasNormals] instead of u_hasNormals bool
    bool hasNormals = 0u != (u_surfaceFlags[kSurfaceBitIndex_HasNormals / 32] & uint(1 << (kSurfaceBitIndex_HasNormals % 32)));
    if (hasNormals && dot(v_normal, u_sunDir) < 0.0) {
        visible = 0.0;  // back face gets full shadow
    } else {
        visible = shadowMapEVSM(v_shadowPos);
    }
    // Ported from: itwinjs-core SolarShadowMapping.ts line 72
    // Multiplies shadow into baseColor (not replaces it)
    return vec4(baseColor.rgb * mix(u_shadowParams.rgb, vec3(1.0), visible), baseColor.a);
}
)glsl";

END_DQ_RENDER_NAMESPACE
