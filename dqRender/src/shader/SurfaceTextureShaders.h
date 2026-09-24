// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface texture pipeline shader constants
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addTexture() (line 571-687) + texture GLSL functions
#pragma once

#include <cstdlib>
#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL — unquantize2d: decode quantized 2D position
// Ported from: itwinjs-core Decode.ts line 31-34
// params.xy = origin, params.zw = scale
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUnquantize2d = R"(
vec2 unquantize2d(vec2 qpos, vec4 params) { return params.xy + params.zw * qpos; }
)";

// ---------------------------------------------------------------------------
// GLSL — constantLodTextureLookup: constant-LOD texture sampling
// Ported from: itwinjs-core Surface.ts line 49-75
// Uses v_uvCustom (vec3: xy=UV, z=depth) for LOD computation
// ---------------------------------------------------------------------------
inline constexpr std::string_view kConstantLodTextureLookup = R"(
vec4 constantLodTextureLookup(sampler2D textureSampler) {
  float logDepth = log2(v_uvCustom.z);
  float f = fract(logDepth);
  float p = floor(logDepth);
  float p1, p2;
  if (0u == (uint(p) & 1u)) {
    p1 = p;
    p2 = p + 1.0;
  } else {
    p1 = p + 1.0;
    p2 = p;
    f = 1.0 - f;
  }
  vec2 tc1 = v_uvCustom.xy / clamp(pow(2.0, p1), float(u_constantLodFParams.x), float(u_constantLodFParams.y)) * u_constantLodFParams.z;
  vec2 tc2 = v_uvCustom.xy / clamp(pow(2.0, p2), float(u_constantLodFParams.x), float(u_constantLodFParams.y)) * u_constantLodFParams.z;
  return mix(TEXTURE(textureSampler, tc1), TEXTURE(textureSampler, tc2), f);
}
)";

// ---------------------------------------------------------------------------
// GLSL — sampleSurfaceTexture: sample the surface texture
// Ported from: itwinjs-core Surface.ts line 78-89
// Returns white if no texture, otherwise samples with constant LOD or direct UV
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUvDebugSampleSurfaceTexture = R"(
vec4 sampleSurfaceTexture() {
  vec4 clr;
  if (!isSurfaceBitSet(kSurfaceBit_HasTexture))
    clr = vec4(1.0, 1.0, 1.0, 1.0);
  else
    clr = vec4(v_texCoord.x, v_texCoord.y, 0.0, 1.0);
  return clr;
}
)";

inline constexpr std::string_view kSampleSurfaceTexture = R"(
vec4 sampleSurfaceTexture() {
  vec4 clr;
  if (!isSurfaceBitSet(kSurfaceBit_HasTexture))
    clr = vec4(1.0, 1.0, 1.0, 1.0);
  else if (u_surfaceFlags[kSurfaceBitIndex_UseConstantLodTextureMapping])
    clr = constantLodTextureLookup(s_texture);
  else
    clr = TEXTURE(s_texture, v_texCoord);
  return clr;
}
)";

// ---------------------------------------------------------------------------
// GLSL — getSurfaceColor: return vertex color as surface color
// Ported from: itwinjs-core Surface.ts line 478-480
// ---------------------------------------------------------------------------
inline constexpr std::string_view kGetSurfaceColor = R"(
vec4 getSurfaceColor() { return v_color; }
)";

// ---------------------------------------------------------------------------
// GLSL — computeBaseColor: sample texture + mix with surface color
// Ported from: itwinjs-core Surface.ts line 486-502
// Sets g_surfaceTexel global, handles glyph texture and white-on-white
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeBaseColor = R"(
  g_surfaceTexel = sampleSurfaceTexture();
  vec4 surfaceColor = getSurfaceColor();
  if (!u_applyGlyphTex)
    return surfaceColor;
  const vec3 white = vec3(1.0);
  const vec3 epsilon = vec3(0.0001);
  vec3 almostWhite = white - epsilon;
  bvec3 isAlmostWhite = greaterThan(surfaceColor.rgb, almostWhite);
  surfaceColor.rgb = (u_reverseWhiteOnWhite && isAlmostWhite.r && isAlmostWhite.g && isAlmostWhite.b ? vec3(0.0, 0.0, 0.0) : surfaceColor.rgb);
  return vec4(surfaceColor.rgb * g_surfaceTexel.rgb, g_surfaceTexel.a * surfaceColor.a);
)";

// ---------------------------------------------------------------------------
// GLSL — computeTexCoord (quantized variant)
// Ported from: itwinjs-core Surface.ts getComputeTexCoord(true) (line 460-467)
// Reads quantized UVs from g_vertLutData3, decodes uint16 pairs
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeTexCoordQuantized = R"(
  vec4 rgba = g_vertLutData3;
  vec2 qcoords = vec2(decodeUInt16(rgba.xy), decodeUInt16(rgba.zw));
  return chooseVec2With2BitFlags(vec2(0.0), unquantize2d(qcoords, u_qTexCoordParams), surfaceFlags, kSurfaceBit_HasTexture, kSurfaceBit_HasNormalMap);
)";

// ---------------------------------------------------------------------------
// GLSL — computeTexCoord (non-quantized variant)
// Ported from: itwinjs-core Surface.ts getComputeTexCoord(false) (line 460-467)
// Reads quantized UVs from g_vertLutData4, decodes uint16 pairs
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeTexCoordNonQuantized = R"(
  vec4 rgba = g_vertLutData4;
  vec2 qcoords = vec2(decodeUInt16(rgba.xy), decodeUInt16(rgba.zw));
  return chooseVec2With2BitFlags(vec2(0.0), unquantize2d(qcoords, u_qTexCoordParams), surfaceFlags, kSurfaceBit_HasTexture, kSurfaceBit_HasNormalMap);
)";

END_DQ_RENDER_NAMESPACE
