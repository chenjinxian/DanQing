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
inline char const* getSsaoFragmentShader()
{
    return R"(
#version 410 core
in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D s_depthTexture;
uniform sampler2D s_normalTexture;
uniform mat4 u_proj;
uniform vec2 u_frustumScale;
uniform float u_radius;
uniform float u_intensity;

const int NUM_SAMPLES = 16;
const vec3 samples[16] = vec3[](
    vec3(0.5381, 0.1856, -0.4319), vec3(0.1379, 0.2486, 0.4430),
    vec3(0.3371, 0.5679, -0.0057), vec3(-0.6999, -0.0451, -0.0019),
    vec3(0.0689, -0.1598, -0.8547), vec3(0.0560, 0.0069, -0.1843),
    vec3(-0.0146, 0.1402, 0.0762), vec3(0.0100, -0.1924, -0.0344),
    vec3(-0.3577, -0.5301, -0.4358), vec3(-0.3169, 0.1063, 0.0158),
    vec3(0.0103, -0.5869, 0.0046), vec3(-0.0897, -0.4940, 0.3287),
    vec3(0.7119, -0.0154, -0.0918), vec3(-0.0533, 0.0596, -0.5411),
    vec3(0.0352, -0.0631, 0.5460), vec3(-0.4776, 0.2847, -0.0271)
);

void main() {
    float depth = texture(s_depthTexture, v_texCoord).r;
    if (depth >= 1.0) { fragColor = vec4(1.0); return; }

    vec3 normal = normalize(texture(s_normalTexture, v_texCoord).rgb * 2.0 - 1.0);
    float occlusion = 0.0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        vec3 sampleDir = reflect(samples[i], normal);
        float sampleDepth = texture(s_depthTexture, v_texCoord + sampleDir.xy * u_radius).r;
        float diff = depth - sampleDepth;
        occlusion += step(0.001, diff) * (1.0 - smoothstep(0.001, u_radius, diff));
    }

    occlusion = 1.0 - (occlusion / float(NUM_SAMPLES)) * u_intensity;
    fragColor = vec4(vec3(occlusion), 1.0);
}
)";
}

// ---------------------------------------------------------------------------
// Gaussian blur fragment shader
// (Ported from: itwinjs-core glsl/Blur.ts)
// ---------------------------------------------------------------------------
inline char const* getBlurFragmentShader()
{
    return R"(
#version 410 core
in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D s_texture;
uniform vec2 u_texelSize;  // 1.0 / textureSize
uniform int u_blurRadius;

void main() {
    vec4 sum = vec4(0.0);
    float totalWeight = 0.0;

    for (int i = -u_blurRadius; i <= u_blurRadius; i++) {
        float weight = 1.0 - abs(float(i)) / float(u_blurRadius + 1);
        vec2 offset = vec2(float(i)) * u_texelSize;
        sum += texture(s_texture, v_texCoord + offset) * weight;
        totalWeight += weight;
    }

    fragColor = sum / totalWeight;
}
)";
}

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
