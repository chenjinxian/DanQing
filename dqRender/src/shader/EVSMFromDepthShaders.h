// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — EVSM from depth shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/EVSMFromDepth.ts
//
// GLSL functions for Exponential Variance Shadow Map (EVSM) generation
// from a depth texture. Reads the depth texture, converts it to EVSM
// values via warpDepth, then averages 4 texels down to 1 for downsampling.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// EVSMFromDepth — computeTexCoord (vertex)
// ---------------------------------------------------------------------------
// Converts NDC position [-1..1] to UV [0..1].
// Ported from: itwinjs-core EVSMFromDepth.ts computeTexCoord
static char const* kEVSMFromDepthComputeTexCoord = R"glsl(
v_texCoord = (rawPosition.xy + 1.0) * 0.5;
)glsl";

// ---------------------------------------------------------------------------
// EVSMFromDepth — computePosition (vertex)
// ---------------------------------------------------------------------------
// Passes through the raw position (viewport quad).
// Ported from: itwinjs-core EVSMFromDepth.ts computePosition
static char const* kEVSMFromDepthComputePosition = R"glsl(
return rawPos;
)glsl";

// ---------------------------------------------------------------------------
// EVSMFromDepth — computeEVSM (fragment)
// ---------------------------------------------------------------------------
// Reads 4 depth samples in a 2x2 pattern, warps each to EVSM space via
// warpDepth, and averages the positive-negative variance pairs.
// Ported from: itwinjs-core EVSMFromDepth.ts computeEVSM
static char const* kEVSMFromDepthComputeEVSM = R"glsl(
  const float sampleWeight = 0.25;
  vec4 average = vec4(0.0);
  vec2 tc = v_texCoord - u_stepSize * 0.5; // v_texCoord starts in between the 4 texels

  float depth = TEXTURE(u_depthTexture, tc).r;
  vec2 vsmDepth = warpDepth(depth, u_evsmExponent);
  average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

  tc.x += u_stepSize.x;
  depth = TEXTURE(u_depthTexture, tc).r;
  vsmDepth = warpDepth(depth, u_evsmExponent);
  average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

  tc.y += u_stepSize.y;
  depth = TEXTURE(u_depthTexture, tc).r;
  vsmDepth = warpDepth(depth, u_evsmExponent);
  average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

  tc.x -= u_stepSize.x;
  depth = TEXTURE(u_depthTexture, tc).r;
  vsmDepth = warpDepth(depth, u_evsmExponent);
  average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

  return average;
)glsl";

// ---------------------------------------------------------------------------
// EVSMFromDepth — complete fragment shader
// ---------------------------------------------------------------------------
// Standalone fragment shader: samples depth texture, warps to EVSM, averages 4-to-1.
static char const* kEVSMFromDepthFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_depthTexture;
uniform vec2 u_stepSize;
uniform float u_evsmExponent;
out vec4 fragColor;

vec2 warpDepth(float depth, float exponent) {
    depth = 2.0 * depth - 1.0;
    float pos =  exp( exponent * depth);
    float neg = -exp(-exponent * depth);
    return vec2(pos, neg);
}

void main() {
    const float sampleWeight = 0.25;
    vec4 average = vec4(0.0);
    vec2 tc = v_texCoord - u_stepSize * 0.5;

    float depth = texture(u_depthTexture, tc).r;
    vec2 vsmDepth = warpDepth(depth, u_evsmExponent);
    average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

    tc.x += u_stepSize.x;
    depth = texture(u_depthTexture, tc).r;
    vsmDepth = warpDepth(depth, u_evsmExponent);
    average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

    tc.y += u_stepSize.y;
    depth = texture(u_depthTexture, tc).r;
    vsmDepth = warpDepth(depth, u_evsmExponent);
    average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

    tc.x -= u_stepSize.x;
    depth = texture(u_depthTexture, tc).r;
    vsmDepth = warpDepth(depth, u_evsmExponent);
    average += sampleWeight * vec4(vsmDepth.xy, vsmDepth.xy * vsmDepth.xy);

    fragColor = average;
}
)glsl";

END_DQ_RENDER_NAMESPACE
