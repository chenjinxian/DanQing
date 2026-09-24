// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PlanarGrid shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/PlanarGrid.ts
//
// Grid line rendering with fwidth-based anti-aliased lines.
// Uses unquantize2d for texture coordinates and drawGridLine for line rendering.
// OIT dual-MRT pre-multiplied output per Translucency.ts addTranslucency
// (PlanarGrid.ts:78 calls addTranslucency(builder) — without it the fragment
// outputs straight alpha while the translucent pass blends pre-multiplied).
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PlanarGrid vertex shader
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core PlanarGrid.ts (computePosition + computeTexCoord
//              + Translucency.ts addEyeSpace)
// Attributes: a_position (vec3), a_uvParam (vec2)
// Uniforms:   u_mvp (mat4), u_mv (mat4), u_qTexCoordParams (vec4)
// Varyings:   v_texCoord (vec2), v_eyeSpace (vec3)
static char const* kPlanarGridVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uvParam;

uniform mat4 u_mvp;
uniform mat4 u_mv;
uniform vec4 u_qTexCoordParams;

out vec2 v_texCoord;
out vec3 v_eyeSpace;

vec2 unquantize2d(vec2 qpos, vec4 params) { return params.xy + params.zw * qpos; }

void main()
{
    gl_PointSize = 1.0;
    v_texCoord = unquantize2d(a_uvParam, u_qTexCoordParams);
    gl_Position = u_mvp * vec4(a_position, 1.0);
    // Translucency.ts addEyeSpace — OIT weight needs eye-space z.
    v_eyeSpace = (u_mv * vec4(a_position, 1.0)).xyz;
}
)glsl";

// ---------------------------------------------------------------------------
// PlanarGrid fragment shader
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core PlanarGrid.ts (drawGridLine + computeBaseColor)
//              + Translucency.ts (computeAlphaWeight + computeOutputs)。
// fwidth-based anti-aliased lines; OIT pre-multiplied dual-MRT output.
static char const* kPlanarGridFrag = R"glsl(
#version 410 core

in vec2 v_texCoord;
in vec3 v_eyeSpace;

uniform vec3 u_gridColor;     // grid line color (RGB)
uniform vec4 u_gridProps;     // x = gridsPerRef, y = planeAlpha, z = lineAlpha, w = refAlpha
uniform vec3 u_frustum;       // near, far, type（CommonShaders addFrustum 供值）

layout(location = 0) out vec4 fragColor0;   // OIT accumulation
layout(location = 1) out vec4 fragColor1;   // OIT revealage

// screenSpaceDeriv — 2D fwidth helper
// Ported from: itwinjs-core PlanarGrid.ts fwidth2d
vec2 screenSpaceDeriv(vec2 screenXY) { return fwidth(screenXY); }

// drawGridLine — fract/fwidth based anti-aliased grid line
// Ported from: itwinjs-core PlanarGrid.ts drawGridLine
bool drawGridLine(inout vec4 color, float mult, float alphaScale) {
    vec2 deriv = mult * screenSpaceDeriv(v_texCoord);
    if (deriv.x != 0.0 && deriv.y != 0.0) {
        vec2 grid = abs(fract(mult * v_texCoord - 0.5) - 0.5) / deriv;
        float line = min(grid.x, grid.y);
        if (line < 1.0) {
            color.a += alphaScale * (1.0 - min(line, 1.0)) / max(1.0, length(deriv));
            return true;
        }
    }
    return false;
}

// computeLinearDepth — linearize eye-space depth to [0,1]
// Ported from: itwinjs-core Fragment.ts computeLinearDepth（经 TranslucencyShaders 同式）
float computeLinearDepth(float eyeSpaceZ) {
    float eyeZ = -eyeSpaceZ;
    float near = u_frustum.x;
    float far = u_frustum.y;
    float linearDepth = (eyeZ - near) / (far - near);
    return 1.0 - linearDepth;
}

// computeAlphaWeight — WBOIT Equation 10 weight
// Ported from: itwinjs-core Translucency.ts computeAlphaWeight（flat 位默认 false）
float computeAlphaWeight(float a) {
    float d = computeLinearDepth(v_eyeSpace.z) * 0.85 + 0.15;
    float z = d;   // kShaderBit_OITFlatAlphaWeight 默认 false
    return pow(a + 0.01, 4.0) + max(1e-2, 3.0 * 1e3 * pow(z, 3.0));
}

void main()
{
    // Ported from: itwinjs-core PlanarGrid.ts computeBaseColor (glsl/PlanarGrid.ts:25-33).
    // u_gridProps - x = gridsPerRef, y = planeAlpha, z = lineAlpha, w = refAlpha.
    vec4 color = vec4(u_gridColor, u_gridProps.y);
    float refsPerGrid = u_gridProps.x;
    if (0.0 == refsPerGrid || !drawGridLine(color, 1.0 / refsPerGrid, u_gridProps.w - color.a))
        drawGridLine(color, 1.0, u_gridProps.z - color.a);


    // Ported from: itwinjs-core Translucency.ts computeOutputs（pre-multiplied OIT）。
    vec3 Ci = color.rgb * color.a;
    float ai = min(0.99, color.a);
    float wzi = computeAlphaWeight(ai);
    fragColor0 = vec4(Ci * wzi, ai);
    fragColor1 = vec4(ai * wzi, 0.0, 0.0, ai * wzi);
}
)glsl";

END_DQ_RENDER_NAMESPACE
