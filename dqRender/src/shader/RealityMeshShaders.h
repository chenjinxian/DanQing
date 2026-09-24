// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Reality mesh shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/RealityMesh.ts
//
// Reality mesh rendering: MVP transform, oct-decoded normals, UV
// unquantization, multi-texture sampling with map layer draping support,
// and feature color override mixing.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Reality mesh vertex shader
// ---------------------------------------------------------------------------
// Transforms position by MVP, decodes oct-encoded normal, unquantizes UV.
// Attributes: a_position (vec3), a_norm (uint), a_uvParam (vec2)
// Uniforms:   u_mvp (mat4), u_worldToViewN (mat3), u_qTexCoordParams (vec4)
// Varyings:   v_eyeSpace (vec3), v_n (vec3), v_texCoord (vec2), v_color (vec4)
// Ported from: itwinjs-core RealityMesh.ts computePosition / computeNormal / computeTexCoord
static char const* kRealityMeshVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in uint a_norm;
layout(location = 2) in vec2 a_uvParam;
layout(location = 3) in vec4 a_color;

uniform mat4 u_mvp;
uniform mat4 u_mv;
uniform mat3 u_worldToViewN;
uniform vec4 u_qTexCoordParams;

out vec3 v_eyeSpace;
out vec3 v_n;
out vec2 v_texCoord;
out vec4 v_color;

// Oct-decode: recovers a unit normal from a packed uint
// Ported from: itwinjs-core Surface.ts octDecodeNormal
vec3 octDecodeNormal(uint oct) {
    float fx = float(oct & 0xFFu) / 255.0 * 2.0 - 1.0;
    float fy = float((oct >> 8u) & 0xFFu) / 255.0 * 2.0 - 1.0;
    vec2 octEnc = vec2(fx, fy);
    vec3 v = vec3(octEnc.xy, 1.0 - abs(octEnc.x) - abs(octEnc.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
    }
    return normalize(v);
}

// unquantize 2D: maps quantized uint16 UV to float UV
// Ported from: itwinjs-core Decode.ts unquantize2d
vec2 unquantize2d(vec2 qpos, vec4 params) {
    return params.xy + params.zw * qpos;
}

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    gl_PointSize = 1.0;
    v_eyeSpace = (u_mv * vec4(a_position, 1.0)).xyz;

    // normal comes in world-space, transform to view space
    vec3 normal = octDecodeNormal(a_norm);
    v_n = normalize(u_worldToViewN * normal);

    v_texCoord = unquantize2d(a_uvParam, u_qTexCoordParams);
    v_color = a_color;
}
)glsl";

// ---------------------------------------------------------------------------
// Reality mesh fragment shader
// ---------------------------------------------------------------------------
// Multi-texture sampling with applyTexture, feature color override mix.
// Uniforms:   u_texturesPresent (bool), s_texture0..N (sampler2D),
//             u_texParams0..N (mat4), u_texMatrix0..N (mat4),
//             u_baseColor (vec4), u_overrideColorMix (float)
// Varyings:   v_eyeSpace (vec3), v_n (vec3), v_texCoord (vec2), v_color (vec4)
// Ported from: itwinjs-core RealityMesh.ts baseColorFromTextures / mixFeatureColor
static char const* kRealityMeshFrag = R"glsl(
#version 410 core

in vec3 v_eyeSpace;
in vec3 v_n;
in vec2 v_texCoord;
in vec4 v_color;

uniform bool u_texturesPresent;
uniform vec4 u_baseColor;
uniform float u_overrideColorMix;

// Feature color override globals (set by ShaderBuilder)
float featureIncrement = 0.0;
vec4 classifierId = vec4(0.0);

out vec4 fragColor;

// Half-plane boundary test for projected textures
// Ported from: itwinjs-core MaplayerDraping.ts testInside
bool testInside(float x0, float y0, float x1, float y1, float x, float y) {
  vec2 perp = vec2(y0 - y1, x1 - x0), test = vec2(x - x0, y - y0);
  float dotVal = (test.x * perp.x + test.y * perp.y) / sqrt(perp.x * perp.x + perp.y * perp.y);
  return dotVal >= -0.001;
}

// Carry-propagating byte-wise addition for classifier ID
// Ported from: itwinjs-core Common.ts addUInt32s
vec4 addUInt32s(vec4 a, vec4 b) {
  vec4 c = a + b;
  if (c.x > 255.0) { c.x -= 256.0; c.y += 1.0; }
  if (c.y > 255.0) { c.y -= 256.0; c.z += 1.0; }
  if (c.z > 255.0) { c.z -= 256.0; c.w += 1.0; }
  return c;
}

// Apply a single texture layer (reality-tile variant with OR boundary tests)
// Ported from: itwinjs-core MaplayerDraping.ts applyTexture(true)
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

  vec4 texCol = texture(sampler, uv);
  float alpha = layerAlpha * texCol.a;

  if (alpha > 0.05) {
    vec3 texRgb = isProjected ? (texCol.rgb / texCol.a) : texCol.rgb;
    col.rgb = (1.0 - alpha) * col.rgb + alpha * texRgb;

    if (isProjected) {
      vec4 featureTexel = texture(sampler, vec2(uv.x, (1.0 + classPos.y) / imageCount));
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

// Feature color override: mix per-feature color into base color
// feature_rgb.r = -1.0 if rgb not overridden, feature_alpha = -1.0 if alpha not overridden
// Ported from: itwinjs-core RealityMesh.ts mixFeatureColor
void applyFeatureColor(inout vec4 col) {
  col.rgb = mix(col.rgb, mix(col.rgb, v_color.rgb, u_overrideColorMix), step(0.0, v_color.r));
  col.a = mix(col.a, v_color.a, step(0.0, v_color.a));
}

// override feature ID for classification
// Ported from: itwinjs-core MaplayerDraping.ts overrideFeatureId
vec4 overrideFeatureId(vec4 feature_id) {
  return (classifierId == vec4(0)) ? (addUInt32s(feature_id * 255.0, vec4(featureIncrement, 0.0, 0.0, 0.0)) / 255.0) : classifierId;
}

// finalizeNormal — flip normal based on gl_FrontFacing
// Ported from: itwinjs-core RealityMesh.ts finalizeNormal (line 41-43)
vec3 finalizeNormal(vec3 normal) {
    return normalize(normal) * (2.0 * float(gl_FrontFacing) - 1.0);
}

void main()
{
    // Finalize normal (flip back faces)
    v_n = finalizeNormal(v_n);

    if (!u_texturesPresent) {
        vec4 col = u_baseColor;
        applyFeatureColor(col);
        fragColor = col;
        return;
    }

    bool doDiscard = true;
    vec4 col = u_baseColor;
    // Texture sampling slots are populated by ShaderBuilder per mesh texture count
    // ApplyTexture calls are inserted here by ShaderBuilder:
    //   if (applyTexture(col, s_texture0, u_texParams0, u_texMatrix0)) doDiscard = false;
    //   if (applyTexture(col, s_texture1, u_texParams1, u_texMatrix1)) doDiscard = false;
    //   ... (up to maxRealityImageryLayers)
    if (doDiscard)
        discard;

    applyFeatureColor(col);
    fragColor = col;
}
)glsl";

END_DQ_RENDER_NAMESPACE
