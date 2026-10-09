// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Compositing and post-processing GLSL shader builders
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//              Composite.ts, AmbientOcclusion.ts, Blur.ts, EDL.ts,
//              EVSMFromDepth.ts, SolarShadowMapping.ts
//
// Factory functions for post-processing effect shaders.
#pragma once

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Fullscreen quad vertex shader (shared by all post-processing)
// (Ported from: itwinjs-core PostProcessShaders.h kFullscreenQuadVert)
// ---------------------------------------------------------------------------
inline char const* getFullscreenQuadVertexShader()
{
    return R"(
#version 410 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";
}

// ---------------------------------------------------------------------------
// SSAO fragment shader
// (Ported from: itwinjs-core glsl/AmbientOcclusion.ts)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Gaussian blur fragment shader
// (Ported from: itwinjs-core glsl/Blur.ts)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// EDL (Eye-Dome Lighting) fragment shader
// (Ported from: itwinjs-core glsl/EDL.ts)
// ---------------------------------------------------------------------------
inline char const* getEdlFragmentShader()
{
    return R"(
#version 410 core
in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D s_colorTexture;
uniform sampler2D s_depthTexture;
uniform vec2 u_texelSize;
uniform float u_edlStrength;
uniform float u_edlRadius;

float sampleDepth(vec2 offset) {
    return texture(s_depthTexture, v_texCoord + offset * u_texelSize).r;
}

void main() {
    float depth = sampleDepth(vec2(0.0));
    if (depth >= 1.0) { fragColor = texture(s_colorTexture, v_texCoord); return; }

    float d0 = sampleDepth(vec2(-1.0, 0.0));
    float d1 = sampleDepth(vec2(1.0, 0.0));
    float d2 = sampleDepth(vec2(0.0, -1.0));
    float d3 = sampleDepth(vec2(0.0, 1.0));

    float edl = 0.0;
    edl += max(0.0, depth - d0);
    edl += max(0.0, depth - d1);
    edl += max(0.0, depth - d2);
    edl += max(0.0, depth - d3);
    edl /= 4.0;

    float shade = exp(-edl * u_edlStrength * 1000.0);
    vec4 color = texture(s_colorTexture, v_texCoord);
    fragColor = vec4(color.rgb * shade, color.a);
}
)";
}

// ---------------------------------------------------------------------------
// EVSM (Exponential Variance Shadow Map) from depth fragment shader
// (Ported from: itwinjs-core glsl/EVSMFromDepth.ts)
// ---------------------------------------------------------------------------
inline char const* getEvsmFromDepthFragmentShader()
{
    return R"(
#version 410 core
in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D s_depthTexture;
uniform float u_evsmExponent;

void main() {
    float depth = texture(s_depthTexture, v_texCoord).r;
    float expPos = exp(depth * u_evsmExponent);
    float expNeg = exp(-depth * u_evsmExponent);
    fragColor = vec4(expPos, expPos * expPos, expNeg, expNeg * expNeg);
}
)";
}

// ---------------------------------------------------------------------------
// Solar shadow map sampling GLSL
// (Ported from: itwinjs-core glsl/SolarShadowMapping.ts)
// ---------------------------------------------------------------------------
inline char const* getSolarShadowMapSampling()
{
    return R"(
uniform sampler2D s_shadowMap;
uniform mat4 u_shadowMapMatrix;
uniform float u_shadowIntensity;

float sampleShadow(vec3 worldPos) {
    vec4 shadowCoord = u_shadowMapMatrix * vec4(worldPos, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xyz = shadowCoord.xyz * 0.5 + 0.5;

    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
        shadowCoord.y < 0.0 || shadowCoord.y > 1.0) {
        return 1.0;  // Outside shadow map
    }

    float shadowDepth = texture(s_shadowMap, shadowCoord.xy).r;
    float bias = 0.005;
    return shadowCoord.z - bias > shadowDepth ? (1.0 - u_shadowIntensity) : 1.0;
}
)";
}

// ---------------------------------------------------------------------------
// Composite (final combine) fragment shader
// (Ported from: itwinjs-core glsl/Composite.ts)
// ---------------------------------------------------------------------------
inline char const* getCompositeFragmentShader()
{
    return R"(
#version 410 core
in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D s_colorTexture;
uniform sampler2D s_occlusionTexture;
uniform sampler2D s_hiliteTexture;
uniform bool u_hasOcclusion;
uniform bool u_hasHilite;

void main() {
    vec4 color = texture(s_colorTexture, v_texCoord);

    if (u_hasOcclusion) {
        float ao = texture(s_occlusionTexture, v_texCoord).r;
        color.rgb *= ao;
    }

    if (u_hasHilite) {
        vec4 hilite = texture(s_hiliteTexture, v_texCoord);
        color.rgb = mix(color.rgb, hilite.rgb, hilite.a);
    }

    fragColor = color;
}
)";
}

END_DQ_RENDER_NAMESPACE
