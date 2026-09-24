// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Edge/Polyline shared shader helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Polyline.ts
//
// Shared GLSL functions used by both Edge and Polyline shaders:
// - adjustWidth (slope-based width adjustment for non-AA lines)
// - addLineCode (line code texture pipeline)
// - addAdjustWidth (u_aaSamples + adjustWidth function)
#pragma once

#include "CommonShaders.h"       // addFrustum
#include "RenderPassShaders.h"   // addRenderPass
#include "ShaderBuilder.h"
#include "VertexShaderModules.h" // addLineCodeUniform

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// adjustWidth GLSL function
// Ported from: itwinjs-core Polyline.ts adjustWidth (line 132-186)
//
// Slope-based width adjustment for non-AA lines, widths 1-4.
// For width 1, also populates v_lnInfo with pixel trimming data.
// ---------------------------------------------------------------------------
inline constexpr char const* kAdjustWidth = R"(
void adjustWidth(inout float width, vec2 d2, vec2 org) {
  if (u_aaSamples > 1) {
    if (width < 5.0)
      width += (5.0 - width) * 0.125;
    return;
  }

  // calculate slope based width adjustment for non-AA lines, widths 1 to 4
  vec2 d2A = abs(d2);
  const float s_myFltEpsilon = 0.0001;
  if (d2A.y > s_myFltEpsilon && width < 4.5) {
    float len = length(d2A);
    float tan = d2A.x / d2A.y;

    if (width < 1.5) { // width 1
      if (tan <= 1.0)
        width = d2A.y;
      else
        width = d2A.x;
      width *= 1.01;
      v_lnInfo.xy = org;
      v_lnInfo.w = 1.0; // set flag to do trimming
      if (d2A.x - d2A.y > s_myFltEpsilon) {
        v_lnInfo.z = d2.y / d2.x;
        v_lnInfo.w += 2.0; // add in x-major flag
      } else
        v_lnInfo.z = d2.x / d2.y;

    } else if (width < 2.5) { // width 2
      if (tan <= 0.5)
        width = 2.0 * d2A.y;
      else
        width = (d2A.y + 2.0 * d2A.x);

    } else if (width < 3.5) { // width 3
        if (tan <= 1.0)
            width = (3.0 * d2A.y + d2A.x);
        else
            width = (d2A.y + 3.0 * d2A.x);

    } else { // width 4
      if (tan <= 0.5)
        width = (4.0 * d2A.y + d2A.x);
      else if (tan <= 2.0)
        width = (3.0 * d2A.y + 3.0 * d2A.x);
      else
        width = (d2A.y + 4.0 * d2A.x);
    }
    width /= len;
  }
}
)";

// ---------------------------------------------------------------------------
// addAdjustWidth — wire u_aaSamples uniform + adjustWidth function
// Ported from: itwinjs-core Polyline.ts addAdjustWidth() (line 189-197)
// ---------------------------------------------------------------------------
inline void addAdjustWidth(ShaderBuilder& vert)
{
    vert.addUniform("u_aaSamples", VariableType::Int, nullptr);
    vert.addFunction(std::string(kAdjustWidth));
}

// ---------------------------------------------------------------------------
// computeLineCodeTextureCoords GLSL function
// Ported from: itwinjs-core Polyline.ts computeTextureCoord (line 53-83)
// ---------------------------------------------------------------------------
inline constexpr char const* kComputeLineCodeTextureCoords = R"(
vec2 computeLineCodeTextureCoords(vec2 windowDir, vec4 projPos, float adjust, float patternDist) {
  vec2 texc;
  float lineCode = computeLineCode();
  if (0.0 == lineCode) {
    texc = vec2(-1.0, -1.0);
  } else {
    const float imagesPerPixel = 1.0/32.0;
    const float textureCoordinateBase = 8192.0;

    float patternDistPixels;
    if (u_useCumDist > 0.5) {
      patternDistPixels = patternDist * u_pixelsPerWorld;
    } else {
      if (abs(windowDir.x) > abs(windowDir.y))
        patternDistPixels = projPos.x + adjust * windowDir.x;
      else
        patternDistPixels = projPos.y + adjust * windowDir.y;
    }
    texc.x = textureCoordinateBase + imagesPerPixel * patternDistPixels;

    float numRows = u_numLineCodes;
    float centerY = 0.5 / numRows;
    float stepY = 1.0 / numRows;
    texc.y = stepY * lineCode + centerY;
  }

  return texc;
}
)";

// ---------------------------------------------------------------------------
// addLineCode — full line code pipeline (vertex + fragment)
// Ported from: itwinjs-core Polyline.ts addLineCode() (line 212-231)
//
// Adds: u_lineCode, u_pixelsPerWorld, u_useCumDist, u_numLineCodes,
//       computeLineCodeTextureCoords, v_texc varying, v_lnInfo varying,
//       u_lineCodeTexture, applyLineCode, checkForDiscard.
// ---------------------------------------------------------------------------
inline void addLineCode(ProgramBuilder& builder, std::string const& lineCodeArgs)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // Vertex uniforms
    addLineCodeUniform(vert);
    vert.addUniform("u_pixelsPerWorld", VariableType::Float, nullptr);
    vert.addUniform("u_useCumDist", VariableType::Float, nullptr);
    vert.addUniform("u_numLineCodes", VariableType::Float, nullptr);

    // computeLineCodeTextureCoords function (vertex)
    vert.addFunction(std::string(kComputeLineCodeTextureCoords));

    // v_texc varying (function-computed)
    builder.addVarying("v_texc", VariableType::Vec2);
    vert.addInitializer(
        std::string("v_texc = computeLineCodeTextureCoords(") + lineCodeArgs + ");");

    // v_lnInfo varying
    builder.addVarying("v_lnInfo", VariableType::Vec4);

    // Fragment: line code texture + applyLineCode
    addFrustum(builder);
    frag.addUniform("u_lineCodeTexture", VariableType::Sampler2D, nullptr);
    frag.addGlobal("discardByLineCode", VariableType::Boolean, "false");

    // Define finalizeBaseColor as a function (outside main).
    // buildFragmentMain() will call: baseColor = finalizeBaseColor(baseColor);
    frag.addFunction(
        "vec4 finalizeBaseColor(vec4 bc) {\n"
        "  if (v_texc.x >= 0.0) {\n"
        "    vec4 texColor = TEXTURE(u_lineCodeTexture, v_texc);\n"
        "    if (0.0 == texColor.r) discardByLineCode = true;\n"
        "  }\n"
        "  if (v_lnInfo.w > 0.5) {\n"
        "    vec2 dxy = gl_FragCoord.xy - v_lnInfo.xy;\n"
        "    if (v_lnInfo.w < 1.5) dxy = dxy.yx;\n"
        "    float dist = v_lnInfo.z * dxy.x - dxy.y;\n"
        "    float distA = abs(dist);\n"
        "    if (distA > 0.5 || (distA == 0.5 && dist < 0.0)) discardByLineCode = true;\n"
        "  }\n"
        "  return bc;\n"
        "}\n");

    // Define checkForDiscard as a function (outside main).
    // buildFragmentMain() will call: if (checkForDiscard(baseColor)) { discard; return; }
    frag.addFunction(
        "bool checkForDiscard(vec4 bc) { return discardByLineCode; }\n");

    // Slot bodies are empty — the functions are already defined above.
    // buildFragmentMain() appends the function calls automatically.
    frag.setFragmentComponent(FragmentShaderComponent::FinalizeBaseColor, "");
    frag.setFragmentComponent(FragmentShaderComponent::CheckForDiscard, "");
}

END_DQ_RENDER_NAMESPACE
