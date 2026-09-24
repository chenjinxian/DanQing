// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Composite shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Composite.ts
//
// All composite shaders for combining render passes.
// These are fullscreen quad shaders that sample from multiple textures.
// Includes hilite edge detection (readEdgePixel + computeNearbyHilites)
// and proper weighted OIT compositing.
//
// Note: standalone shaders use texture() directly (no TEXTURE macro).
#pragma once

#include <cstdint>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Reuse fullscreen quad vertex shader from PostProcessShaders.h
// (kFullscreenQuadVert is defined there)

// ---------------------------------------------------------------------------
// Hilite helper GLSL fragments
// Ported from: itwinjs-core Composite.ts readEdgePixel + computeNearbyHilites
// ---------------------------------------------------------------------------

// windowCoordsToTexCoords — convert window coords to texture coords
// Ported from: itwinjs-core Fragment.ts windowCoordsToTexCoords
inline std::string hiliteHelperFunctions() {
    return R"glsl(
vec2 windowCoordsToTexCoords(vec2 windowPos) {
  return windowPos / vec2(textureSize(u_colorTexture, 0));
}

vec2 readEdgePixel(float xOffset, float yOffset) {
  vec2 t = windowCoordsToTexCoords(gl_FragCoord.xy + vec2(xOffset, yOffset));
  return texture(u_hilite, t).xy;
}

vec2 computeNearbyHilites() {
  float hw = u_hilite_width.x;
  float ew = u_hilite_width.y;
  float maxWidth = max(hw, ew);
  if (0.0 == maxWidth)
    return vec2(0.0);

  vec2 nearest = vec2(0.0, 0.0);
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
      if (0 != x || 0 != y)
        nearest = nearest + readEdgePixel(float(x), float(y));

  nearest = nearest * vec2(float(hw > 0.0), float(ew > 0.0));

  if ((0.0 == nearest.x && hw > 1.0) || (0.0 == nearest.y && ew > 1.0)) {
    vec2 farthest = vec2(0.0, 0.0);
    for (int i = -2; i <= 2; i++) {
      float f = float(i);
      farthest = farthest + readEdgePixel(f, -2.0) + readEdgePixel(-2.0, f) + readEdgePixel(f, 2.0) + readEdgePixel(2.0, f);
    }
    farthest = farthest * vec2(float(hw > 1.0), float(ew > 1.0));
    nearest = nearest + farthest;
  }

  return nearest;
}

vec4 applyHilite(vec4 baseColor) {
  vec2 flags = texture(u_hilite, v_texCoord).rg;
  vec2 outline = computeNearbyHilites();
  if (u_hilite_width.y < u_hilite_width.x) {
    if (outline.y > 0.0 && flags.y == 0.0)
      return vec4(u_hilite_settings[1], 1.0);
    if (outline.x > 0.0 && flags.x == 0.0)
      return vec4(u_hilite_settings[0], 1.0);
  } else {
    if (outline.x > 0.0 && flags.x == 0.0)
      return vec4(u_hilite_settings[0], 1.0);
    if (outline.y > 0.0 && flags.y == 0.0)
      return vec4(u_hilite_settings[1], 1.0);
  }
  float hiliteMix = flags.x * u_hilite_settings[2][0];
  float emphasisMix = flags.y * u_hilite_settings[2][1];
  baseColor.rgb *= (1.0 - (hiliteMix + emphasisMix));
  baseColor.rgb += u_hilite_settings[0] * hiliteMix;
  baseColor.rgb += u_hilite_settings[1] * emphasisMix;
  return baseColor;
}
)glsl";
}

// ---------------------------------------------------------------------------
// CompositeHilite — hilite overlay with edge detection
// Ported from: itwinjs-core Composite.ts computeHiliteBaseColor
// ---------------------------------------------------------------------------
inline std::string compositeHiliteFrag() {
    return std::string(R"glsl(
#version 410 core
in vec2 v_texCoord;

uniform sampler2D u_colorTexture;
uniform sampler2D u_hilite;
uniform mat3 u_hilite_settings;
uniform vec2 u_hilite_width;

out vec4 fragColor;

)glsl") + hiliteHelperFunctions() + R"glsl(

void main() {
    vec4 baseColor = texture(u_colorTexture, v_texCoord);
    fragColor = applyHilite(baseColor);
}
)glsl";
}

// ---------------------------------------------------------------------------
// CompositeHiliteAndTranslucent — hilite + weighted OIT compositing
// Ported from: itwinjs-core Composite.ts computeTranslucentColor + computeHiliteBaseColor
// ---------------------------------------------------------------------------
inline std::string compositeHiliteAndTranslucentFrag() {
    return std::string(R"glsl(
#version 410 core
in vec2 v_texCoord;

uniform sampler2D u_colorTexture;
uniform sampler2D u_hilite;
uniform sampler2D u_accumulation;
uniform sampler2D u_revealage;
uniform mat3 u_hilite_settings;
uniform vec2 u_hilite_width;
uniform vec3 u_clipIntersection;

out vec4 fragColor;

)glsl") + hiliteHelperFunctions() + R"glsl(

void main() {
    vec4 opaque = texture(u_colorTexture, v_texCoord);

    // Weighted OIT compositing
    // Ported from: itwinjs-core Composite.ts computeTranslucentColor (line 106-116)
    vec4 accum = texture(u_accumulation, v_texCoord);
    vec2 rg = texture(u_revealage, v_texCoord).rg;
    vec4 transparent = vec4(accum.rgb / clamp(rg.r, 1e-4, 5e4), accum.a);
    vec4 baseColor = mix((1.0 - transparent.a) * transparent + transparent.a * opaque, vec4(u_clipIntersection, 1.0), rg.g);

    fragColor = applyHilite(baseColor);
}
)glsl";
}

// ---------------------------------------------------------------------------
// CompositeOcclusion — applies AO to main render
// ---------------------------------------------------------------------------
static char const* kCompositeOcclusionFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_aoTexture;
out vec4 fragColor;
void main() {
    vec4 color = texture(u_colorTexture, v_texCoord);
    float ao = texture(u_aoTexture, v_texCoord).r;
    fragColor = vec4(color.rgb * ao, color.a);
}
)glsl";

// ---------------------------------------------------------------------------
// CompositeTranslucentAndOcclusion — translucent + AO
// Ported from: itwinjs-core Composite.ts computeTranslucentColor
// ---------------------------------------------------------------------------
static char const* kCompositeTranslucentAndOcclusionFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_accumulation;
uniform sampler2D u_revealage;
uniform sampler2D u_aoTexture;
uniform vec3 u_clipIntersection;
out vec4 fragColor;
void main() {
    vec4 opaque = texture(u_colorTexture, v_texCoord);
    opaque.rgb *= texture(u_aoTexture, v_texCoord).r;
    vec4 accum = texture(u_accumulation, v_texCoord);
    vec2 rg = texture(u_revealage, v_texCoord).rg;
    vec4 transparent = vec4(accum.rgb / clamp(rg.r, 1e-4, 5e4), accum.a);
    fragColor = mix((1.0 - transparent.a) * transparent + transparent.a * opaque, vec4(u_clipIntersection, 1.0), rg.g);
}
)glsl";

// ---------------------------------------------------------------------------
// CompositeHiliteAndOcclusion — hilite + AO
// ---------------------------------------------------------------------------
inline std::string compositeHiliteAndOcclusionFrag() {
    return std::string(R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_hilite;
uniform sampler2D u_aoTexture;
uniform mat3 u_hilite_settings;
uniform vec2 u_hilite_width;
out vec4 fragColor;

)glsl") + hiliteHelperFunctions() + R"glsl(

void main() {
    vec4 baseColor = texture(u_colorTexture, v_texCoord);
    baseColor.rgb *= texture(u_aoTexture, v_texCoord).r;
    fragColor = applyHilite(baseColor);
}
)glsl";
}

// ---------------------------------------------------------------------------
// CompositeAll — hilite + translucent + AO
// Ported from: itwinjs-core Composite.ts (all three combined)
// ---------------------------------------------------------------------------
inline std::string compositeAllFrag() {
    return std::string(R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_hilite;
uniform sampler2D u_accumulation;
uniform sampler2D u_revealage;
uniform sampler2D u_aoTexture;
uniform mat3 u_hilite_settings;
uniform vec2 u_hilite_width;
uniform vec3 u_clipIntersection;
out vec4 fragColor;

)glsl") + hiliteHelperFunctions() + R"glsl(

void main() {
    vec4 opaque = texture(u_colorTexture, v_texCoord);
    opaque.rgb *= texture(u_aoTexture, v_texCoord).r;

    // Weighted OIT compositing
    vec4 accum = texture(u_accumulation, v_texCoord);
    vec2 rg = texture(u_revealage, v_texCoord).rg;
    vec4 transparent = vec4(accum.rgb / clamp(rg.r, 1e-4, 5e4), accum.a);
    vec4 baseColor = mix((1.0 - transparent.a) * transparent + transparent.a * opaque, vec4(u_clipIntersection, 1.0), rg.g);

    fragColor = applyHilite(baseColor);
}
)glsl";
}

// ---------------------------------------------------------------------------
// CopyColor — simple texture copy
// ---------------------------------------------------------------------------
static char const* kCopyColorFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_texture;
out vec4 fragColor;
void main() { fragColor = texture(u_texture, v_texCoord); }
)glsl";

// ---------------------------------------------------------------------------
// CopyColorNoAlpha — copy without alpha
// ---------------------------------------------------------------------------
static char const* kCopyColorNoAlphaFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_texture;
out vec4 fragColor;
void main() { fragColor = vec4(texture(u_texture, v_texCoord).rgb, 1.0); }
)glsl";

// ---------------------------------------------------------------------------
// CopyPickBuffers — resolve pick buffer (R32UI)
// ---------------------------------------------------------------------------
static char const* kCopyPickBuffersFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform usampler2D u_pickTexture;
out uint fragColor;
void main() { fragColor = texture(u_pickTexture, v_texCoord).r; }
)glsl";

// ---------------------------------------------------------------------------
// ClearPickAndColor — clear both pick and color buffers
// ---------------------------------------------------------------------------
static char const* kClearPickAndColorFrag = R"glsl(
#version 410 core
layout(location = 0) out vec4 fragColor0;
layout(location = 1) out uint fragColor1;
void main() {
    fragColor0 = vec4(0.0);
    fragColor1 = 0u;
}
)glsl";

// ---------------------------------------------------------------------------
// EvsmFromDepth — Exponential Variance Shadow Map from depth
// ---------------------------------------------------------------------------
static char const* kEvsmFromDepthFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_depthTexture;
uniform float u_evsmExponent;
out vec4 fragColor;
void main() {
    float depth = texture(u_depthTexture, v_texCoord).r;
    float exp1 = exp(depth * u_evsmExponent);
    float exp2 = exp(depth * u_evsmExponent * 2.0);
    fragColor = vec4(exp1, exp2, 0.0, 1.0);
}
)glsl";

END_DQ_RENDER_NAMESPACE
