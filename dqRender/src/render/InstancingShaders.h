// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instancing shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//
// Vertex-side instancing: the per-instance model matrix (RTC) computation,
// instance override extraction, and instance color application.
// Depends on Common.addExtractNthBit + FeatureSymbology.addOvrFlagConstants.
#pragma once

#include "CommonShaders.h"            // addExtractNthBit
#include "FeatureSymbologyShaders.h"  // addOvrFlagConstants
#include "ShaderBuilder.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Reference GLSL fragments (exact ports from Instancing.ts)
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Instancing.ts extractInstanceBit
inline constexpr char const* kExtractInstanceBit = R"(
float extractInstanceBit(uint flag) { return extractNthBit(a_instanceOverrides.r, flag); }
)";

// Ported from: itwinjs-core Instancing.ts computeInstancedModelMatrixRTC
inline constexpr char const* kComputeInstancedModelMatrixRTC = R"(
  if (g_isAreaPattern) {
    vec2 spacing = u_patternParams.yz;
    float scale = u_patternParams.w;

    float x = u_patternOrigin.x + a_patternX * spacing.x;
    float y = u_patternOrigin.y + a_patternY * spacing.y;
    vec4 translation = vec4(x / scale, y / scale, 0.0, 1.0);
    mat4 symbolTrans = u_patOrg;
    symbolTrans[3] = symbolTrans * translation;

    g_modelMatrixRTC = u_patLocalToModel * symbolTrans * u_patSymbolToLocal;
  } else {
    g_modelMatrixRTC = mat4(
      a_instanceMatrixRow0.x, a_instanceMatrixRow1.x, a_instanceMatrixRow2.x, 0.0,
      a_instanceMatrixRow0.y, a_instanceMatrixRow1.y, a_instanceMatrixRow2.y, 0.0,
      a_instanceMatrixRow0.z, a_instanceMatrixRow1.z, a_instanceMatrixRow2.z, 0.0,
      a_instanceMatrixRow0.w, a_instanceMatrixRow1.w, a_instanceMatrixRow2.w, 1.0);
  }
)";

// ---------------------------------------------------------------------------
// Wiring (exact ports of Instancing.ts addXxx)
//
// Uniform VALUE bindings (u_patternParams, u_patOrg, u_patLocalToModel,
// u_patSymbolToLocal, u_patternOrigin, u_applyInstanceColor) are registered
// with binding=nullptr per convention.
// ---------------------------------------------------------------------------

/// add the area-pattern transform uniforms.
/// Ported from: itwinjs-core Instancing.ts addPatternTransforms()
inline void addPatternTransforms(ShaderBuilder& vert)
{
    vert.addUniform("u_patOrg", VariableType::Mat4, nullptr);
    vert.addUniform("u_patLocalToModel", VariableType::Mat4, nullptr);
    vert.addUniform("u_patSymbolToLocal", VariableType::Mat4, nullptr);
    vert.addUniform("u_patternOrigin", VariableType::Vec2, nullptr);
}

/// Wire the instanced model matrix (RTC) computation into the vertex shader.
/// Ported from: itwinjs-core Instancing.ts addInstancedModelMatrixRTC()
/// (reference asserts vert.usesInstancedGeometry; dev-only invariant omitted
///  in this no-assert build -- the caller sets it via setUsesInstancedGeometry.)
inline void addInstancedModelMatrixRTC(ShaderBuilder& vert)
{
    vert.addUniform("u_patternParams", VariableType::Vec4, nullptr);
    addPatternTransforms(vert);

    vert.addGlobal("g_isAreaPattern", VariableType::Boolean);
    vert.addInitializer("g_isAreaPattern = 0.0 != u_patternParams.x;");

    vert.addGlobal("g_modelMatrixRTC", VariableType::Mat4);
    vert.addInitializer(std::string(kComputeInstancedModelMatrixRTC));
}

/// Wire instance override extraction (idempotent via find()).
/// Ported from: itwinjs-core Instancing.ts addInstanceOverrides()
inline void addInstanceOverrides(ShaderBuilder& vert)
{
    if (nullptr != vert.find("a_instanceOverrides"))
        return;

    addOvrFlagConstants(vert);
    addExtractNthBit(vert);
    vert.addFunction(std::string(kExtractInstanceBit));
}

/// Wire instance color application.
/// Ported from: itwinjs-core Instancing.ts addInstanceColor()
inline void addInstanceColor(ShaderBuilder& vert)
{
    addInstanceOverrides(vert);
    vert.addUniform("u_applyInstanceColor", VariableType::Float, nullptr);
}

END_DQ_RENDER_NAMESPACE
