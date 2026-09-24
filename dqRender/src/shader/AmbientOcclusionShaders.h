// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Ambient occlusion shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/AmbientOcclusion.ts
//
// HBAO-style screen-space ambient occlusion with:
// - Normal reconstruction from depth (4-sample cross)
// - Noise texture for randomized sampling directions
// - Perspective/orthographic branching
// - Distance fade factor
// - Render order check (skip unlit surfaces)
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Ambient occlusion vertex shader (fullscreen quad)
// ---------------------------------------------------------------------------
static char const* kAmbientOcclusionVert = R"glsl(
#version 410 core
layout(location = 0) in vec4 a_position;
out vec2 v_texCoord;
void main() {
    gl_Position = a_position;
    v_texCoord = (a_position.xy + 1.0) * 0.5;
}
)glsl";

// ---------------------------------------------------------------------------
// Ambient occlusion fragment shader (HBAO)
// Ported from: itwinjs-core AmbientOcclusion.ts (line 60-200)
// ---------------------------------------------------------------------------
static char const* kAmbientOcclusionFrag = R"glsl(
#version 410 core
in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D u_depthBuffer;
uniform sampler2D u_noise;
uniform mat4 u_invProj;
uniform vec4 u_hbaoSettings;   // x=bias, y=zLengthCap, z=intensity, w=texelStepSize
uniform vec4 u_frustumPlanes;  // x=top, y=bottom, z=left, w=right
uniform vec3 u_frustum;        // x=near, y=far, z=cameraType (2.0=perspective)
uniform vec3 u_logZ;           // x=logZ.x, y=logZ.y, z=unused
uniform float u_maxDistance;

const float kFrustumType_Perspective = 2.0;
const float kRenderOrder_LitSurface = 4.0;
uniform float u_renderOrder;

// readDepth — read depth from depth buffer
// Ported from: itwinjs-core AmbientOcclusion.ts readDepthDB
float readDepth(vec2 tc) {
    return texture(u_depthBuffer, tc).r;
}

// unfinalizeLinearDepth — convert finalized depth back to linear depth
// Ported from: itwinjs-core AmbientOcclusion.ts unfinalizeLinearDepthDB
float unfinalizeLinearDepth(float depth) {
    float eyeZ = 0.0 == u_logZ.x ? depth * u_logZ.y : exp(depth * u_logZ.y) / u_logZ.x;
    float near = u_frustum.x, far = u_frustum.y;
    float depthRange = far - near;
    float linearDepth = (eyeZ - near) / depthRange;
    return 1.0 - linearDepth;
}

// computeNonLinearDepth — convert finalized depth to non-linear
// Ported from: itwinjs-core AmbientOcclusion.ts computeNonLinearDepthDB
float computeNonLinearDepth(float depth) {
    return 0.0 == u_logZ.x ? depth * u_logZ.y : exp(depth * u_logZ.y) / u_logZ.x;
}

// computePositionFromDepth — reconstruct eye-space position from depth
// Ported from: itwinjs-core AmbientOcclusion.ts computePositionFromDepth (line 130-143)
vec4 computePositionFromDepth(vec2 tc, float nonLinearDepth) {
    if (kFrustumType_Perspective == u_frustum.z) {
        vec2 xy = vec2((tc.x * 2.0 - 1.0), ((1.0 - tc.y) * 2.0 - 1.0));
        vec4 posEC = u_invProj * vec4(xy, nonLinearDepth, 1.0);
        posEC = posEC / posEC.w;
        return posEC;
    } else {
        float top = u_frustumPlanes.x;
        float bottom = u_frustumPlanes.y;
        float left = u_frustumPlanes.z;
        float right = u_frustumPlanes.w;
        return vec4(mix(left, right, tc.x), mix(bottom, top, tc.y), nonLinearDepth, 1.0);
    }
}

// computeNormalFromDepth — reconstruct view-space normals from depth
// Ported from: itwinjs-core AmbientOcclusion.ts computeNormalFromDepth (line 147-168)
vec3 computeNormalFromDepth(vec3 viewPos, vec2 tc, vec2 pixelSize) {
    float nonLinearDepthU = computeNonLinearDepth(readDepth(tc - vec2(0.0, pixelSize.y)));
    float nonLinearDepthD = computeNonLinearDepth(readDepth(tc + vec2(0.0, pixelSize.y)));
    float nonLinearDepthL = computeNonLinearDepth(readDepth(tc - vec2(pixelSize.x, 0.0)));
    float nonLinearDepthR = computeNonLinearDepth(readDepth(tc + vec2(pixelSize.x, 0.0)));

    vec3 viewPosUp = computePositionFromDepth(tc - vec2(0.0, pixelSize.y), nonLinearDepthU).xyz;
    vec3 viewPosDown = computePositionFromDepth(tc + vec2(0.0, pixelSize.y), nonLinearDepthD).xyz;
    vec3 viewPosLeft = computePositionFromDepth(tc - vec2(pixelSize.x, 0.0), nonLinearDepthL).xyz;
    vec3 viewPosRight = computePositionFromDepth(tc + vec2(pixelSize.x, 0.0), nonLinearDepthR).xyz;

    vec3 up = viewPos.xyz - viewPosUp.xyz;
    vec3 down = viewPosDown.xyz - viewPos.xyz;
    vec3 left = viewPos.xyz - viewPosLeft.xyz;
    vec3 right = viewPosRight.xyz - viewPos.xyz;

    vec3 dx = length(left) < length(right) ? left : right;
    vec3 dy = length(up) < length(down) ? up : down;

    return normalize(cross(dy, dx));
}

void main() {
    float db = readDepth(v_texCoord);
    float linearDepth = unfinalizeLinearDepth(db);
    float nonLinearDepth = computeNonLinearDepth(db);

    // Skip unlit surfaces
    // Ported from: itwinjs-core AmbientOcclusion.ts line 44-49
    if (u_renderOrder < kRenderOrder_LitSurface) {
        fragColor = vec4(1.0);
        return;
    }

    vec3 viewPos = computePositionFromDepth(v_texCoord, nonLinearDepth).xyz;
    vec2 pixelSize = 1.0 / vec2(textureSize(u_depthBuffer, 0));
    vec3 viewNormal = computeNormalFromDepth(viewPos, v_texCoord, pixelSize);

    vec2 sampleDirection = vec2(1.0, 0.0);
    float gapAngle = 90.0 * 0.017453292519943295; // radians per degree

    // Noise texture for randomized sampling
    // Ported from: itwinjs-core AmbientOcclusion.ts line 69
    vec3 noiseVec = (texture(u_noise, v_texCoord * vec2(textureSize(u_depthBuffer, 0)) / 4.0).rgb + 1.0) / 2.0;

    float bias = u_hbaoSettings.x;
    float zLengthCap = u_hbaoSettings.y;
    float intensity = u_hbaoSettings.z;
    float texelStepSize = clamp(u_hbaoSettings.w * linearDepth, 1.0, u_hbaoSettings.w);

    float tOcclusion = 0.0;

    // Loop for each direction (4 directions)
    // Ported from: itwinjs-core AmbientOcclusion.ts line 86-117
    for (int i = 0; i < 4; i++) {
        float newGapAngle = gapAngle * (float(i) + noiseVec.x);
        float cosVal = cos(newGapAngle);
        float sinVal = sin(newGapAngle);

        vec2 rotatedSampleDirection = vec2(
            cosVal * sampleDirection.x - sinVal * sampleDirection.y,
            sinVal * sampleDirection.x + cosVal * sampleDirection.y
        );
        float curOcclusion = 0.0;
        float curStepSize = texelStepSize;

        for (int j = 0; j < 6; j++) {
            vec2 directionWithStep = vec2(
                rotatedSampleDirection.x * curStepSize * pixelSize.x,
                rotatedSampleDirection.y * curStepSize * pixelSize.y
            );
            vec2 newCoords = directionWithStep + v_texCoord;

            if (newCoords.x > 1.0 || newCoords.y > 1.0 || newCoords.x < 0.0 || newCoords.y < 0.0)
                break;

            float curDepth = readDepth(newCoords);
            float curLinearDepth = unfinalizeLinearDepth(curDepth);
            float curNonLinearDepth = computeNonLinearDepth(curDepth);
            vec3 curViewPos = computePositionFromDepth(newCoords, curNonLinearDepth).xyz;
            vec3 diffVec = curViewPos.xyz - viewPos.xyz;
            float zLength = abs(curLinearDepth - linearDepth);

            float dotVal = clamp(dot(viewNormal, normalize(diffVec)), 0.0, 1.0);
            float weight = smoothstep(0.0, 1.0, zLengthCap / zLength);

            if (dotVal < bias)
                dotVal = 0.0;

            curOcclusion = max(curOcclusion, dotVal * weight);
            curStepSize += texelStepSize;
        }
        tOcclusion += curOcclusion;
    }

    // Distance fade factor
    // Ported from: itwinjs-core AmbientOcclusion.ts line 119
    float distanceFadeFactor = kFrustumType_Perspective == u_frustum.z
        ? 1.0 - pow(clamp(nonLinearDepth / u_maxDistance, 0.0, 1.0), 4.0)
        : 1.0;
    tOcclusion *= distanceFadeFactor;

    tOcclusion /= 4.0;
    tOcclusion = 1.0 - clamp(tOcclusion, 0.0, 1.0);
    tOcclusion = pow(tOcclusion, intensity);

    fragColor = vec4(tOcclusion, tOcclusion, tOcclusion, 1.0);
}
)glsl";

END_DQ_RENDER_NAMESPACE
