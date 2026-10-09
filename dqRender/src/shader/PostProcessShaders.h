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
