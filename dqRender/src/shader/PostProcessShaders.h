// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Post-processing shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//
// Contains shaders for:
// - Ambient Occlusion (SSAO)
// - Blur (Gaussian)
// - EDL (Eye-Dome Lighting)
// - Composite (final image composition)
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Common fullscreen quad vertex shader
// ---------------------------------------------------------------------------
static char const* kFullscreenQuadVert = R"glsl(
#version 410 core
layout(location = 0) in vec2 a_position;
out vec2 v_texCoord;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
    v_texCoord = a_position * 0.5 + 0.5;
}
)glsl";

// ---------------------------------------------------------------------------
// Ambient Occlusion (SSAO) fragment shader
// ---------------------------------------------------------------------------
static char const* kSsaoFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;

uniform sampler2D u_depthTexture;
uniform sampler2D u_normalTexture;
uniform mat4 u_proj;
uniform mat4 u_projInv;
uniform vec2 u_screenSize;
uniform float u_radius;
uniform float u_bias;
uniform float u_intensity;

out float fragColor;

// Hemisphere sample kernel (simplified)
const vec3 kernel[16] = vec3[](
    vec3(0.5381, 0.1856, -0.4319), vec3(0.1379, 0.2486, 0.4430),
    vec3(0.3371, 0.5679, -0.0057), vec3(-0.6999, -0.0451, -0.0019),
    vec3(0.0689, -0.1598, -0.8547), vec3(0.0560, 0.0069, -0.1843),
    vec3(-0.0146, 0.1402, 0.0762), vec3(0.0103, -0.1924, 0.0344),
    vec3(-0.3577, -0.5301, -0.4358), vec3(-0.3169, 0.1063, 0.0158),
    vec3(0.0103, -0.5869, 0.0046), vec3(-0.0897, -0.4940, 0.3287),
    vec3(0.7119, -0.0154, -0.0918), vec3(-0.0533, 0.0596, -0.5411),
    vec3(0.0352, -0.0631, 0.5460), vec3(-0.4776, 0.2847, -0.0271)
);

void main() {
    float depth = texture(u_depthTexture, v_texCoord).r;
    if (depth >= 1.0) { fragColor = 1.0; return; }

    vec3 normal = normalize(texture(u_normalTexture, v_texCoord).rgb * 2.0 - 1.0);

    // Reconstruct view-space position from depth
    vec4 clipPos = vec4(v_texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_projInv * clipPos;
    viewPos /= viewPos.w;

    float occlusion = 0.0;
    for (int i = 0; i < 16; i++) {
        // Orient kernel along normal
        vec3 sampleDir = reflect(kernel[i], normal);
        vec3 samplePos = viewPos.xyz + sampleDir * u_radius;

        // Project sample to screen
        vec4 offset = u_proj * vec4(samplePos, 1.0);
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        // Sample depth at offset
        float sampleDepth = texture(u_depthTexture, offset.xy).r;
        vec4 sampleClip = vec4(offset.xy * 2.0 - 1.0, sampleDepth * 2.0 - 1.0, 1.0);
        vec4 sampleView = u_projInv * sampleClip;
        sampleView /= sampleView.w;

        // Range check and accumulate
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(viewPos.z - sampleView.z));
        occlusion += (sampleView.z >= samplePos.z + u_bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / 16.0) * u_intensity;
    fragColor = occlusion;
}
)glsl";

// ---------------------------------------------------------------------------
// Gaussian Blur fragment shader
// ---------------------------------------------------------------------------
static char const* kBlurFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;

uniform sampler2D u_inputTexture;
uniform vec2 u_direction;  // (1,0) for horizontal, (0,1) for vertical
uniform vec2 u_texelSize;

out float fragColor;

void main() {
    float result = 0.0;
    // 9-tap Gaussian kernel
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    result += texture(u_inputTexture, v_texCoord).r * weights[0];
    for (int i = 1; i < 5; i++) {
        vec2 offset = u_direction * u_texelSize * float(i);
        result += texture(u_inputTexture, v_texCoord + offset).r * weights[i];
        result += texture(u_inputTexture, v_texCoord - offset).r * weights[i];
    }
    fragColor = result;
}
)glsl";

// ---------------------------------------------------------------------------
// EDL (Eye-Dome Lighting) fragment shader
// ---------------------------------------------------------------------------
static char const* kEdlFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;

uniform sampler2D u_depthTexture;
uniform sampler2D u_colorTexture;
uniform vec2 u_texelSize;
uniform float u_strength;
uniform float u_radius;

out vec4 fragColor;

float sampleDepth(vec2 offset) {
    return texture(u_depthTexture, v_texCoord + offset).r;
}

void main() {
    float depth = sampleDepth(vec2(0.0));
    if (depth >= 1.0) {
        fragColor = texture(u_colorTexture, v_texCoord);
        return;
    }

    // Sample neighbors
    float d = u_radius * u_texelSize.x;
    float n = sampleDepth(vec2(0.0, d));
    float s = sampleDepth(vec2(0.0, -d));
    float e = sampleDepth(vec2(d, 0.0));
    float w = sampleDepth(vec2(-d, 0.0));

    // Compute EDL response
    float response = 0.0;
    float maxDepth = max(max(n, s), max(e, w));
    if (maxDepth < 1.0) {
        response = max(0.0, log2(depth) - log2(maxDepth));
    }

    float shade = exp(-response * u_strength * 100.0);
    vec4 color = texture(u_colorTexture, v_texCoord);
    fragColor = vec4(color.rgb * shade, color.a);
}
)glsl";

// ---------------------------------------------------------------------------
// Composite shader — combines main render with AO and hilite
// ---------------------------------------------------------------------------
static char const* kCompositeFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;

uniform sampler2D u_colorTexture;
uniform sampler2D u_aoTexture;
uniform sampler2D u_hiliteTexture;
uniform bool u_hasAO;
uniform bool u_hasHilite;

out vec4 fragColor;

void main() {
    vec4 color = texture(u_colorTexture, v_texCoord);

    // Apply AO
    if (u_hasAO) {
        float ao = texture(u_aoTexture, v_texCoord).r;
        color.rgb *= ao;
    }

    // add hilite
    if (u_hasHilite) {
        vec4 hilite = texture(u_hiliteTexture, v_texCoord);
        color.rgb += hilite.rgb * hilite.a;
    }

    fragColor = color;
}
)glsl";

END_DQ_RENDER_NAMESPACE
