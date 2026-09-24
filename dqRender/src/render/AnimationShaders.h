// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Animation displacement shader helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Animation.ts
//
// Provides GLSL functions for vertex animation displacement via LUT texture.
// The animation LUT stores per-vertex displacement data for multiple frames.
// The shader interpolates between frames based on the analysis fraction.
//
// NOTE: The full animation infrastructure (AuxChannelTable, animation LUT
// texture creation, analysis style integration) is not yet implemented.
// This file provides the GLSL-side code; uniform bindings use nullptr
// (uploaded by draw loop when animation is active).
#pragma once

#include "ShaderBindings.h" // wireAnimLUT, wireAnimLUTParams, etc.
#include "ShaderBuilder.h"
#include "shader/DecodeShaders.h" // kDecodeUint16

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Animation GLSL functions
// Ported from: itwinjs-core Animation.ts
// ---------------------------------------------------------------------------

// LUT initialization
inline constexpr char const* kAnimInitialize = R"(
  g_anim_step = vec2(1.0) / u_animLUTParams.xy;
  g_anim_center = g_anim_step * 0.5;
)";

// LUT coordinate computation
inline constexpr char const* kComputeAnimLUTCoords = R"(
vec3 computeAnimLUTCoords(float vertIndex, float frameIndex) {
  float baseIndex = (vertIndex * u_animLUTParams.z) + frameIndex;
  float halfIndex = baseIndex * 0.5;
  float index = floor(halfIndex);

  float epsilon = 0.5 / u_animLUTParams.x;
  float yId = floor(index / u_animLUTParams.x + epsilon);
  float xId = index - u_animLUTParams.x * yId;

  vec2 texCoord = g_anim_center + vec2(xId / u_animLUTParams.x, yId / u_animLUTParams.y);
  return vec3(texCoord, 2.0 * (halfIndex - index));
}
)";

// Sample 2 bytes at the specified index
inline constexpr char const* kSampleAnimVec2 = R"(
vec2 sampleAnimVec2(float vertIndex, float frameIndex) {
  vec3 tc = computeAnimLUTCoords(vertIndex, frameIndex);
  vec4 texel = floor(TEXTURE(u_animLUT, tc.xy) * 255.0 + 0.5);
  return texel.xy * (1.0 - tc.z) + texel.zw * tc.z;
}
)";

// Compute displacement for a single frame
inline constexpr char const* kComputeAnimationFrameDisplacement = R"(
vec3 computeAnimationFrameDisplacement(float vertIndex, float frameIndex, vec3 origin, vec3 scale) {
  vec3 tc = computeAnimLUTCoords(vertIndex, frameIndex);
  vec4 enc1 = floor(TEXTURE(u_animLUT, tc.xy) * 255.0 + 0.5);
  tc.x += g_anim_step.x;
  vec4 enc2 = floor(TEXTURE(u_animLUT, tc.xy) * 255.0 + 0.5);

  vec2 ex = enc1.xy * (1.0 - tc.z) + enc1.zw * tc.z;
  vec2 ey = enc1.zw * (1.0 - tc.z) + enc2.xy * tc.z;
  vec2 ez = enc2.xy * (1.0 - tc.z) + enc2.zw * tc.z;

  vec3 qpos = vec3(decodeUInt16(ex), decodeUInt16(ey), decodeUInt16(ez));
  return unquantizePosition(qpos, origin, scale).xyz;
}
)";

// Interpolate displacement between two frames
inline constexpr char const* kComputeAnimationDisplacement = R"(
vec3 computeAnimationDisplacement(float vertIndex, float frameIndex0, float frameIndex1, float fraction, vec3 origin, vec3 scale) {
  if (frameIndex0 < 0.0)
    return vec3(0.0, 0.0, 0.0);

  vec3 displacement = computeAnimationFrameDisplacement(vertIndex, frameIndex0, origin, scale);
  if (fraction > 0.0) {
    vec3 displacement1 = computeAnimationFrameDisplacement(vertIndex, frameIndex1, origin, scale);
    displacement += fraction * (displacement1 - displacement);
  }

  return displacement;
}
)";

// AdjustRawPosition slot body: apply animation displacement
inline constexpr char const* kAnimAdjustRawPosition = R"(
  rawPos.xyz += computeAnimationDisplacement(g_vertexLUTIndex, u_animDispParams.x, u_animDispParams.y, u_animDispParams.z, u_qAnimDispOrigin, u_qAnimDispScale);
  return rawPos;
)";

// ---------------------------------------------------------------------------
// addAnimation — wire animation displacement into a vertex ShaderBuilder
// Ported from: itwinjs-core Animation.ts addAnimation() (line 187-304)
// ---------------------------------------------------------------------------
inline void addAnimation(ShaderBuilder& vert)
{
    // LUT globals
    vert.addGlobal("g_anim_step", VariableType::Vec2);
    vert.addGlobal("g_anim_center", VariableType::Vec2);
    vert.addGlobal("g_vertexLUTIndex", VariableType::Float);
    vert.addInitializer(kAnimInitialize);

    // LUT uniforms — wired to DrawParams animation state
    wireAnimLUT(vert);
    wireAnimLUTParams(vert);

    // Displacement uniforms — wired to DrawParams animation state
    wireAnimDispParams(vert);
    wireAnimDispScale(vert);
    wireAnimDispOrigin(vert);

    // GLSL functions
    vert.addFunction(std::string(kDecodeUint16));
    vert.addFunction(std::string(kComputeAnimLUTCoords));
    vert.addFunction(std::string(kSampleAnimVec2));
    vert.addFunction(std::string(kComputeAnimationFrameDisplacement));
    vert.addFunction(std::string(kComputeAnimationDisplacement));

    // AdjustRawPosition slot: apply displacement
    vert.setVertexComponent(VertexShaderComponent::AdjustRawPosition, kAnimAdjustRawPosition);
}

END_DQ_RENDER_NAMESPACE
