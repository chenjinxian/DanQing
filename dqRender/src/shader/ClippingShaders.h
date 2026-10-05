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
// Clipping functions（M-P P-D 接线版）
// ---------------------------------------------------------------------------
// 依赖（由 wire 侧/宿主 shader 提供）：
//   varying v_eyeSpace (vec3) —— Surface 的 createCommon() 已含（SurfaceCommon.h:154）；
//   uniform u_frustum (vec3) —— addFrustum 已含。
// 本文件不再内嵌 uniform 声明——uniforms 经 addClipping（ShaderBindings.cpp）
// 逐个注册（绑定面同步建立）；applyClipping 签名对齐 ShaderBuilder 的
// ApplyClipping 槽（"bvec2 applyClipping(vec4 baseColor)"——ShaderBuilder.cpp:548），
// g_clipColor/g_hasClipColor 为全局（参考 prelude.addline + addGlobal——
// ShaderBuilder.ts:1017 / Clipping.ts:163）。
//
// 拆分（M-P P-D）：kClippingHelpers（完整函数——addFunction）+
// kApplyClippingBody（槽体语句——setFragmentComponent 包裹签名）。
// Sentinel protocol（C++ 侧 ClipVolume 编码，见 ClipVolume.cpp）：
//   plane.x == 2.0          → start of a new union set
//   plane.xyz == vec3(0.0)  → start of a new intersection set within current union
// ---------------------------------------------------------------------------

static char const* kClippingHelpers = R"glsl(
    // Read clip plane from texture (row-major: plane at row = index).
    // Ported from: itwinjs-core Clipping.ts getClipPlaneFloat (:17-21)
    vec4 getClipPlane(int index) {
        return texelFetch(s_clipSampler, ivec2(0, index), 0);
    }

    // Signed distance from eye-space point to clip plane.
    // Ported from: itwinjs-core Clipping.ts calcClipPlaneDist (:37-41)
    float calcClipPlaneDist(vec3 camPos, vec4 plane) {
        return dot(vec4(camPos, 1.0), plane);
    }
)glsl";

// applyClipping 的槽体（纯语句——setFragmentComponent 以签名
// "bvec2 applyClipping(vec4 baseColor)" 包裹，ShaderBuilder.cpp:548）。
// Ported from: itwinjs-core Clipping.ts applyClipPlanesPrelude +
//   applyClipPlanesPostlude + applyClipPlanesLoopBody +
//   applyClipPlanesIntersectionLoopBody (:43-131)。
// A fragment is clipped when every intersection set in every union set clips it.
static char const* kApplyClippingBody = R"glsl(
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
)glsl";

inline constexpr int kClipVolumeTextureUnit = 9;

END_DQ_RENDER_NAMESPACE
