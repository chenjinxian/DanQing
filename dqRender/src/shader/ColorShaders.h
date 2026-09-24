// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Color shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Color.ts
//
// GLSL functions for monochrome/color conversion used by surface rendering.
// Provides vertex-shader element color lookup from LUT texture and
// fragment-shader base color passthrough. Supports instanced geometry
// with per-instance color override.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Color — computeElementColor (quantized position)
// ---------------------------------------------------------------------------
// Computes element color from the color LUT for quantized-vertex geometry.
// Color table is appended to vertex data. The index is computed from
// g_vertLutData1.zw (quantized vertex data).
// NB: Color in color table has pre-multiplied alpha — revert it.
// Ported from: itwinjs-core Color.ts getComputeElementColor (quantized=true)
static char const* kColorComputeElementColorQuantized = R"glsl(
  float colorTableStart = u_vertParams.z * u_vertParams.w; // num rgba per-vertex times num vertices
  float colorIndex = decodeUInt16(g_vertLutData1.zw);
  vec2 tc = computeLUTCoords(colorTableStart+colorIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 lutColor = TEXTURE(u_vertLUT, tc);
  lutColor.rgb /= max(0.0001, lutColor.a);
  vec4 color = (u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color);
)glsl";

// ---------------------------------------------------------------------------
// Color — computeElementColor (unquantized position)
// ---------------------------------------------------------------------------
// Computes element color from the color LUT for unquantized-vertex geometry.
// Color table is appended to vertex data. The index is computed from
// g_vertLutData4.xy (unquantized vertex data).
// NB: Color in color table has pre-multiplied alpha — revert it.
// Ported from: itwinjs-core Color.ts getComputeElementColor (quantized=false)
static char const* kColorComputeElementColorUnquantized = R"glsl(
  float colorTableStart = u_vertParams.z * u_vertParams.w; // num rgba per-vertex times num vertices
  float colorIndex = decodeUInt16(g_vertLutData4.xy);
  vec2 tc = computeLUTCoords(colorTableStart+colorIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 lutColor = TEXTURE(u_vertLUT, tc);
  lutColor.rgb /= max(0.0001, lutColor.a);
  vec4 color = (u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color);
)glsl";

// ---------------------------------------------------------------------------
// Color — returnColor
// ---------------------------------------------------------------------------
// Simple return statement for the color varying.
// Ported from: itwinjs-core Color.ts returnColor
static char const* kColorReturnColor = R"glsl(
  return color;
)glsl";

// ---------------------------------------------------------------------------
// Color — applyInstanceColor
// ---------------------------------------------------------------------------
// Blends per-instance RGBA color into the computed element color.
// Uses u_applyInstanceColor uniform to control mix factor and
// extractInstanceBit to selectively apply RGB and Alpha channels.
// Ported from: itwinjs-core Color.ts applyInstanceColor
static char const* kColorApplyInstanceColor = R"glsl(
  color.rgb = mix(color.rgb, a_instanceRgba.rgb / 255.0, u_applyInstanceColor * extractInstanceBit(kOvrBit_Rgb));
  color.a = mix(color.a, a_instanceRgba.a / 255.0, u_applyInstanceColor * extractInstanceBit(kOvrBit_Alpha));
)glsl";

// ---------------------------------------------------------------------------
// Color — computeBaseColor (fragment)
// ---------------------------------------------------------------------------
// Fragment shader passthrough: returns the interpolated vertex color.
// Ported from: itwinjs-core Color.ts computeBaseColor (fragment)
static char const* kColorComputeBaseColorFrag = R"glsl(
return v_color;
)glsl";

// ---------------------------------------------------------------------------
// Color — full vertex computeColor (quantized, no instancing)
// ---------------------------------------------------------------------------
// Complete vertex shader color computation for quantized geometry without
// instancing: compute element color from LUT, return directly.
static char const* kColorComputeVertexColorQuantized = R"glsl(
  float colorTableStart = u_vertParams.z * u_vertParams.w;
  float colorIndex = decodeUInt16(g_vertLutData1.zw);
  vec2 tc = computeLUTCoords(colorTableStart+colorIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 lutColor = TEXTURE(u_vertLUT, tc);
  lutColor.rgb /= max(0.0001, lutColor.a);
  vec4 color = (u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color);
  return color;
)glsl";

// ---------------------------------------------------------------------------
// Color — full vertex computeColor (unquantized, no instancing)
// ---------------------------------------------------------------------------
// Complete vertex shader color computation for unquantized geometry without
// instancing: compute element color from LUT, return directly.
static char const* kColorComputeVertexColorUnquantized = R"glsl(
  float colorTableStart = u_vertParams.z * u_vertParams.w;
  float colorIndex = decodeUInt16(g_vertLutData4.xy);
  vec2 tc = computeLUTCoords(colorTableStart+colorIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 lutColor = TEXTURE(u_vertLUT, tc);
  lutColor.rgb /= max(0.0001, lutColor.a);
  vec4 color = (u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color);
  return color;
)glsl";

// ---------------------------------------------------------------------------
// Color — full vertex computeColor (quantized, with instancing)
// ---------------------------------------------------------------------------
// Complete vertex shader color computation for quantized geometry with
// per-instance color blending.
static char const* kColorComputeVertexColorQuantizedInstanced = R"glsl(
  float colorTableStart = u_vertParams.z * u_vertParams.w;
  float colorIndex = decodeUInt16(g_vertLutData1.zw);
  vec2 tc = computeLUTCoords(colorTableStart+colorIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 lutColor = TEXTURE(u_vertLUT, tc);
  lutColor.rgb /= max(0.0001, lutColor.a);
  vec4 color = (u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color);
  color.rgb = mix(color.rgb, a_instanceRgba.rgb / 255.0, u_applyInstanceColor * extractInstanceBit(kOvrBit_Rgb));
  color.a = mix(color.a, a_instanceRgba.a / 255.0, u_applyInstanceColor * extractInstanceBit(kOvrBit_Alpha));
  return color;
)glsl";

// ---------------------------------------------------------------------------
// Color — full vertex computeColor (unquantized, with instancing)
// ---------------------------------------------------------------------------
// Complete vertex shader color computation for unquantized geometry with
// per-instance color blending.
static char const* kColorComputeVertexColorUnquantizedInstanced = R"glsl(
  float colorTableStart = u_vertParams.z * u_vertParams.w;
  float colorIndex = decodeUInt16(g_vertLutData4.xy);
  vec2 tc = computeLUTCoords(colorTableStart+colorIndex, u_vertParams.xy, g_vert_center, 1.0);
  vec4 lutColor = TEXTURE(u_vertLUT, tc);
  lutColor.rgb /= max(0.0001, lutColor.a);
  vec4 color = (u_shaderFlags[kShaderBit_NonUniformColor] ? lutColor : u_color);
  color.rgb = mix(color.rgb, a_instanceRgba.rgb / 255.0, u_applyInstanceColor * extractInstanceBit(kOvrBit_Rgb));
  color.a = mix(color.a, a_instanceRgba.a / 255.0, u_applyInstanceColor * extractInstanceBit(kOvrBit_Alpha));
  return color;
)glsl";

END_DQ_RENDER_NAMESPACE
