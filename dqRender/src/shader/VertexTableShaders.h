// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Vertex table (LUT) shader constants
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//              addPositionFromLUT() (line 215-255)
//              computeVertexPositionFromLUT (line 35-41)
//              computeUnquantizedPosition (line 43-56)
//              initializeVertLUTCoords (line 20-23)
//              pre-read initializers (line 200-212)
//
// The VertexLUT system stores per-vertex data (position, color, feature,
// material, normal, UV) in an RGBA byte texture.  The shader reads texels
// at computed coordinates to decode vertex attributes.
#pragma once

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLSL — unquantizePosition: decode quantized position
// Ported from: itwinjs-core Vertex.ts line 27
// ---------------------------------------------------------------------------
inline constexpr std::string_view kUnquantizePosition = R"(
vec4 unquantizePosition(vec3 pos, vec3 origin, vec3 scale) {
  return vec4(origin + scale * pos, 1.0);
}
)";

// ---------------------------------------------------------------------------
// GLSL — computeVertexPositionFromLUT: decode quantized position from LUT
// Ported from: itwinjs-core Vertex.ts line 35-41
// Reads 16-bit quantized XYZ from g_vertLutData0/1, feature+material from
// g_vertLutData2.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeVertexPositionFromLUT = R"(
vec4 computeVertexPosition(vec3 encodedIndex) {
  vec3 qpos = vec3(decodeUInt16(g_vertLutData0.xy), decodeUInt16(g_vertLutData0.zw), decodeUInt16(g_vertLutData1.xy));
  g_featureAndMaterialIndex = g_vertLutData2;
  return unquantizePosition(qpos, u_qOrigin, u_qScale);
}
)";

// ---------------------------------------------------------------------------
// GLSL — computeUnquantizedPosition: decode 32-bit float position from LUT
// Ported from: itwinjs-core Vertex.ts line 43-56
// Reassembles 3 IEEE floats from bytes packed across g_vertLutData0-3.
// g_featureAndMaterialIndex is extracted from the .w channel of each texel.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeUnquantizedPositionFromLUT = R"(
vec4 computeVertexPosition(vec3 encodedIndex) {
  uvec3 vux = uvec3(g_vertLutData0.xyz);
  g_featureAndMaterialIndex.x = g_vertLutData0.w;
  uvec3 vuy = uvec3(g_vertLutData1.xyz);
  g_featureAndMaterialIndex.y = g_vertLutData1.w;
  uvec3 vuz = uvec3(g_vertLutData2.xyz);
  g_featureAndMaterialIndex.z = g_vertLutData2.w;
  uvec3 vuw = uvec3(g_vertLutData3.xyz);
  g_featureAndMaterialIndex.w = g_vertLutData3.w;
  uvec3 u = (vuw << 24) | (vuz << 16) | (vuy << 8) | vux;
  return vec4(uintBitsToFloat(u), 1.0);
}
)";

// ---------------------------------------------------------------------------
// GLSL — pre-read quantized vertex data (3 or 4 RGBA texels)
// Ported from: itwinjs-core Vertex.ts line 200-204
// Reads the first 3 RGBA values unconditionally, then conditionally reads
// a 4th if numRgbaPerVertex > 3.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPreReadVertexDataQuantized = R"(
  vec2 tc = g_vertexBaseCoords;
  g_vertLutData0 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  tc.x += g_vert_stepX;
  g_vertLutData1 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  tc.x += g_vert_stepX;
  g_vertLutData2 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  if (3.0 < u_vertParams.z) {
    tc.x += g_vert_stepX;
    g_vertLutData3 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  }
)";

// ---------------------------------------------------------------------------
// GLSL — pre-read unquantized vertex data (5 or 6 RGBA texels)
// Ported from: itwinjs-core Vertex.ts line 206-212
// Reads the first 5 RGBA values unconditionally, then conditionally reads
// a 6th if numRgbaPerVertex > 5.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPreReadVertexDataUnquantized = R"(
  vec2 tc = g_vertexBaseCoords;
  g_vertLutData0 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  tc.x += g_vert_stepX;
  g_vertLutData1 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  tc.x += g_vert_stepX;
  g_vertLutData2 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  tc.x += g_vert_stepX;
  g_vertLutData3 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  tc.x += g_vert_stepX;
  g_vertLutData4 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  if (5.0 < u_vertParams.z) {
    tc.x += g_vert_stepX;
    g_vertLutData5 = floor(TEXTURE(u_vertLUT, tc) * 255.0 + 0.5);
  }
)";

// ---------------------------------------------------------------------------
// GLSL — compute_vert_coords: per-LUT coordinate function
// Ported from: itwinjs-core LookupTable.ts computeCoordsTemplate
// Maps a vertex index to 2D texture coordinates in the vertex LUT.
// The multiplier is u_vertParams.z (numRgbaPerVertex).
// ---------------------------------------------------------------------------
inline constexpr std::string_view kComputeVertCoords = R"(
vec2 compute_vert_coords(float index) {
  return computeLUTCoords(index, u_vertParams.xy, g_vert_center, u_vertParams.z);
}
)";

// ---------------------------------------------------------------------------
// GLSL — Lookup table step/center initializer
// Ported from: itwinjs-core LookupTable.ts initializerTemplate
// Sets g_vert_stepX and g_vert_center from u_vertParams.xy.
// ---------------------------------------------------------------------------
inline constexpr std::string_view kVertLutInitTemplate = R"(
  g_vert_stepX = 1.0 / u_vertParams.x;
  float vert_stepY = 1.0 / u_vertParams.y;
  g_vert_center = vec2(0.5 * g_vert_stepX, 0.5 * vert_stepY);
)";

END_DQ_RENDER_NAMESPACE
