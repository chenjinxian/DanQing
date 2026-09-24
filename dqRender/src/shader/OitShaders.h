// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OIT (Order-Independent Transparency) shaders
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Composite.ts
//
// Simplified OIT using alpha blending with depth-weighted compositing.
// Phase 1: weighted blended OIT (McGuire 2013).
// Phase 2: dual depth peeling for correct transparency.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// OIT clear shader — initializes OIT accumulation buffers
// ---------------------------------------------------------------------------
static char const* kOitClearVert = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
void main() { gl_Position = vec4(a_position, 0.0, 1.0); }
)glsl";

static char const* kOitClearFrag = R"glsl(
#version 410 core
layout(location = 0) out vec4 fragColor0;  // accumulation
layout(location = 1) out vec4 fragColor1;  // revealage
void main() {
    fragColor0 = vec4(0.0);
    fragColor1 = vec4(1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// OIT composite shader — composites translucent layers
// Weighted Blended OIT (McGuire & Bavoil 2013)
// ---------------------------------------------------------------------------
static char const* kOitCompositeVert = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
out vec2 v_texCoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = a_position * 0.5 + 0.5;
}
)glsl";

// OIT composite fragment shader
// Ported from: itwinjs-core Composite.ts computeTranslucentColor (line 106-116)
static char const* kOitCompositeFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;

uniform sampler2D u_accumTexture;    // accumulated color+weight
uniform sampler2D u_revealTexture;   // revealage
uniform sampler2D u_opaqueTexture;   // opaque scene color (pre-composite)

out vec4 fragColor;

void main() {
    // Ported from: itwinjs-core Composite.ts computeTranslucentColor (line 106-116):
    //   vec4 accum = TEXTURE(u_accumulation, v_texCoord);
    //   vec2 rg = TEXTURE(u_revealage, v_texCoord).rg;
    //   vec4 transparent = vec4(accum.rgb / clamp(rg.r, 1e-4, 5e4), accum.a);
    //   vec4 col = mix((1.0 - transparent.a) * transparent + transparent.a * opaque,
    //                  vec4(u_clipIntersection.rgb, 1.0), rg.g);
    // rg.g（clip intersection 标记）未接线恒 0 → mix 退化为 over 合成。
    vec4 accum = texture(u_accumTexture, v_texCoord);
    vec2 rg = texture(u_revealTexture, v_texCoord).rg;
    vec4 opaque = texture(u_opaqueTexture, v_texCoord);

    vec4 transparent = vec4(accum.rgb / clamp(rg.r, 1e-4, 5e4), accum.a);
    fragColor = (1.0 - transparent.a) * transparent + transparent.a * opaque;
}
)glsl";

// ---------------------------------------------------------------------------
// OIT translucent accumulation shader
// Ported from: itwinjs-core Translucency.ts computeAlphaWeight + computeOutputs
//
// Uses Equation 10 from McGuire & Bavoil 2013 for the weight function.
// The vertex shader does NOT perform lighting — itwinjs-core's translucent
// pass uses the Surface shader pipeline which handles lighting separately.
// ---------------------------------------------------------------------------
static char const* kOitTranslucentVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec4 a_color;

uniform mat4 u_mvp;
uniform mat4 u_mv;

out vec4 v_color;
out vec3 v_eyeSpace;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_eyeSpace = (u_mv * vec4(a_position, 1.0)).xyz;
    v_color = a_color;
}
)glsl";

static char const* kOitTranslucentFrag = R"glsl(
#version 410 core

in vec4 v_color;
in vec3 v_eyeSpace;

uniform float u_frustum;  // near plane for linear depth computation
uniform bool u_oitFlatAlphaWeight;
uniform bool u_oitScaleOutput;
uniform float u_outputScale;

layout(location = 0) out vec4 fragColor0;  // accumulation
layout(location = 1) out vec4 fragColor1;  // revealage

float computeLinearDepth(float eyeZ) {
    return (-eyeZ - u_frustum.x) / (u_frustum.y - u_frustum.x);
}

void main() {
    vec4 color = v_color;
    float ai = color.a;

    // Linear depth scaled to [0.15, 1.0]
    // Ported from: itwinjs-core Translucency.ts computeLinearDepth (line 12-15)
    float z = clamp(computeLinearDepth(v_eyeSpace.z), 0.15, 1.0);

    // Equation 10 weight function
    // Ported from: itwinjs-core Translucency.ts computeAlphaWeight (line 20-26)
    float wzi;
    if (u_oitFlatAlphaWeight)
        wzi = 1.0;
    else
        wzi = pow(ai + 0.01, 4.0) + max(1e-2, 3.0 * 1e3 * pow(z, 3.0));

    // Output scale
    // Ported from: itwinjs-core Translucency.ts computeOutputs (line 35)
    float outputScale = u_oitScaleOutput ? u_outputScale : 1.0;

    // Accumulation buffer: premultiplied alpha * weight
    fragColor0 = vec4(color.rgb * ai * wzi * outputScale, ai * wzi * outputScale);

    // Revealage buffer: ai * wzi * outputScale in all channels
    // Ported from: itwinjs-core Translucency.ts computeOutputs (line 38)
    float w = ai * wzi * outputScale;
    fragColor1 = vec4(w, 0.0, 0.0, w);
}
)glsl";

END_DQ_RENDER_NAMESPACE
