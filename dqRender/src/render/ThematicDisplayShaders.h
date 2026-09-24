// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display assembly helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Thematic.ts
//
// Wires thematic display (height/slope/hillshade/sensor) into ShaderBuilder.
// Uses GLSL snippets from shader/ThematicShaders.h.
#pragma once

#include "ShaderBuilder.h"
#include "shader/ThematicShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addThematicDisplay — wire thematic display into a ProgramBuilder
// Ported from: itwinjs-core Thematic.ts addThematicDisplay() (line 214-336)
// ---------------------------------------------------------------------------
inline void addThematicDisplay(ProgramBuilder& builder)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // Display mode + gradient mode constants
    frag.addFunction(std::string(kThematicDisplayModeConstants));
    vert.addFunction(std::string(kThematicDisplayModeConstants));
    frag.addFunction(std::string(kThematicGradientModeConstants));

    // Uniforms — Ported from: itwinjs-core Thematic.ts (line 220-227)
    vert.addUniform("u_modelToWorld", VariableType::Mat4, nullptr);
    builder.addUniform("u_thematicRange", VariableType::Vec2, nullptr);
    builder.addUniform("u_thematicAxis", VariableType::Vec3, nullptr);
    builder.addUniform("u_thematicSunDirection", VariableType::Vec3, nullptr);
    builder.addUniform("u_thematicDisplayMode", VariableType::Float, nullptr);
    frag.addUniform("u_marginColor", VariableType::Vec4, nullptr);
    builder.addUniform("u_thematicSettings", VariableType::Vec4, nullptr);
    frag.addUniform("u_thematicColorMix", VariableType::Float, nullptr);
    frag.addUniform("u_numSensors", VariableType::Int, nullptr);
    frag.addUniform("s_sensorSampler", VariableType::Sampler2D, nullptr);
    frag.addUniform("u_discardBetweenIsolines", VariableType::Boolean, nullptr);

    // Gradient LUT texture (s_texture)
    frag.addUniform("s_texture", VariableType::Sampler2D, nullptr);

    // Vertex: findFractionalPositionOnLine + v_thematicIndex varying
    vert.addFunction(std::string(kFindFractionalPositionOnLine));
    builder.addVarying("v_thematicIndex", VariableType::Float);

    // Compute thematic index using a_position directly (avoids rawPosition scope issue).
    vert.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
    vert.addInitializer(
        "if (kThematicDisplayMode_Height == u_thematicDisplayMode) {\n"
        "    vec3 u = (u_modelToWorld * vec4(a_position, 1.0)).xyz;\n"
        "    vec3 v = u_thematicAxis;\n"
        "    vec3 proju = (dot(v, u) / dot(v, v)) * v;\n"
        "    vec3 a = v * u_thematicRange.x;\n"
        "    vec3 b = v * u_thematicRange.y;\n"
        "    vec3 c = proju;\n"
        "    v_thematicIndex = findFractionalPositionOnLine(a, b, c);\n"
        "} else if (kThematicDisplayMode_HillShade == u_thematicDisplayMode) {\n"
        "    v_thematicIndex = v_n.z;\n"
        "}\n");

    // Fragment helper functions
    frag.addFunction(std::string(kUniversalFwidth));
    frag.addFunction(std::string(kThematicGetColor));
    frag.addFunction(std::string(kThematicGetSensor));
    frag.addFunction(std::string(kThematicGetIsoLineColor));

    // Fragment slot: apply thematic display
    // buildFragmentMain() appends: baseColor = applyThematicDisplay(baseColor);
    // So we define the function outside main and leave the slot body empty.
    frag.addFunction(
        "vec4 applyThematicDisplay(vec4 baseColor) {\n"
        + std::string(kApplyThematicColorPrelude)
        + std::string(kApplyThematicColorPostlude)
        + "  return baseColor;\n"
        "}\n");
    frag.setFragmentComponent(FragmentShaderComponent::ApplyThematicDisplay, "");
}

END_DQ_RENDER_NAMESPACE
