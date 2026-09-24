// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Solar shadow assembly helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/SolarShadowMapping.ts
//
// Wires EVSM shadow mapping into the ShaderBuilder component system.
// Refactors standalone GLSL snippets from shader/SolarShadowShaders.h into
// composable function bodies compatible with buildFragmentMain().
#pragma once

#include "CommonShaders.h"   // addFrustum (for u_frustum)
#include "ShaderBuilder.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// EVSM shadow GLSL functions (composable — no uniform/in/out declarations)
// Ported from: itwinjs-core SolarShadowMapping.ts
// ---------------------------------------------------------------------------

inline constexpr char const* kEVSMFunctions = R"(
const float kVSMBias = 0.1;

vec2 warpDepth(float depth, float exponent) {
    depth = 2.0 * depth - 1.0;
    float pos =  exp( exponent * depth);
    float neg = -exp(-exponent * depth);
    return vec2(pos, neg);
}

float chebyshevUpperBound(vec2 moments, float mean, float minVariance) {
    float variance = moments.y - (moments.x * moments.x);
    variance = max(variance, minVariance);
    float d = mean - moments.x;
    float pMax = variance / (variance + (d * d));
    return (mean <= moments.x ? 1.0 : pMax);
}

float shadowMapEVSM(vec3 shadowPos) {
    vec2 warpedDepth = warpDepth(shadowPos.z, u_evsmExponent);
    vec4 occluder = TEXTURE(s_shadowSampler, shadowPos.xy);
    vec2 depthScale = kVSMBias * 0.01 * u_evsmExponent * warpedDepth;
    vec2 minVariance = depthScale * depthScale;
    float posContrib = chebyshevUpperBound(occluder.xz, warpedDepth.x, minVariance.x);
    float negContrib = chebyshevUpperBound(occluder.yw, warpedDepth.y, minVariance.y);
    return min(posContrib, negContrib);
}
)";

// ---------------------------------------------------------------------------
// addSolarShadowMap — wire EVSM shadow mapping into a ProgramBuilder
// Ported from: itwinjs-core SolarShadowMapping.ts addSolarShadowMapping() (line 93-140)
// ---------------------------------------------------------------------------
inline void addSolarShadowMap(ProgramBuilder& builder)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // Uniforms
    // Ported from: itwinjs-core SolarShadowMapping.ts (line 96-101)
    frag.addUniform("s_shadowSampler", VariableType::Sampler2D, nullptr);
    frag.addUniform("u_shadowParams", VariableType::Vec4, nullptr);   // rgb=shadow color, a=bias
    frag.addUniform("u_evsmExponent", VariableType::Float, nullptr);  // 42.0 for 32-bit, 5.545 for 16-bit
    frag.addUniform("u_sunDir", VariableType::Vec3, nullptr);

    // v_shadowPos computed varying
    // Ported from: itwinjs-core SolarShadowMapping.ts (line 104-108)
    builder.addVarying("v_shadowPos", VariableType::Vec4);
    vert.addUniform("u_shadowProj", VariableType::Mat4, nullptr);

    // Compute shadow position using a_position directly (avoids rawPosition scope issue).
    vert.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addInitializer(
        "v_shadowPos = u_shadowProj * vec4(a_position, 1.0);\n"
        "v_shadowPos.xyz /= v_shadowPos.w;\n"
        "v_shadowPos.z = 1.0 - v_shadowPos.z;");

    // EVSM functions
    frag.addFunction(std::string(kEVSMFunctions));

    // applySolarShadowMap function (defined outside main)
    // buildFragmentMain() appends: baseColor = applySolarShadowMap(baseColor);
    // Ported from: itwinjs-core SolarShadowMapping.ts applySolarShadowMap (line 64-73)
    frag.addFunction(R"(
vec4 applySolarShadowMap(vec4 bc) {
    if (v_shadowPos.x < 0.0 || v_shadowPos.x > 1.0 ||
        v_shadowPos.y < 0.0 || v_shadowPos.y > 1.0 ||
        v_shadowPos.z < 0.0 || v_shadowPos.z > 1.0)
        return bc;
    float visible = shadowMapEVSM(v_shadowPos.xyz);
    return vec4(bc.rgb * mix(u_shadowParams.rgb, vec3(1.0), visible), bc.a);
}
)");
    frag.setFragmentComponent(FragmentShaderComponent::ApplySolarShadowMap, "");
}

END_DQ_RENDER_NAMESPACE
