// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Lookup table shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/LookupTable.ts
//
// LUT coordinate computation for vertex-based color lookups. Provides a
// generic computeLUTCoords function and per-LUT coordinate computation
// functions used by thematic display, feature symbology, and similar
// techniques that store color ramps in texture lookup tables.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// computeLUTCoords — generic LUT coordinate computation
// ---------------------------------------------------------------------------
// Maps a 1D index to 2D texture coordinates within a LUT texture.
// Uses epsilon-based mod precision fix to avoid mod(x,y)==y when x is
// a multiple of y.
// Parameters:
//   index      — linear index into the LUT
//   dimensions — (width, height) of the LUT texture in texels
//   center     — half-texel offset for texel-center sampling
//   mult       — multiplier (1.0 for direct index, larger for sub-entry sampling)
// Returns: vec2 texture coordinate in [0..1]
// Ported from: itwinjs-core LookupTable.ts computeLUTCoords
static char const* kLookupTableFunctions = R"glsl(
vec2 computeLUTCoords(float index, vec2 dimensions, vec2 center, float mult) {
  float baseIndex = index * mult;

  // Fix precision issues wherein mod(x,y) => y instead of 0 when x is multiple of y...
  float epsilon = 0.5 / dimensions.x;
  float yId = floor(baseIndex / dimensions.x + epsilon);
  float xId = baseIndex - dimensions.x * yId; // replaces mod()...

  return center + vec2(xId / dimensions.x, yId / dimensions.y);
}
)glsl";

// ---------------------------------------------------------------------------
// Lookup table initialization template
// ---------------------------------------------------------------------------
// Generates initialization code for a named LUT. Used by ShaderBuilder
// to produce per-LUT globals and initializer statements.
//
// Template parameters (replaced at code-generation time):
//   {LUTSTEPX}   — g_{name}_stepX global (1.0 / textureWidth)
//   {LUTSTEPY}   — {name}_stepY local (1.0 / textureHeight)
//   {LUTCENTER}  — g_{name}_center global (half-texel offset)
//   {LUTPARAMS}  — u_{name}Params uniform (vec2 with texture dimensions)
//
// Ported from: itwinjs-core LookupTable.ts initializerTemplate
static char const* kLookupTableInitTemplate = R"glsl(
  g_{LUTSTEPX} = 1.0 / {LUTPARAMS}.x;
  float {LUTSTEPY} = 1.0 / {LUTPARAMS}.y;
  {LUTCENTER} = vec2(0.5 * g_{LUTSTEPX}, 0.5 * {LUTSTEPY});
)glsl";

// ---------------------------------------------------------------------------
// Lookup table coordinate function template
// ---------------------------------------------------------------------------
// Generates a compute_{name}_coords(index) function for a specific LUT.
//
// Template parameters:
//   {LUTNAME} — LUT identifier (e.g., "thematic", "featureSymbology")
//   {MULT}    — multiplier constant (default "1.0")
//
// Ported from: itwinjs-core LookupTable.ts computeCoordsTemplate
static char const* kLookupTableCoordTemplate = R"glsl(
vec2 compute_{LUTNAME}_coords(float index) {
  return computeLUTCoords(index, u_{LUTNAME}Params.xy, g_{LUTNAME}_center, {MULT});
}
)glsl";

END_DQ_RENDER_NAMESPACE
