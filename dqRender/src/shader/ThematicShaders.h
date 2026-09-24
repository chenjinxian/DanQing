// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Thematic.ts
//
// Complete thematic display GLSL: height/slope/hillshade/IDW sensor modes,
// gradient modes (smooth/stepped/isolines), and sensor interpolation.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Display mode constants
// Ported from: itwinjs-core ThematicDisplayMode enum
// ---------------------------------------------------------------------------
inline constexpr char const* kThematicDisplayModeConstants = R"(
const float kThematicDisplayMode_Height = 0.0;
const float kThematicDisplayMode_InverseDistanceWeightedSensors = 1.0;
const float kThematicDisplayMode_Slope = 2.0;
const float kThematicDisplayMode_HillShade = 3.0;
)";

// ---------------------------------------------------------------------------
// Gradient mode constants
// Ported from: itwinjs-core ThematicGradientMode enum
// ---------------------------------------------------------------------------
inline constexpr char const* kThematicGradientModeConstants = R"(
const float kThematicGradientMode_Smooth = 0.0;
const float kThematicGradientMode_Stepped = 1.0;
const float kThematicGradientMode_SteppedWithDelimiter = 2.0;
const float kThematicGradientMode_IsoLines = 3.0;
)";

// ---------------------------------------------------------------------------
// findFractionalPositionOnLine — vertex function for height mode
// Ported from: itwinjs-core Thematic.ts line 228
// ---------------------------------------------------------------------------
inline constexpr char const* kFindFractionalPositionOnLine = R"(
float findFractionalPositionOnLine(vec3 a, vec3 b, vec3 c) {
  float abDist = distance(a, b);
  return dot(b - a, c - a) / (abDist * abDist);
}
)";

// ---------------------------------------------------------------------------
// _universal_fwidth — fwidth wrapper for portability
// Ported from: itwinjs-core Thematic.ts line 329
// ---------------------------------------------------------------------------
inline constexpr char const* kUniversalFwidth = R"(
float _universal_fwidth(float coord) { return fwidth(coord); }
)";

// ---------------------------------------------------------------------------
// getColor — sample gradient texture
// Ported from: itwinjs-core Thematic.ts line 332
// ---------------------------------------------------------------------------
inline constexpr char const* kThematicGetColor = R"(
vec4 getColor(float ndx) {
  if (ndx < 0.0 || ndx > 1.0)
    return u_marginColor;
  return TEXTURE(s_texture, vec2(0.0, ndx));
}
)";

// ---------------------------------------------------------------------------
// getSensor — sample sensor texture
// Ported from: itwinjs-core Thematic.ts line 330
// ---------------------------------------------------------------------------
inline constexpr char const* kThematicGetSensor = R"(
vec4 getSensor(int index) {
  float x = 0.5;
  float y = (float(index) + 0.5) / float(u_numSensors);
  return TEXTURE(s_sensorSampler, vec2(x, y));
}
)";

// ---------------------------------------------------------------------------
// getIsoLineColor — isoline-aware gradient sampling
// Ported from: itwinjs-core Thematic.ts line 333
// ---------------------------------------------------------------------------
inline constexpr char const* kThematicGetIsoLineColor = R"(
vec4 getIsoLineColor(float ndx, float stepCount) {
  if (ndx < 0.01 || ndx > 0.99)
    return u_marginColor;
  ndx += 0.5 / stepCount;
  return TEXTURE(s_texture, vec2(0.0, ndx));
}
)";

// ---------------------------------------------------------------------------
// computeThematicIndex — vertex varying computation for height/hillshade modes
// Ported from: itwinjs-core Thematic.ts getComputeThematicIndex()
// ---------------------------------------------------------------------------
inline constexpr char const* kComputeThematicIndex = R"(
  if (kThematicDisplayMode_Height == u_thematicDisplayMode) {
    vec3 u = (u_modelToWorld * rawPosition).xyz;
    vec3 v = u_thematicAxis;
    vec3 proju = (dot(v, u) / dot(v, v)) * v;
    vec3 a = v * u_thematicRange.x;
    vec3 b = v * u_thematicRange.y;
    vec3 c = proju;
    v_thematicIndex = findFractionalPositionOnLine(a, b, c);
  } else if (kThematicDisplayMode_HillShade == u_thematicDisplayMode) {
    v_thematicIndex = v_n.z;
  }
)";

// ---------------------------------------------------------------------------
// applyThematicColorPrelude — IDW sensor interpolation
// Ported from: itwinjs-core Thematic.ts applyThematicColorPrelude
// ---------------------------------------------------------------------------
inline constexpr char const* kApplyThematicColorPrelude = R"(
  float ndx = v_thematicIndex;

  if (kThematicDisplayMode_InverseDistanceWeightedSensors == u_thematicDisplayMode) {
    float sensorSum = 0.0;
    float contributionSum = 0.0;
    ndx = -1.0;
    float distanceCutoff = u_thematicSettings.y;

    for (int i = 0; i < 128; i++) {
      if (i >= u_numSensors)
        break;
      vec4 sensor = getSensor(i);
      float dist = distance(v_eyeSpace, sensor.xyz);
      bool skipThisSensor = (distanceCutoff > 0.0 && dist > distanceCutoff);
      if (!skipThisSensor) {
        float contribution = 1.0 / pow(dist, 2.0);
        sensorSum += sensor.w * contribution;
        contributionSum += contribution;
      }
    }
    if (contributionSum > 0.0)
      ndx = sensorSum / contributionSum;
  } else if (kThematicDisplayMode_Slope == u_thematicDisplayMode) {
    float d = dot(v_n, u_thematicAxis);
    if (d < 0.0) d = -d;
    d = acos(d);
    if (d < u_thematicRange.x || d > u_thematicRange.y)
      d = -1.0;
    else {
      d -= u_thematicRange.x;
      d /= (u_thematicRange.y - u_thematicRange.x);
    }
    ndx = d;
  } else if (kThematicDisplayMode_HillShade == u_thematicDisplayMode) {
    float d = dot(v_n, u_thematicSunDirection);
    ndx = max(0.0, d);
  }
)";

// ---------------------------------------------------------------------------
// applyThematicColorPostlude — gradient mode handling (surface)
// Ported from: itwinjs-core Thematic.ts applyThematicColorPostlude
// ---------------------------------------------------------------------------
inline constexpr char const* kApplyThematicColorPostlude = R"(
  float gradientMode = u_thematicSettings.x;
  float stepCount = u_thematicSettings.z;

  vec4 rgba = (kThematicGradientMode_IsoLines == gradientMode) ? getIsoLineColor(ndx, stepCount) : getColor(ndx);
  rgba.a = baseColor.a * (u_thematicSettings.w > 0.0 ? rgba.a : 1.0);
  rgba = mix(rgba, baseColor, u_thematicColorMix);

  if (kThematicGradientMode_IsoLines == gradientMode) {
    float coord = v_thematicIndex * stepCount;
    float line = abs(fract(coord - 0.5) - 0.5) / _universal_fwidth(coord);
    rgba.a = 1.0 - min(line, 1.0);
    if (u_discardBetweenIsolines && 0.0 == rgba.a)
      discard;
  } else if (kThematicGradientMode_SteppedWithDelimiter == gradientMode) {
    float coord = v_thematicIndex * stepCount;
    float line = abs(fract(coord - 0.5) - 0.5) / _universal_fwidth(coord);
    float value = min(line, 1.0);
    rgba.rgb *= value;
  }

  baseColor = rgba;
)";

END_DQ_RENDER_NAMESPACE
