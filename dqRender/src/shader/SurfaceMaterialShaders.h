// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface material system shader constants
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addMaterial() (line 183-238) + material GLSL functions
//
// The material system provides:
// - Material color (RGB + alpha) with per-vertex override support
// - Material parameters (diffuse/specular weights, specular RGB, exponent)
// - Material atlas lookup from LUT texture
// - Texture weight blending in fragment shader
#pragma once

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL — unpack2Bytes: split a float into two byte components
// Ported from: itwinjs-core Decode.ts line 66-72
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUnpack2Bytes = R"(
vec2 unpack2Bytes(float f) {
  f = floor(f + 0.5);
  vec2 v;
  v.y = floor(f / 256.0);
  v.x = floor(f - v.y * 256.0);
  return v;
}
)";

// ---------------------------------------------------------------------------
// GLSL — unpackAndNormalize2Bytes: split + normalize to [0,1]
// Ported from: itwinjs-core Decode.ts line 74-76
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUnpackAndNormalize2Bytes = R"(
vec2 unpackAndNormalize2Bytes(float f) {
  return unpack2Bytes(f) / 255.0;
}
)";

// NOTE: kChooseVec3WithBitFlag used to be (re)defined here as std::string_view;
// it collided (ODR) with the char const* definition in CommonShaders.h when
// both headers were included in one TU. Deduped: the single canonical
// definition lives in CommonShaders.h (with its addChooseVec3WithBitFlagFunction
// helper). Ported from: itwinjs-core Common.ts line 25.

// ---------------------------------------------------------------------------
// GLSL — unpackFloat: unpack a float from 4 bytes with exponent
// Ported from: itwinjs-core Clipping.ts line 23
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUnpackFloat = R"(
float unpackFloat(vec4 v) {
  const float bias = 38.0;
  v = floor(v * 255.0 + 0.5);
  float temp = v.w / 2.0;
  float exponent = floor(temp);
  float sign = (temp - exponent) * 2.0;
  exponent = exponent - bias;
  sign = -(sign * 2.0 - 1.0);
  float unpacked = dot(sign * v.xyz, vec3(1.0 / 256.0, 1.0 / 65536.0, 1.0 / 16777216.0));
  return unpacked * pow(10.0, exponent);
}
)";

// ---------------------------------------------------------------------------
// GLSL — decodeMaterialColor: unpack material RGBA into mat_rgb/mat_alpha
// Ported from: itwinjs-core Surface.ts line 126-130
// ---------------------------------------------------------------------------
inline constexpr std::string_view kDecodeMaterialColor = R"(
void decodeMaterialColor(vec4 rgba) {
  mat_rgb = vec4(rgba.rgb, float(rgba.r >= 0.0));
  mat_alpha = vec2(rgba.a, float(rgba.a >= 0.0));
}
)";

// ---------------------------------------------------------------------------
// GLSL — decodeMaterialParams: unpack fragment material parameters
// Ported from: itwinjs-core Surface.ts line 114-124
// ---------------------------------------------------------------------------
inline constexpr std::string_view kDecodeFragMaterialParams = R"(
void decodeMaterialParams(vec4 params) {
  mat_weights = unpackAndNormalize2Bytes(params.x);
  vec2 texAndSpecR = unpackAndNormalize2Bytes(params.y);
  mat_texture_weight = texAndSpecR.x;
  vec2 specGB = unpackAndNormalize2Bytes(params.z);
  mat_specular = vec4(texAndSpecR.y, specGB, params.w);
}
)";

// ---------------------------------------------------------------------------
// GLSL — computeMaterial: vertex ComputeMaterial component
// Ported from: itwinjs-core Surface.ts line 169-175
// Reads material from atlas or uniform
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeMaterial = R"(
  if (u_surfaceFlags[kSurfaceBitIndex_HasMaterialAtlas]) {
    readMaterialAtlas();
  } else {
    decodeMaterialColor(u_materialColor);
    g_materialParams = u_materialParams;
  }
)";

// ---------------------------------------------------------------------------
// GLSL — computeMaterialInstanced: vertex ComputeMaterial for instanced geometry
// Ported from: itwinjs-core Surface.ts line 178-180
// Always reads from uniform (no atlas support for instanced)
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeMaterialInstanced = R"(
  decodeMaterialColor(u_materialColor);
  g_materialParams = u_materialParams;
)";

// ---------------------------------------------------------------------------
// GLSL — applyMaterialColor: vertex ApplyMaterialColor component
// Ported from: itwinjs-core Surface.ts line 91-96
// Blends material color with base color based on use_material flag
// ---------------------------------------------------------------------------
inline constexpr std::string_view kApplyMaterialColor = R"(
  float useMatColor = float(use_material);
  vec3 rgb = mix(baseColor.rgb, mat_rgb.rgb, useMatColor * mat_rgb.a);
  float a = mix(baseColor.a, mat_alpha.x, useMatColor * mat_alpha.y);
  return vec4(rgb, a);
)";

// ---------------------------------------------------------------------------
// GLSL — applyTextureWeight: fragment ApplyMaterialOverrides component
// Ported from: itwinjs-core Surface.ts line 103-111
// Blends texture texel with base color using mat_texture_weight
// ---------------------------------------------------------------------------
inline constexpr std::string_view kApplyTextureWeight = R"(
  bool applyTexture = !u_applyGlyphTex && isSurfaceBitSet(kSurfaceBit_HasTexture);
  float textureWeight = applyTexture ? mat_texture_weight : 0.0;
  vec3 rgb = mix(baseColor.rgb, g_surfaceTexel.rgb, textureWeight);
  rgb = chooseVec3WithBitFlag(rgb, baseColor.rgb, surfaceFlags, kSurfaceBit_OverrideRgb);
  float a = applyTexture ? baseColor.a * g_surfaceTexel.a : baseColor.a;
  return vec4(rgb, a);
)";

// ---------------------------------------------------------------------------
// GLSL — computeMaterialParams: computed varying for material parameters
// Ported from: itwinjs-core Surface.ts line 134-136
// Returns material params or defaults if material is disabled
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeMaterialParams = R"(
  const vec4 defaults = vec4(26265.0, 65535.0, 65535.0, 13.5);
  return use_material ? g_materialParams : defaults;
)";

// ---------------------------------------------------------------------------
// GLSL — readMaterialAtlas: read material from LUT texture atlas
// Ported from: itwinjs-core Surface.ts line 140-166
// Reads 4 LUT texels: RGBA, weights+flags, specular RGB, packed exponent
// ---------------------------------------------------------------------------
inline constexpr std::string_view kReadMaterialAtlas = R"(
void readMaterialAtlas() {
  float materialAtlasStart = u_vertParams.z * u_vertParams.w + u_numColors;
  float materialIndex = g_featureAndMaterialIndex.w * 4.0 + materialAtlasStart;

  vec2 tc = computeLUTCoords(materialIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 rgba = TEXTURE(u_vertLUT, tc);

  tc = computeLUTCoords(materialIndex + 1.0, u_vertParams.xy, g_vert_center, 1.0);
  vec4 weightsAndFlags = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);

  tc = computeLUTCoords(materialIndex + 2.0, u_vertParams.xy, g_vert_center, 1.0);
  vec3 specularRgb = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5).rgb;

  tc = computeLUTCoords(materialIndex + 3.0, u_vertParams.xy, g_vert_center, 1.0);
  vec4 packedSpecularExponent = TEXTURE(u_vertLUT, tc);

  float flags = weightsAndFlags.w;
  mat_rgb = vec4(rgba.rgb, float(flags == 1.0 || flags == 3.0));
  mat_alpha = vec2(rgba.a, float(flags == 2.0 || flags == 3.0));

  float specularExponent = unpackFloat(packedSpecularExponent);
  g_materialParams.x = weightsAndFlags.y + weightsAndFlags.z * 256.0;
  g_materialParams.y = 255.0 + specularRgb.r * 256.0;
  g_materialParams.z = specularRgb.g + specularRgb.b * 256.0;
  g_materialParams.w = specularExponent;
}
)";

END_DQ_RENDER_NAMESPACE
