// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Animation displacement shader helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Animation.ts
//
// Provides GLSL functions for vertex animation displacement via LUT texture.
// The animation LUT stores per-vertex displacement data for multiple frames.
// The shader interpolates between frames based on the analysis fraction.
//
// M-W W-a5 归位：isSurface 半（normals/params 段）补齐；配套基建=
// AuxChannelTable 打包器（W-a3）+ Mesh auxChannels 链（W-a4）+ AuxChannelLUT
// 纹理（W-a4）+ TargetImpl analysisStyle/analysisFraction（M-U U-e 已在位）。
#pragma once

#include "ShaderBindings.h" // wireAnimLUT, wireAnimLUTParams, etc.
#include "ShaderBuilder.h"
#include "shader/DecodeShaders.h" // kDecodeUint16
#include "shader/SurfaceNormalShaders.h" // kOctDecodeNormal（isSurface 半）

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

// --- isSurface 半（W-a5 归位——Animation.ts:256-303）---

// Ported from: Animation.ts:89-94 computeAnimationFrameNormal（octDecodeNormal 消费
// 归位后 16 位参考格式——OctEncodedNormal.ts:62）。
inline constexpr char const* kComputeAnimationFrameNormal = R"(
vec3 computeAnimationFrameNormal(float frameIndex) {
  vec2 enc = sampleAnimVec2(g_vertexLUTIndex, frameIndex);
  return octDecodeNormal(enc);
}
)";

// Ported from: Animation.ts:96-106 computeAnimationNormal。
inline constexpr char const* kComputeAnimationNormal = R"(
vec3 computeAnimationNormal(float frameIndex0, float frameIndex1, float fraction) {
  vec3 normal = computeAnimationFrameNormal(frameIndex0);
  if (fraction > 0.0) {
    vec3 normal1 = computeAnimationFrameNormal(frameIndex1);
    normal += fraction * (normal1 - normal);
    }

  return normal;
}
)";

// Ported from: Animation.ts:108-113 computeAnimationFrameParam。
inline constexpr char const* kComputeAnimationFrameParam = R"(
float computeAnimationFrameParam(float frameIndex, float origin, float scale) {
  vec2 enc = sampleAnimVec2(g_vertexLUTIndex, frameIndex);
  return clamp((origin + scale * decodeUInt16(enc)), 0.0, 1.0);
}
)";

// Ported from: Animation.ts:115-125 computeAnimationParam。
inline constexpr char const* kComputeAnimationParam = R"(
vec2 computeAnimationParam(float frameIndex0, float frameIndex1, float fraction, float origin, float scale) {
  float param = computeAnimationFrameParam(frameIndex0, origin, scale);
  if (fraction > 0.0) {
    float param1 = computeAnimationFrameParam(frameIndex1, origin, scale);
    param += fraction * (param1 - param);
  }

  return vec2(.5, param);
}
)";

// ---------------------------------------------------------------------------
// addAnimation — wire animation displacement into a vertex ShaderBuilder
// Ported from: itwinjs-core Animation.ts addAnimation() (line 187-305)
// ---------------------------------------------------------------------------
// isSurface（参考 :187 形参）：true（Surface）→ 追加 normals/params 两段
// （u_animNormalParams/u_animScalarParams/u_animScalarQParams + octDecodeNormal +
// computeAnimationFrameNormal/Normal + computeAnimationFrameParam/Param）；
// false（Edge）→ 仅位移段（Edge.ts:275 的 `addAnimation(vert, false)`）。
inline void addAnimation(ShaderBuilder& vert, bool isSurface)
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

    // Normal and param（Animation.ts:257 isSurface 门）
    if (isSurface) {
        vert.addFunction(std::string(kOctDecodeNormal));  // Surface.ts:383-394（addFunction 去重）
        vert.addFunction(std::string(kComputeAnimationFrameNormal));
        vert.addFunction(std::string(kComputeAnimationNormal));
        vert.addFunction(std::string(kComputeAnimationFrameParam));
        vert.addFunction(std::string(kComputeAnimationParam));

        wireAnimNormalParams(vert);
        wireAnimScalarParams(vert);
        wireAnimScalarQParams(vert);
    }
}

END_DQ_RENDER_NAMESPACE
