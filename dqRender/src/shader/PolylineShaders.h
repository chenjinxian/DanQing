// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Polyline shader constants
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Polyline.ts
//
// The polyline shader system provides:
// - Joint computation (miter/bevel/square) with clamped miter distance
// - Slope-based width adjustment for non-AA lines (widths 1-4)
// - Line code pattern texture sampling
// - Pixel trimming for width-1 lines
// - Front-plane clipping with segment interpolation
// - LUT texture position sampling (quantized and unquantized paths)
#pragma once

#include "EdgeShaders.h"

#include <string_view>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Phase 1 fallback: reuse Edge shaders for simple polyline rendering.
static char const* kPolylineVert = kEdgeVert;
static char const* kPolylineFrag = kEdgeFrag;

// ---------------------------------------------------------------------------
// GLSL — decodeFloatFromBytes: unpack a float from 4 bytes
// Ported from: itwinjs-core Polyline.ts decodeFloatFromBytes
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPolylineDecodeFloatFromBytes = R"(
float decodeFloatFromBytes(vec4 bytes) {
  uvec4 b = uvec4(bytes);
  uint u = b.x | (b.y << 8) | (b.z << 16) | (b.w << 24);
  return uintBitsToFloat(u);
}
)";

// ---------------------------------------------------------------------------
// GLSL — adjustWidth: slope-based width adjustment for non-AA lines
// Ported from: itwinjs-core Polyline.ts adjustWidth (widths 1-4)
// Sets v_lnInfo for pixel trimming on width-1 lines
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPolylineAdjustWidth = R"(
void adjustWidth(inout float width, vec2 d2, vec2 org) {
  if (u_aaSamples > 1) {
    if (width < 5.0) width += (5.0 - width) * 0.125;
    return;
  }
  vec2 d2A = abs(d2);
  const float s_myFltEpsilon = 0.0001;
  if (d2A.y > s_myFltEpsilon && width < 4.5) {
    float len = length(d2A);
    float tan = d2A.x / d2A.y;
    if (width < 1.5) {
      if (tan <= 1.0) width = d2A.y; else width = d2A.x;
      width *= 1.01;
      v_lnInfo.xy = org;
      v_lnInfo.w = 1.0;
      if (d2A.x - d2A.y > s_myFltEpsilon) {
        v_lnInfo.z = d2.y / d2.x;
        v_lnInfo.w += 2.0;
      } else v_lnInfo.z = d2.x / d2.y;
    } else if (width < 2.5) {
      if (tan <= 0.5) width = 2.0 * d2A.y; else width = (d2A.y + 2.0 * d2A.x);
    } else if (width < 3.5) {
      if (tan <= 1.0) width = (3.0 * d2A.y + d2A.x); else width = (d2A.y + 3.0 * d2A.x);
    } else {
      if (tan <= 0.5) width = (4.0 * d2A.y + d2A.x);
      else if (tan <= 2.0) width = (3.0 * d2A.y + 3.0 * d2A.x);
      else width = (d2A.y + 4.0 * d2A.x);
    }
    width /= len;
  }
}
)";

// ---------------------------------------------------------------------------
// GLSL — computeLineCodeTextureCoords: line code pattern texture lookup
// Ported from: itwinjs-core Polyline.ts computeTextureCoord
// Returns (-1,-1) for solid lines
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPolylineComputeLineCodeTextureCoords = R"(
vec2 computeLineCodeTextureCoords(vec2 windowDir, vec4 projPos, float adjust, float patternDist) {
  vec2 texc;
  float lineCode = computeLineCode();
  if (0.0 == lineCode) {
    texc = vec2(-1.0, -1.0);
  } else {
    const float imagesPerPixel = 1.0/32.0;
    const float textureCoordinateBase = 8192.0;
    float patternDistPixels;
    if (u_useCumDist > 0.5) {
      patternDistPixels = patternDist * u_pixelsPerWorld;
    } else {
      if (abs(windowDir.x) > abs(windowDir.y))
        patternDistPixels = projPos.x + adjust * windowDir.x;
      else
        patternDistPixels = projPos.y + adjust * windowDir.y;
    }
    texc.x = textureCoordinateBase + imagesPerPixel * patternDistPixels;
    float numRows = u_numLineCodes;
    float centerY = 0.5 / numRows;
    float stepY = 1.0 / numRows;
    texc.y = stepY * lineCode + centerY;
  }
  return texc;
}
)";

// ---------------------------------------------------------------------------
// GLSL — applyLineCode (fragment): line code texture + pixel trimming
// Ported from: itwinjs-core Polyline.ts applyLineCode
// Sets discardByLineCode for pattern-based discard
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPolylineApplyLineCode = R"(
  if (v_texc.x >= 0.0) {
    vec4 texColor = TEXTURE(u_lineCodeTexture, v_texc);
    discardByLineCode = (0.0 == texColor.r);
  }
  if (v_lnInfo.w > 0.5) {
    vec2 dxy = gl_FragCoord.xy - v_lnInfo.xy;
    if (v_lnInfo.w < 1.5) dxy = dxy.yx;
    float dist = v_lnInfo.z * dxy.x - dxy.y;
    float distA = abs(dist);
    if (distA > 0.5 || (distA == 0.5 && dist < 0.0))
      discardByLineCode = true;
  }
  return baseColor;
)";

// ---------------------------------------------------------------------------
// GLSL — modelToWindowCoordinates: project position to window coords
// Ported from: itwinjs-core Viewport.ts modelToWindowCoordinates
// Handles front-plane clipping with segment interpolation
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPolylineModelToWindowCoordinates = R"(
vec4 modelToWindowCoordinates(vec4 position, vec4 next, out vec4 clippedMvpPos, out vec3 clippedMvPos) {
  if (kRenderPass_ViewOverlay == u_renderPass || kRenderPass_Background == u_renderPass) {
    vec4 q = MAT_MV * position;
    clippedMvPos = q.xyz;
    q = u_proj * q;
    clippedMvpPos = q;
    q.xyz /= q.w;
    q.xyz = (u_viewportTransformation * vec4(q.xyz, 1.0)).xyz;
    return q;
  }
  float s_maxZ = -u_frustum.x;
  vec4 q = MAT_MV * position;
  vec4 n = MAT_MV * next;
  if (q.z > s_maxZ) {
    if (n.z > s_maxZ) {
      clippedMvPos = vec3(0.0, 0.0, 1.0);
      clippedMvpPos = vec4(0.0, 0.0, 1.0, 0.0);
      return vec4(0.0, 0.0, 1.0, 0.0);
    }
    float t = (s_maxZ - q.z) / (n.z - q.z);
    q.x += t * (n.x - q.x);
    q.y += t * (n.y - q.y);
    q.z = s_maxZ;
  }
  clippedMvPos = q.xyz;
  q = u_proj * q;
  clippedMvpPos = q;
  q.xyz /= q.w;
  q.xyz = (u_viewportTransformation * vec4(q.xyz, 1.0)).xyz;
  return q;
}
)";

// ---------------------------------------------------------------------------
// GLSL — buildComputePosition: main vertex position with joint computation
// Ported from: itwinjs-core Polyline.ts buildComputePosition
// Handles miter/bevel/square joints, slope-based width, front-plane clipping
// ---------------------------------------------------------------------------
inline constexpr std::string_view kPolylineComputePosition = R"(
  const float kNone = 0.0, kSquare = 3.0, kMiter = 6.0, kMiterInsideOnly = 9.0,
              kJointBase = 12.0, kNegatePerp = 24.0, kNegateAlong = 48.0, kNoneAdjWt = 96.0;
  v_lnInfo = vec4(0.0, 0.0, 0.0, 0.0);
  vec4 next = g_nextPos;
  vec4 pos;
  g_windowPos = modelToWindowCoordinates(rawPos, next, pos, v_eyeSpace);
  if (g_windowPos.w == 0.0) return g_windowPos;
  float param = a_param;
  float weight = computeLineWeight();
  float scale = 1.0, directionScale = 1.0;
  if (param >= kNoneAdjWt) param -= kNoneAdjWt;
  if (param >= kNegateAlong) { directionScale = -directionScale; param -= kNegateAlong; }
  if (param >= kNegatePerp) { scale = -1.0; param -= kNegatePerp; }
  vec4 otherPos;
  vec3 otherMvPos;
  vec4 projNext = modelToWindowCoordinates(next, rawPos, otherPos, otherMvPos);
  g_windowDir = projNext.xy - g_windowPos.xy;
  if (u_useCumDist > 0.5)
    v_patternDistance = ((u_vertParams.z > 5.0) ? decodeFloatFromBytes(g_vertLutData5) : 0.0);
  else v_patternDistance = 0.0;
  if (param < kJointBase) {
    vec2 dir = (directionScale > 0.0) ? g_windowDir : -g_windowDir;
    vec2 org = (directionScale > 0.0) ? g_windowPos.xy : projNext.xy;
    adjustWidth(weight, dir, org);
  }
  if (kNone != param) {
    vec2 delta = vec2(0.0);
    vec4 prev = g_prevPos;
    vec4 projPrev = modelToWindowCoordinates(prev, rawPos, otherPos, otherMvPos);
    vec2 prevDir = g_windowPos.xy - projPrev.xy;
    float thisLength = length(g_windowDir);
    const float s_minLen = 1.0E-5;
    float dist = weight / 2.0;
    if (thisLength > s_minLen) {
      g_windowDir /= thisLength;
      float prevLength = length(prevDir);
      if (prevLength > s_minLen) {
        prevDir /= prevLength;
        float prevNextDot = dot(prevDir, g_windowDir);
        if (prevNextDot < -0.9999 || prevNextDot > 0.9999) param = kSquare;
      } else param = kSquare;
    } else { g_windowDir = -normalize(prevDir); param = kSquare; }
    vec2 perp = scale * vec2(-g_windowDir.y, g_windowDir.x);
    if (param == kSquare) delta = perp;
    else {
      vec2 bisector = normalize(prevDir - g_windowDir);
      float dotP = dot(bisector, perp);
      if (dotP != 0.0) {
        const float maxMiter = 3.0;
        float miterDist = 1.0 / dotP;
        if (param == kMiter)
          delta = (abs(miterDist) > maxMiter) ? perp : bisector * miterDist;
        else if (param == kMiterInsideOnly)
          delta = (dotP > 0.0 || abs(miterDist) > maxMiter) ? perp : bisector * miterDist;
        else {
          float ratio = (param - kJointBase) / 3.0;
          delta = normalize((1.0 - ratio) * bisector + (dotP < 0.0 ? -ratio : ratio) * perp);
        }
      }
    }
    miterAdjust = dot(g_windowDir, delta) * dist;
    pos.x += dist * delta.x * 2.0 * pos.w / u_viewport.x;
    pos.y += dist * delta.y * 2.0 * pos.w / u_viewport.y;
  }
  return pos;
)";

END_DQ_RENDER_NAMESPACE
