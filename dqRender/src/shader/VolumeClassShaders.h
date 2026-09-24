// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Volume classification shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//
// Volume classification techniques for planar classification and
// shadow mapping (EVSM).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Reuse kFullscreenQuadVert from PostProcessShaders.h

// ---------------------------------------------------------------------------
// VolClassColorUsingStencil — volume classification with depth comparison
// Ported from: itwinjs-core PlanarClassification.ts volClassOpaqueColor (line 24-30)
// Uses depth comparison instead of simple threshold.
// ---------------------------------------------------------------------------
static char const* kVolClassColorUsingStencilFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_classTexture;
uniform vec4 u_classColor;
out vec4 fragColor;

vec2 windowCoordsToTexCoords(vec2 windowPos) {
    return windowPos / vec2(textureSize(u_colorTexture, 0));
}

void main() {
    vec4 color = texture(u_colorTexture, v_texCoord);
    // Depth comparison: discard if fragment depth <= classification depth
    // Ported from: itwinjs-core PlanarClassification.ts volClassOpaqueColor
    float classDepth = texture(u_classTexture, windowCoordsToTexCoords(gl_FragCoord.xy)).r;
    if (gl_FragCoord.z <= classDepth) discard;
    fragColor = vec4(color.rgb, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// VolClassCopyZ — copy depth for volume classification
// Ported from: itwinjs-core CopyStencil.ts (line 34)
// Outputs vec4 with depth in all channels for proper texture format.
// ---------------------------------------------------------------------------
static char const* kVolClassCopyZFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_depthTexture;
out vec4 fragColor;
void main() {
    float depth = texture(u_depthTexture, v_texCoord).r;
    fragColor = vec4(depth, depth, depth, depth);
}
)glsl";

// ---------------------------------------------------------------------------
// VolClassSetBlend — set blend mode for volume classification
// Ported from: itwinjs-core CopyStencil.ts (line 121-171)
// Includes boundary type checking and depth-based background discard.
// ---------------------------------------------------------------------------
static char const* kVolClassSetBlendFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_classTexture;
uniform sampler2D u_depthTexture;
uniform int u_boundaryType;  // 0=Outside, 1=Inside, 2=Selected
out vec4 fragColor;

const int kBoundaryType_Out = 0;
const int kBoundaryType_In = 1;
const int kBoundaryType_Selected = 2;

void main() {
    float classification = texture(u_classTexture, v_texCoord).r;
    float depth = texture(u_depthTexture, v_texCoord).r;

    // Discard background based on boundary type
    // Ported from: itwinjs-core CopyStencil.ts checkDiscardBackgroundByZ
    if (u_boundaryType == kBoundaryType_Out) {
        if (depth >= 1.0) discard;
    } else if (u_boundaryType == kBoundaryType_In) {
        if (depth < 1.0) discard;
    }
    // kBoundaryType_Selected: no discard

    fragColor = vec4(classification, 0.0, 0.0, 1.0);
}
)glsl";

// ---------------------------------------------------------------------------
// VolClassBlend — blend volume classification with display modes
// Ported from: itwinjs-core PlanarClassification.ts applyPlanarClassificationColor
// Supports Off/On/Dimmed/Hilite/Element display modes.
// ---------------------------------------------------------------------------
static char const* kVolClassBlendFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_classTexture;
uniform vec4 u_classColor;
uniform vec4 u_pClassColorParams;  // x=classifiedDisplay, y=unclassifiedDisplay, z=imageCount, w=maskTransparency
uniform mat3 u_hilite_settings;
out vec4 fragColor;

const float kClassifierDisplay_Off = 0.0;
const float kClassifierDisplay_On = 1.0;
const float kClassifierDisplay_Dimmed = 2.0;
const float kClassifierDisplay_Hilite = 3.0;
const float kClassifierDisplay_Element = 4.0;
const float dimScale = 0.7;

void main() {
    vec4 color = texture(u_colorTexture, v_texCoord);
    float classification = texture(u_classTexture, v_texCoord).r;

    bool isClassified = classification > 0.0;
    float param = isClassified ? u_pClassColorParams.x : u_pClassColorParams.y;

    if (kClassifierDisplay_Off == param) {
        discard;
        return;
    }

    vec4 classColor;
    if (kClassifierDisplay_On == param)
        classColor = color;
    else if (!isClassified || kClassifierDisplay_Dimmed == param)
        classColor = vec4(color.rgb * dimScale, color.a);
    else if (kClassifierDisplay_Hilite == param)
        classColor = vec4(mix(color.rgb, u_hilite_settings[0], u_hilite_settings[2][0]), color.a);
    else
        classColor = mix(color, u_classColor, classification * u_classColor.a);

    fragColor = classColor;
}
)glsl";

// ---------------------------------------------------------------------------
// BlurTestOrder — blur with test order (for debugging)
// ---------------------------------------------------------------------------
static char const* kBlurTestOrderFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_inputTexture;
uniform vec2 u_direction;
uniform vec2 u_texelSize;
out float fragColor;
void main() {
    float result = 0.0;
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
// CombineTextures — combine two textures
// ---------------------------------------------------------------------------
static char const* kCombineTexturesFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform float u_weight0;
uniform float u_weight1;
out vec4 fragColor;
void main() {
    fragColor = texture(u_texture0, v_texCoord) * u_weight0 +
                texture(u_texture1, v_texCoord) * u_weight1;
}
)glsl";

// ---------------------------------------------------------------------------
// Combine3Textures — combine three textures
// ---------------------------------------------------------------------------
static char const* kCombine3TexturesFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_texture0;
uniform sampler2D u_texture1;
uniform sampler2D u_texture2;
uniform float u_weight0;
uniform float u_weight1;
uniform float u_weight2;
out vec4 fragColor;
void main() {
    fragColor = texture(u_texture0, v_texCoord) * u_weight0 +
                texture(u_texture1, v_texCoord) * u_weight1 +
                texture(u_texture2, v_texCoord) * u_weight2;
}
)glsl";

// ---------------------------------------------------------------------------
// EdlCalcFull — full EDL calculation
// ---------------------------------------------------------------------------
static char const* kEdlCalcFullFrag = R"glsl(
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
    if (depth >= 1.0) { fragColor = texture(u_colorTexture, v_texCoord); return; }

    float d = u_radius * u_texelSize.x;
    float n = sampleDepth(vec2(0.0, d));
    float s = sampleDepth(vec2(0.0, -d));
    float e = sampleDepth(vec2(d, 0.0));
    float w = sampleDepth(vec2(-d, 0.0));
    float ne = sampleDepth(vec2(d, d));
    float nw = sampleDepth(vec2(-d, d));
    float se = sampleDepth(vec2(d, -d));
    float sw = sampleDepth(vec2(-d, -d));

    float response = 0.0;
    float maxDepth = max(max(max(n, s), max(e, w)), max(max(ne, nw), max(se, sw)));
    if (maxDepth < 1.0) {
        response = max(0.0, log2(depth) - log2(maxDepth));
    }

    float shade = exp(-response * u_strength * 100.0);
    vec4 color = texture(u_colorTexture, v_texCoord);
    fragColor = vec4(color.rgb * shade, color.a);
}
)glsl";

// ---------------------------------------------------------------------------
// EdlFilter — EDL filter pass
// ---------------------------------------------------------------------------
static char const* kEdlFilterFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_inputTexture;
uniform vec2 u_texelSize;
uniform float u_radius;
out vec4 fragColor;
void main() {
    vec4 result = vec4(0.0);
    float d = u_radius * u_texelSize.x;
    result += texture(u_inputTexture, v_texCoord);
    result += texture(u_inputTexture, v_texCoord + vec2(d, 0.0));
    result += texture(u_inputTexture, v_texCoord + vec2(-d, 0.0));
    result += texture(u_inputTexture, v_texCoord + vec2(0.0, d));
    result += texture(u_inputTexture, v_texCoord + vec2(0.0, -d));
    fragColor = result / 5.0;
}
)glsl";

// ---------------------------------------------------------------------------
// EdlMix — EDL final mix
// ---------------------------------------------------------------------------
static char const* kEdlMixFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
uniform sampler2D u_colorTexture;
uniform sampler2D u_edlTexture;
uniform float u_mixFactor;
out vec4 fragColor;
void main() {
    vec4 color = texture(u_colorTexture, v_texCoord);
    vec4 edl = texture(u_edlTexture, v_texCoord);
    fragColor = mix(color, edl, u_mixFactor);
}
)glsl";

END_DQ_RENDER_NAMESPACE
