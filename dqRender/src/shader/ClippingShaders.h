// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Clipping shader sources
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Clipping.ts
//
// Clip plane shader functions supporting UnionOfConvexClipPlaneSets.
// Planes are read from a texture (row-major). Sentinel values encode
// set boundaries:
//   plane.x == 2.0          → start of a new union set
//   plane.xyz == vec3(0.0)  → start of a new intersection set within current union
// A fragment is clipped only when every intersection set in every union set clips it.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// unpackFloat — IEEE float unpacking from RGBA bytes
// Ported from: itwinjs-core Clipping.ts unpackFloat (line 23-35)
// ---------------------------------------------------------------------------
static char const* kUnpackFloat = R"glsl(
float unpackFloat(vec4 v) {
    const float bias = 38.0;
    v = floor(v * 255.0 + 0.5);
    float temp = v.w / 2.0;
    float exponent = floor(temp);
    float sign = (temp - exponent) * 2.0;
    exponent = exponent - bias;
    sign = -(sign * 2.0 - 1.0);
    float unpacked = dot(sign * v.xyz, vec3(1.0 / 256.0, 1.0 / 65536.0, 1.0 / 16777216.0));
    return unpacked * pow(10.0, exponent);
}
)glsl";

// ---------------------------------------------------------------------------
// Clipping functions (included by other shaders via #include)
// ---------------------------------------------------------------------------
// Requires varyings: v_eyeSpace (vec3) — eye-space position of fragment.
// Requires uniforms from other shader modules:
//   u_frustum (vec3) — {near, far, cameraType}, cameraType 2.0 = perspective.
//
// Our uniforms:
//   u_clipParams[3] — [0]=startIndex, [1]=endIndex (one past), [2]=textureHeight
//   u_outsideRgba (vec4) — clip-outside color (alpha > 0 → output color, else discard)
//   u_insideRgba  (vec4) — clip-inside  color (alpha > 0 → output color, else skip)
//   s_clipSampler (sampler2D) — clip planes, one per row (row-major texelFetch)
//   u_colorizeIntersection (bool) — enable edge highlighting near clip planes
//   u_clipIntersection (vec4) — intersection highlight color + width (.a = width scale)
//   u_pixelWidthFactor (float) — screen-space pixel width for intersection width calc
//
// Sentinel protocol (encoded by C++ clip-plane upload):
//   plane.x == 2.0          → start of a new union set
//   plane.xyz == vec3(0.0)  → start of a new intersection set within current union
// ---------------------------------------------------------------------------

static char const* kClippingFunctions = R"glsl(
    // Frustum camera type constants
    const float kFrustumType_Perspective = 2.0;

    // Clip plane uniforms
    uniform int u_clipParams[3];  // [0]=startIndex, [1]=endIndex, [2]=textureHeight
    uniform vec4 u_outsideRgba;
    uniform vec4 u_insideRgba;
    uniform sampler2D s_clipSampler;

    // Intersection colorization uniforms
    uniform bool u_colorizeIntersection;
    uniform vec4 u_clipIntersection;   // rgb = color, a = width scale
    uniform float u_pixelWidthFactor;

    // Read clip plane from texture (row-major: plane at row = index).
    // Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Clipping.ts
    //   getClipPlaneFloat
    vec4 getClipPlane(int index) {
        return texelFetch(s_clipSampler, ivec2(0, index), 0);
    }

    // Signed distance from eye-space point to clip plane.
    // Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Clipping.ts
    //   calcClipPlaneDist
    float calcClipPlaneDist(vec3 camPos, vec4 plane) {
        return dot(vec4(camPos, 1.0), plane);
    }

    // Apply UnionOfConvexClipPlaneSets clipping.
    // Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Clipping.ts
    //   applyClipPlanesPrelude + applyClipPlanesPostlude + applyClipPlanesLoopBody
    //   + applyClipPlanesIntersectionLoopBody
    //
    // A fragment is clipped when every intersection set in every union set clips it.
    // Sentinel values in the plane texture encode set boundaries (see header comment).
    bvec2 applyClipPlanes() {
        int numPlaneSets = 1;
        int numSetsClippedBy = 0;
        bool clippedByCurrentPlaneSet = false;
        bool colorizeIntersection = false;

        if (u_colorizeIntersection) {
            float widthFactor = u_pixelWidthFactor * 2.0 * u_clipIntersection.a;

            for (int i = u_clipParams[0]; i < u_clipParams[1]; i++) {
                vec4 plane = getClipPlane(i);

                if (plane.x == 2.0) {
                    // Start of new union set — early out if already fully clipped
                    if (numSetsClippedBy + int(clippedByCurrentPlaneSet) == numPlaneSets)
                        break;

                    numPlaneSets = 1;
                    numSetsClippedBy = 0;
                    clippedByCurrentPlaneSet = false;
                } else if (plane.xyz == vec3(0.0)) {
                    // Start of new intersection set within current union
                    numPlaneSets = numPlaneSets + 1;
                    numSetsClippedBy += int(clippedByCurrentPlaneSet);
                    clippedByCurrentPlaneSet = false;
                } else if (!clippedByCurrentPlaneSet && calcClipPlaneDist(v_eyeSpace, plane) < 0.0) {
                    clippedByCurrentPlaneSet = true;
                }

                // Intersection colorization: highlight fragments near clip plane edges
                if (i <= u_clipParams[1] - 2 && !clippedByCurrentPlaneSet) {
                    vec3 pointOnPlane = v_eyeSpace - (abs(calcClipPlaneDist(v_eyeSpace, plane)) * plane.xyz);
                    if (distance(v_eyeSpace, pointOnPlane) <=
                        (kFrustumType_Perspective == u_frustum.z ? -pointOnPlane.z * widthFactor : widthFactor)) {
                        colorizeIntersection = true;
                    }
                }
            }

            // Pull this out of loop for multiple clip planes
            // Ported from: itwinjs-core Clipping.ts line 80
            if (colorizeIntersection && !clippedByCurrentPlaneSet) {
                g_clipColor = u_clipIntersection.rgb;
                return bvec2(true, true);
            }
        } else {
            for (int i = u_clipParams[0]; i < u_clipParams[1]; i++) {
                vec4 plane = getClipPlane(i);

                if (plane.x == 2.0) {
                    if (numSetsClippedBy + int(clippedByCurrentPlaneSet) == numPlaneSets)
                        break;

                    numPlaneSets = 1;
                    numSetsClippedBy = 0;
                    clippedByCurrentPlaneSet = false;
                } else if (plane.xyz == vec3(0.0)) {
                    numPlaneSets = numPlaneSets + 1;
                    numSetsClippedBy += int(clippedByCurrentPlaneSet);
                    clippedByCurrentPlaneSet = false;
                } else if (!clippedByCurrentPlaneSet && calcClipPlaneDist(v_eyeSpace, plane) < 0.0) {
                    clippedByCurrentPlaneSet = true;
                }
            }
        }

        // Finalize: count last intersection set
        numSetsClippedBy += int(clippedByCurrentPlaneSet);

        if (numSetsClippedBy == numPlaneSets) {
            // All intersection sets clipped → fragment is outside
            // Ported from: itwinjs-core Clipping.ts line 103
            if (u_outsideRgba.a > 0.0) {
                g_clipColor = u_outsideRgba.rgb;
                return bvec2(true, false);
            } else {
                discard;
            }
        } else if (u_insideRgba.a > 0.0) {
            // Fragment is inside — output inside color if configured
            // Ported from: itwinjs-core Clipping.ts line 109
            g_clipColor = u_insideRgba.rgb;
            return bvec2(true, false);
        }

        return bvec2(false, false);
    }
)glsl";

END_DQ_RENDER_NAMESPACE
