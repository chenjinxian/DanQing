// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Atmospheric scattering assembly helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Atmosphere.ts
//
// Wires Rayleigh atmospheric scattering into the ShaderBuilder component system.
// Uses GLSL snippets from shader/AtmosphereShaders.h.
#pragma once

#include "ShaderBuilder.h"
#include "shader/AtmosphereShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addAtmosphericScatteringEffect — wire atmosphere into a ProgramBuilder
// Ported from: itwinjs-core Atmosphere.ts addAtmosphericScatteringEffect() (line 468-524)
// ---------------------------------------------------------------------------
inline void addAtmosphericScatteringEffect(ProgramBuilder& builder, bool perVertex = true)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // Constants
    frag.addFunction(std::string(kAtmosphereConstants));
    vert.addFunction(std::string(kAtmosphereConstants));

    // Uniforms — Ported from: itwinjs-core Atmosphere.ts addMainShaderUniforms()
    auto addAtmUniform = [&](std::string const& name, VariableType type) {
        vert.addUniform(name, type, nullptr);
        frag.addUniform(name, type, nullptr);
    };
    addAtmUniform("u_atmosphereData", VariableType::Mat4);
    addAtmUniform("u_sunDir", VariableType::Vec3);
    addAtmUniform("u_atmosphereScaleMatrix", VariableType::Mat4);
    addAtmUniform("u_inverseAtmosphereScaleInverseRotationMatrix", VariableType::Mat4);
    addAtmUniform("u_inverseEarthScaleInverseRotationMatrix", VariableType::Mat4);
    addAtmUniform("u_earthScaleMatrix", VariableType::Mat4);

    // Both stages need u_exposure (used by applyHdr)
    addAtmUniform("u_exposure", VariableType::Float);

    // Helper functions — both stages need them for per-vertex compute
    auto addAtmFunctions = [&](ShaderBuilder& shader) {
        shader.addFunction(std::string(kAtmosphereComputeRayDir));
        shader.addFunction(std::string(kAtmosphereComputeSceneDepthDefault));
        shader.addFunction(std::string(kAtmosphereComputeRayOrigin));
        shader.addFunction(std::string(kAtmosphereRaySphere));
        shader.addFunction(std::string(kAtmosphereRayEllipsoidIntersection));
        shader.addFunction(std::string(kAtmosphereDensityAtPoint));
        shader.addFunction(std::string(kAtmosphereOpticalDepth));
        shader.addFunction(std::string(kAtmosphereCalculateReflectedLightIntensity));
        shader.addFunction(std::string(kAtmosphereComputeScattering));
        shader.addFunction(std::string(kAtmosphereApplyHdr));
    };

    if (perVertex) {
        addAtmFunctions(vert);
        addAtmFunctions(frag);

        // Varyings
        builder.addVarying("v_atmosphericScatteringColor", VariableType::Vec3);
        builder.addVarying("v_reflectedLightIntensity", VariableType::Float);

        // Vertex slot: compute atmospheric scattering varyings
        vert.setVertexComponent(VertexShaderComponent::ComputeAtmosphericScatteringVaryings,
            "    mat3 scatterResult = computeAtmosphericScattering(false);\n"
            "    v_atmosphericScatteringColor = applyHdr(scatterResult[0]);\n"
            "    v_reflectedLightIntensity = scatterResult[1].x;\n");
    }

    // Fragment slot: apply atmospheric scattering to baseColor
    // buildFragmentMain() appends: baseColor = applyAtmosphericScattering(baseColor);
    frag.addFunction(
        "vec4 applyAtmosphericScattering(vec4 bc) {\n"
        "    return vec4(bc.rgb * v_reflectedLightIntensity + v_atmosphericScatteringColor, bc.a);\n"
        "}\n");
    frag.setFragmentComponent(FragmentShaderComponent::ApplyAtmosphericScattering, "");
}

END_DQ_RENDER_NAMESPACE
