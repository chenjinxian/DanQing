// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Decode shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Decode.ts
//
// GLSL utility functions for decoding/encoding feature IDs, depth values,
// and packed byte data from textures. These are compositional functions
// added to other shaders via ShaderBuilder, not standalone programs.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// decodeUInt16 — decode a 16-bit unsigned integer from a vec2
// ---------------------------------------------------------------------------
// Unpacks two bytes (low, high) into a float in [0..65535].
// Ported from: itwinjs-core Decode.ts decodeUint16
static char const* kDecodeUint16 = R"glsl(
float decodeUInt16(vec2 v) {
  return dot(v, vec2(1.0, 256.0)); // v.x | (v.y << 8)
}
)glsl";

// ---------------------------------------------------------------------------
// decodeUInt24 — decode a 24-bit unsigned integer from a vec3
// ---------------------------------------------------------------------------
// Unpacks three bytes (low, mid, high) into a float in [0..16777215].
// Ported from: itwinjs-core Decode.ts decodeUint24
static char const* kDecodeUint24 = R"glsl(
float decodeUInt24(vec3 v) {
  return dot(v, vec3(1.0, 256.0, 256.0*256.0)); // v.x | (v.y << 8) | (v.z << 16)
}
)glsl";

// ---------------------------------------------------------------------------
// unquantize3d — unquantize a 3D position from integer to world space
// ---------------------------------------------------------------------------
// Reverses quantization: returns origin + scale * qpos.
// Ported from: itwinjs-core Decode.ts unquantize3d
static char const* kUnquantize3d = R"glsl(
vec3 unquantize3d(vec3 qpos, vec3 origin, vec3 scale) { return origin + scale * qpos; }
)glsl";

// NOTE: kDecodeDepthRgb and kEncodeDepthRgb are defined in
// FeatureSymbologyShaders.h (canonical home, inline constexpr std::string_view).
// Do NOT duplicate here — causes ODR violations when both headers are included.

// ---------------------------------------------------------------------------
// pack2Bytes — pack 2 floats [0..255] into a single float
// ---------------------------------------------------------------------------
// Packs two byte-range values into v.x | (v.y << 8).
// Ported from: itwinjs-core Decode.ts pack2Bytes
static char const* kPack2Bytes = R"glsl(
float pack2Bytes(vec2 v) {
  return v.x + (v.y * 256.0);
}
)glsl";

// NOTE: kUnpack2Bytes and kUnpackAndNormalize2Bytes are defined in
// SurfaceMaterialShaders.h (canonical home, inline constexpr std::string_view).
// kUnquantize2d is defined in SurfaceTextureShaders.h.
// Do NOT duplicate here — causes ODR violations when both headers are included.

// ---------------------------------------------------------------------------
// decodeFloat32 — decode IEEE 32-bit float from RGBA bytes
// ---------------------------------------------------------------------------
// Given an IEEE 32-bit float stuffed into an RGBA unsigned byte texture,
// extract the float. Input vec4 components are in [0..255].
// From https://github.com/CesiumGS/cesium/blob/main/Source/Shaders/Builtin/Functions/unpackFloat.glsl
// Ported from: itwinjs-core Decode.ts decodeFloat32
static char const* kDecodeFloat32 = R"glsl(
float decodeFloat32(vec4 packedFloat) {
  float sign = 1.0 - step(128.0, packedFloat[3]) * 2.0;
  float exponent = 2.0 * mod(packedFloat[3], 128.0) + step(128.0, packedFloat[2]) - 127.0;
  if (exponent == -127.0)
    return 0.0;

  float mantissa = mod(packedFloat[2], 128.0) * 65536.0 + packedFloat[1] * 256.0 + packedFloat[0] + float(0x800000);
  float result = sign * exp2(exponent - 23.0) * mantissa;
  return result;
}
)glsl";

// ---------------------------------------------------------------------------
// decode3Float32 — decode 3 packed IEEE floats from interleaved vec3[4]
// ---------------------------------------------------------------------------
// Expects an array of 4 vec3s, where each vec3 contains a slice of all 3
// packed floats in .xyz. pf0 is in [0].x, pf1 in [0].y, pf2 in [0].z.
// Ported from: itwinjs-core Decode.ts decode3Float32
static char const* kDecode3Float32 = R"glsl(
vec3 decode3Float32(vec3 packedFloat[4]) {
  vec3 sign = 1.0 - step(128.0, packedFloat[3].xyz) * 2.0;
  vec3 exponent = 2.0 * mod(packedFloat[3].xyz, 128.0) + step(128.0, packedFloat[2].xyz) - 127.0;
  vec3 zeroFlag = vec3(notEqual(vec3(-127.0), exponent));
  vec3 mantissa = mod(packedFloat[2].xyz, 128.0) * 65536.0 + packedFloat[1].xyz * 256.0 + packedFloat[0].xyz + float(0x800000);
  vec3 result = sign * exp2(exponent - 23.0) * mantissa * zeroFlag;
  return result;
}
)glsl";

END_DQ_RENDER_NAMESPACE
