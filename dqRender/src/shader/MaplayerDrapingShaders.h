// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Map layer draping shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/MaplayerDraping.ts
//
// Map layer texture draping functions for projected and non-projected textures.
// Includes half-plane boundary test (testInside) and texture application
// (applyTexture) with classification support.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Common helper: addUInt32s (carry-propagating byte-wise addition)
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core Common.ts addUInt32s
static char const* kAddUInt32s = R"glsl(
vec4 addUInt32s(vec4 a, vec4 b) {
  vec4 c = a + b;
  if (c.x > 255.0) { c.x -= 256.0; c.y += 1.0; }
  if (c.y > 255.0) { c.y -= 256.0; c.z += 1.0; }
  if (c.z > 255.0) { c.z -= 256.0; c.w += 1.0; }
  return c;
}
)glsl";

// ---------------------------------------------------------------------------
// testInside — half-plane boundary test
// ---------------------------------------------------------------------------
// Tests whether point (x, y) is on the interior side of the directed edge
// from (x0, y0) to (x1, y1). Returns true if the point is inside or on the
// boundary (within a small tolerance).
// Ported from: itwinjs-core MaplayerDraping.ts testInside
static char const* kMaplayerDrapingTestInside = R"glsl(
bool testInside(float x0, float y0, float x1, float y1, float x, float y) {
  vec2 perp = vec2(y0 - y1, x1 - x0), test = vec2(x - x0, y - y0);
  float dot = (test.x * perp.x + test.y * perp.y) / sqrt(perp.x * perp.x + perp.y * perp.y);
  return dot >= -0.001;
}
)glsl";

// ---------------------------------------------------------------------------
// applyTexture — map layer texture application (non-reality-tile variant)
// ---------------------------------------------------------------------------
// Applies a projected or non-projected texture to the fragment color.
// For projected textures, transforms eye-space position via matrix and
// checks half-plane boundaries. For non-projected, uses UV transform.
// Uniforms:   s_texture (sampler2D), u_texParams (mat4), u_texMatrix (mat4)
// Varyings:   v_eyeSpace (vec3), v_texCoord (vec2)
// Outputs:    col (inout vec4), featureIncrement (float), classifierId (vec4)
// Ported from: itwinjs-core MaplayerDraping.ts applyTexture(false)
static char const* kMaplayerDrapingApplyTexture = R"glsl(
bool applyTexture(inout vec4 col, sampler2D sampler, mat4 params, mat4 matrix) {
  vec2 uv;
  float layerAlpha;
  bool isProjected = params[0][0] != 0.0;
  float imageCount = params[0][1];
  vec2 classPos;

  if (isProjected) {
    vec4 eye4 = vec4(v_eyeSpace, 1.0);
    vec4 classPos4 = matrix * eye4;
    classPos = classPos4.xy / classPos4.w;

    if (!testInside(params[2].x, params[2].y, params[2].z, params[2].w, classPos.x, classPos.y)
        && !testInside(params[2].z, params[2].w, params[3].x, params[3].y, classPos.x, classPos.y)
        && !testInside(params[3].x, params[3].y, params[3].z, params[3].w, classPos.x, classPos.y)
        && !testInside(params[3].z, params[3].w, params[2].x, params[2].y, classPos.x, classPos.y))
        return false;

    uv.x = classPos.x;
    uv.y = classPos.y / imageCount;
    layerAlpha = params[0][2];

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
      return false;

  } else {
    vec4 texTransform = matrix[0].xyzw;
    vec4 texClip = matrix[1].xyzw;
    layerAlpha = matrix[2].x;
    uv = vec2(texTransform[0] + texTransform[2] * v_texCoord.x, texTransform[1] + texTransform[3] * v_texCoord.y);

    if (uv.x < texClip[0] || uv.x > texClip[2] || uv.y < texClip[1] || uv.y > texClip[3])
      return false;

    uv.y = 1.0 - uv.y;
  }

  vec4 texCol = TEXTURE(sampler, uv);
  float alpha = layerAlpha * texCol.a;

  if (alpha > 0.05) {
    vec3 texRgb = isProjected ? (texCol.rgb / texCol.a) : texCol.rgb;
    col.rgb = (1.0 - alpha) * col.rgb + alpha * texRgb;

    if (isProjected) {
      vec4 featureTexel = TEXTURE(sampler, vec2(uv.x, (1.0 + classPos.y) / imageCount));
      classifierId = addUInt32s(params[1], featureTexel * 255.0) / 255.0;
    } else {
      featureIncrement = matrix[2].y;
      classifierId = vec4(0);
    }

    if (alpha > col.a)
      col.a = alpha;

    return true;
  }

  return (col.a > 0.05);
}
)glsl";

// ---------------------------------------------------------------------------
// applyTextureReality — map layer texture application (reality-tile variant)
// ---------------------------------------------------------------------------
// Same as applyTexture but uses OR logic for boundary tests instead of AND,
// because reality tiles use a different winding convention.
// Ported from: itwinjs-core MaplayerDraping.ts applyTexture(true)
static char const* kMaplayerDrapingApplyTextureReality = R"glsl(
bool applyTexture(inout vec4 col, sampler2D sampler, mat4 params, mat4 matrix) {
  vec2 uv;
  float layerAlpha;
  bool isProjected = params[0][0] != 0.0;
  float imageCount = params[0][1];
  vec2 classPos;

  if (isProjected) {
    vec4 eye4 = vec4(v_eyeSpace, 1.0);
    vec4 classPos4 = matrix * eye4;
    classPos = classPos4.xy / classPos4.w;

    if (!testInside(params[2].x, params[2].y, params[2].z, params[2].w, classPos.x, classPos.y)
        || !testInside(params[2].z, params[2].w, params[3].x, params[3].y, classPos.x, classPos.y)
        || !testInside(params[3].x, params[3].y, params[3].z, params[3].w, classPos.x, classPos.y)
        || !testInside(params[3].z, params[3].w, params[2].x, params[2].y, classPos.x, classPos.y))
        return false;

    uv.x = classPos.x;
    uv.y = classPos.y / imageCount;
    layerAlpha = params[0][2];

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
      return false;

  } else {
    vec4 texTransform = matrix[0].xyzw;
    vec4 texClip = matrix[1].xyzw;
    layerAlpha = matrix[2].x;
    uv = vec2(texTransform[0] + texTransform[2] * v_texCoord.x, texTransform[1] + texTransform[3] * v_texCoord.y);

    if (uv.x < texClip[0] || uv.x > texClip[2] || uv.y < texClip[1] || uv.y > texClip[3])
      return false;

    uv.y = 1.0 - uv.y;
  }

  vec4 texCol = TEXTURE(sampler, uv);
  float alpha = layerAlpha * texCol.a;

  if (alpha > 0.05) {
    vec3 texRgb = isProjected ? (texCol.rgb / texCol.a) : texCol.rgb;
    col.rgb = (1.0 - alpha) * col.rgb + alpha * texRgb;

    if (isProjected) {
      vec4 featureTexel = TEXTURE(sampler, vec2(uv.x, (1.0 + classPos.y) / imageCount));
      classifierId = addUInt32s(params[1], featureTexel * 255.0) / 255.0;
    } else {
      featureIncrement = matrix[2].y;
      classifierId = vec4(0);
    }

    if (alpha > col.a)
      col.a = alpha;

    return true;
  }

  return (col.a > 0.05);
}
)glsl";

// ---------------------------------------------------------------------------
// overrideFeatureId — feature ID override logic
// ---------------------------------------------------------------------------
// Returns the classifierId if set, otherwise adds featureIncrement to the
// feature_id for proper classification tracking.
// Ported from: itwinjs-core MaplayerDraping.ts overrideFeatureId
static char const* kMaplayerDrapingOverrideFeatureId = R"glsl(
return (classifierId == vec4(0)) ? (addUInt32s(feature_id * 255.0, vec4(featureIncrement, 0.0, 0.0, 0.0)) / 255.0) : classifierId;
)glsl";

END_DQ_RENDER_NAMESPACE
