// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Remaining GLSL shader modules
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/
//
// Remaining shader building blocks: Lighting, Clipping, Monochrome,
// Translucency, LogarithmicDepthBuffer, Viewport, Common utilities.
#pragma once

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Lighting GLSL (Ported from: itwinjs-core glsl/Lighting.ts)
// ---------------------------------------------------------------------------

/// Directional light + ambient lighting
inline char const* getLightingFunctions()
{
    return R"(
struct Light {
    vec3 direction;
    float intensity;
    vec3 ambient;
};

vec3 computeLighting(vec3 normal, Light light) {
    float nDotL = max(dot(normal, light.direction), 0.0);
    return light.ambient + light.intensity * nDotL;
}

vec3 computeLightingWithSpecular(vec3 normal, vec3 viewDir, Light light, float shininess) {
    float nDotL = max(dot(normal, light.direction), 0.0);
    vec3 diffuse = light.intensity * nDotL;

    vec3 reflectDir = reflect(-light.direction, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = light.intensity * spec * vec3(1.0);

    return light.ambient + diffuse + specular;
}
)";
}

// ---------------------------------------------------------------------------
// Clipping GLSL (Ported from: itwinjs-core glsl/Clipping.ts)
// ---------------------------------------------------------------------------

/// Clip plane evaluation
inline char const* getClippingFunctions()
{
    return R"(
uniform int u_numClipPlanes;
uniform vec4 u_clipPlanes[6];

float computeClipDistance(vec4 position) {
    float minDist = 1.0;
    for (int i = 0; i < u_numClipPlanes; i++) {
        float dist = dot(position.xyz, u_clipPlanes[i].xyz) + u_clipPlanes[i].w;
        minDist = min(minDist, dist);
    }
    return minDist;
}

bool isClipped(vec4 position) {
    return computeClipDistance(position) < 0.0;
}
)";
}

// ---------------------------------------------------------------------------
// Monochrome GLSL (Ported from: itwinjs-core glsl/Monochrome.ts)
// ---------------------------------------------------------------------------

/// Monochrome color application
inline char const* getMonochromeFunctions()
{
    return R"(
uniform bool u_monochromeEnabled;
uniform vec3 u_monochromeColor;

vec4 applyMonochrome(vec4 color) {
    if (u_monochromeEnabled) {
        float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
        return vec4(gray * u_monochromeColor, color.a);
    }
    return color;
}
)";
}

// ---------------------------------------------------------------------------
// Translucency GLSL (Ported from: itwinjs-core glsl/Translucency.ts)
// ---------------------------------------------------------------------------

/// OIT translucency accumulation
inline char const* getTranslucencyFunctions()
{
    return R"(
// Weighted Blended OIT (McGuire 2013)
vec4 oitAccumulate(vec4 color, float depth) {
    float weight = max(min(1.0, max(max(color.r, color.g), color.b) * color.a + 0.01),
                       color.a) * pow(1.0 - depth * 0.9, 3.0);
    return vec4(color.rgb * color.a * weight, color.a);
}

vec4 oitComposite(vec4 accum, float revealage) {
    float r = accum.a;
    if (r > 0.0) {
        return vec4(accum.rgb / r, revealage);
    }
    return vec4(0.0);
}
)";
}

// ---------------------------------------------------------------------------
// Logarithmic Depth Buffer GLSL (Ported from: itwinjs-core glsl/LogarithmicDepthBuffer.ts)
// ---------------------------------------------------------------------------

/// Logarithmic depth buffer for improved depth precision
inline char const* getLogDepthFunctions()
{
    return R"(
uniform vec2 u_logZ;  // (1/near, log(far/near))

float computeLogDepth(float depth) {
    return log(depth * u_logZ.x) * u_logZ.y;
}

vec4 applyLogDepth(vec4 position, float depth) {
    position.z = computeLogDepth(depth) * position.w;
    return position;
}
)";
}

// ---------------------------------------------------------------------------
// Common Utilities GLSL (Ported from: itwinjs-core glsl/Common.ts)
// ---------------------------------------------------------------------------

/// Common utility functions
inline char const* getCommonUtilities()
{
    return R"(
// Bit flag extraction
bool nthBitSet(float flags, int bit) {
    float pow2 = pow(2.0, float(bit));
    return mod(floor(flags / pow2), 2.0) >= 1.0;
}

// Viewport transformation
uniform vec2 u_viewport;

vec2 windowToTexCoords(vec2 windowPos) {
    return windowPos / u_viewport;
}

// Frustum uniforms
uniform vec3 u_frustum;  // (near, far, type)

float getNearPlane() { return u_frustum.x; }
float getFarPlane() { return u_frustum.y; }
)";
}

// ---------------------------------------------------------------------------
// Viewport GLSL (Ported from: itwinjs-core glsl/Viewport.ts)
// ---------------------------------------------------------------------------

/// Viewport transformation
inline char const* getViewportFunctions()
{
    return R"(
uniform vec2 u_viewport;
uniform mat4 u_viewportTransformation;

vec4 modelToWindowCoordinates(vec4 mvpPos) {
    vec3 ndc = mvpPos.xyz / mvpPos.w;
    vec2 windowPos = (ndc.xy * 0.5 + 0.5) * u_viewport;
    return vec4(windowPos, ndc.z, 1.0);
}

vec2 windowToNdc(vec2 windowPos) {
    return windowPos / u_viewport * 2.0 - 1.0;
}
)";
}

END_DQ_RENDER_NAMESPACE
