// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Sky rendering shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/SkyBox.ts
//
// SkyBox: cubemap sampling
// SkySphereGradient: procedural gradient sky
// SkySphereTexture: equirectangular map
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SkyBox vertex shader
// ---------------------------------------------------------------------------
static char const* kSkyBoxVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_mvp;

out vec3 v_texCoord;

void main()
{
    v_texCoord = a_position;
    vec4 pos = u_mvp * vec4(a_position, 1.0);
    gl_Position = pos.xyww;  // depth = 1.0 (far plane)
}
)glsl";

// ---------------------------------------------------------------------------
// SkyBox fragment shader (cubemap)
// ---------------------------------------------------------------------------
static char const* kSkyBoxFrag = R"glsl(
#version 410 core

in vec3 v_texCoord;

uniform samplerCube u_skyTexture;

out vec4 fragColor;

void main()
{
    fragColor = texture(u_skyTexture, v_texCoord);
}
)glsl";

// ---------------------------------------------------------------------------
// SkySphereGradient vertex shader
// ---------------------------------------------------------------------------
static char const* kSkySphereGradientVert = R"glsl(
#version 410 core

// Faithful port of itwinjs-core glsl/SkySphere.ts (SkySphereGradient). The
// gradient is the ELEVATION ANGLE of the eye→vertex direction (a_worldPos -
// u_worldEye), so the sky is world-fixed and rotates with the view (not
// screen-fixed). Ported from: SkySphere.ts computeGradientValue +
// ViewportQuad a_worldPos attribute (CachedGeometry.ts:583-596 non-globe).
layout(location = 0) in vec3 a_position;   // rawPosition (NDC fullscreen quad)
layout(location = 1) in vec3 a_worldPos;   // world corner of frustum at mid-depth

uniform mat4 u_mvp;          // identity (fullscreen far-plane quad via .xyww)
uniform vec3 u_worldEye;     // ortho pseudo-camera position (world)
uniform vec3 u_skyParams;    // x<0 = 2-color; else (y,z) = sky/ground exponents
uniform float u_zOffset;

out vec4 v_gradientValue;

const float horizonSize = 0.0015;  // SkySphere.ts addGlobal horizonSize

vec4 computeGradientValue() {
    // Ported from: itwinjs-core SkySphere.ts computeGradientValue.
    vec3 eyeToVert = a_worldPos - u_worldEye;
    float radius = sqrt(eyeToVert.x * eyeToVert.x + eyeToVert.y * eyeToVert.y);
    float zValue = eyeToVert.z - radius * u_zOffset;
    float d = atan(zValue, radius);
    if (u_skyParams.x < 0.0) {  // 2-color gradient
        d = 0.5 - d / 3.14159265359;
        return vec4(d, 0.0, 0.0, 0.0);
    }
    d = d / 1.570796326795;
    return vec4(d,
                1.0 - (d - horizonSize) / (1.0 - horizonSize),
                1.0 - (-d - horizonSize) / (1.0 - horizonSize),
                (d + horizonSize) / (horizonSize * 2.0));
}

void main() {
    v_gradientValue = computeGradientValue();
    vec4 pos = u_mvp * vec4(a_position, 1.0);
    gl_Position = pos.xyww;  // push to far plane
}
)glsl";

// ---------------------------------------------------------------------------
// SkySphereGradient fragment shader (procedural)
// ---------------------------------------------------------------------------
static char const* kSkySphereGradientFrag = R"glsl(
#version 410 core

// Faithful port of itwinjs-core SkySphere.ts computeSkySphereColorGradient.
in vec4 v_gradientValue;

uniform vec3 u_skyParams;     // x<0 = 2-color
uniform vec3 u_zenithColor;
uniform vec3 u_skyColor;
uniform vec3 u_groundColor;
uniform vec3 u_nadirColor;

out vec4 fragColor;

const float horizonSize = 0.0015;

vec4 computeSkySphereColorGradient() {
    if (u_skyParams.x < 0.0)  // 2-color: zenith ↔ nadir
        return vec4(mix(u_zenithColor, u_nadirColor, v_gradientValue.x), 1.0);
    if (v_gradientValue.x > horizonSize)  // above horizon
        return vec4(mix(u_zenithColor, u_skyColor, pow(v_gradientValue.y, u_skyParams.y)), 1.0);
    else if (v_gradientValue.x < -horizonSize)  // below horizon
        return vec4(mix(u_nadirColor, u_groundColor, pow(v_gradientValue.z, u_skyParams.z)), 1.0);
    return vec4(mix(u_groundColor, u_skyColor, v_gradientValue.w), 1.0);  // horizon band
}

void main() {
    fragColor = computeSkySphereColorGradient();
}
)glsl";

// ---------------------------------------------------------------------------
// SkySphereTexture vertex shader (equirectangular) — self-contained.
// Unused by the blank-connection gradient sky; kept so SkySphereTextureTechnique
// compiles. Outputs v_direction (the texture frag maps it to equirect UV).
// ---------------------------------------------------------------------------
static char const* kSkySphereTextureVert = R"glsl(
#version 410 core

layout(location = 0) in vec3 a_position;

uniform mat4 u_mvp;

out vec3 v_direction;

void main() {
    v_direction = normalize(a_position);
    vec4 pos = u_mvp * vec4(a_position, 1.0);
    gl_Position = pos.xyww;
}
)glsl";

// ---------------------------------------------------------------------------
// SkySphereTexture fragment shader
// ---------------------------------------------------------------------------
static char const* kSkySphereTextureFrag = R"glsl(
#version 410 core

in vec3 v_direction;

uniform sampler2D u_skyTexture;

out vec4 fragColor;

void main()
{
    vec3 dir = normalize(v_direction);

    // Convert direction to equirectangular UV
    float u = atan(dir.x, dir.z) / (2.0 * 3.14159265) + 0.5;
    float v = asin(dir.y) / 3.14159265 + 0.5;

    fragColor = texture(u_skyTexture, vec2(u, v));
}
)glsl";

END_DQ_RENDER_NAMESPACE
