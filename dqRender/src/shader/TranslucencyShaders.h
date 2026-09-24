// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Translucency (weighted blended OIT) shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Translucency.ts
//
// Weighted Blended Order-Independent Transparency using Equation 10 from:
// http://jcgt.org/published/0002/02/09/
// Outputs to 2 draw buffers (MRT) for pre-multiplied alpha compositing.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Translucency vertex shader
// ---------------------------------------------------------------------------
// Attributes: a_position (vec3), a_normal (vec3), a_color (vec4)
// Uniforms:   u_mvp (mat4), u_mv (mat4)
// Varyings:   v_color (vec4), v_eyeSpace (vec3)
// Ported from: itwinjs-core Translucency.ts addEyeSpace / addModelViewMatrix
static char const* kTranslucencyVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;

uniform mat4 u_mvp;
uniform mat4 u_mv;

out vec4 v_color;
out vec3 v_eyeSpace;

void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_eyeSpace = (u_mv * vec4(a_position, 1.0)).xyz;
    v_color = a_color;
}
)glsl";

// ---------------------------------------------------------------------------
// Translucency fragment shader
// ---------------------------------------------------------------------------
// Ported from: itwinjs-core Translucency.ts
//   - computeLinearDepth: linearizes perspective depth to [0, 1]
//   - computeAlphaWeight: Equation 10 from weighted blended OIT paper
//   - computeOutputs: pre-multiplied alpha OIT, writes to 2 draw buffers
static char const* kTranslucencyFrag = R"glsl(
#version 410 core

in vec4 v_color;
in vec3 v_eyeSpace;

uniform vec4 u_frustum;  // x = near, y = far
uniform bool u_oitFlatAlphaWeight;
uniform bool u_oitScaleOutput;

layout(location = 0) out vec4 fragColor0;
layout(location = 1) out vec4 fragColor1;

// Linearize perspective depth to [0, 1] range
// Ported from: itwinjs-core Fragment.ts computeLinearDepth
float computeLinearDepth(float eyeSpaceZ) {
    float eyeZ = -eyeSpaceZ;
    float near = u_frustum.x;
    float far = u_frustum.y;
    float depthRange = far - near;
    float linearDepth = (eyeZ - near) / depthRange;
    return 1.0 - linearDepth;
}

// Compute alpha weight for OIT compositing
// Ported from: itwinjs-core Translucency.ts computeAlphaWeight
// Uses Equation 10 from http://jcgt.org/published/0002/02/09/
// To avoid excessively low weight for fragments close to the far plane,
// scale depth to [0.15, 1.0].
float computeAlphaWeight(float a) {
    float d = computeLinearDepth(v_eyeSpace.z) * 0.85 + 0.15;
    float z = u_oitFlatAlphaWeight ? 1.0 : d;
    return pow(a + 0.01, 4.0) + max(1e-2, 3.0 * 1e3 * pow(z, 3.0));
}

void main()
{
    vec4 baseColor = v_color;

    // Pre-multiplied alpha
    vec3 Ci = baseColor.rgb * baseColor.a;
    float ai = min(0.99, baseColor.a); // OIT algorithm does not nicely handle a=1
    float wzi = computeAlphaWeight(ai);

    float outputScale = u_oitScaleOutput ? (1.0 / 3001.040604) : 1.0;
    vec4 output0 = vec4(Ci * wzi * outputScale, ai);
    vec4 output1 = vec4(ai * wzi * outputScale, 0.0, 0.0, ai * wzi * outputScale);

    fragColor0 = output0;
    fragColor1 = output1;
}
)glsl";

END_DQ_RENDER_NAMESPACE
