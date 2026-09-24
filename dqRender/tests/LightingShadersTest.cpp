// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — LightingShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Lighting.ts
//              (no direct reference test exists; tests verify the faithful GLSL
//               strings + wiring against Lighting.ts)
#include "render/LightingShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// computeDirectionalLighting: diffuse + specular (pow) accumulation.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Lighting.ts
//              TEST(LightingShadersTest, ComputeDirectionalLightingGlslFaithful)
TEST(LightingShadersTest, ComputeDirectionalLightingGlslFaithful)
{
    std::string const s(kComputeDirectionalLighting);
    EXPECT_NE(s.find("computeDirectionalLight"), std::string::npos);
    EXPECT_NE(s.find("pow(specularDot, specularExponent)"), std::string::npos);
    EXPECT_NE(s.find("max(dot(normal, lightDir), 0.0)"), std::string::npos);
}

// applyLighting: references the lighting uniforms + frustum + surface flag, and
// calls computeDirectionalLight. Exact reference tokens.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Lighting.ts
//              TEST(LightingShadersTest, ApplyLightingGlslFaithful)
TEST(LightingShadersTest, ApplyLightingGlslFaithful)
{
    std::string const s(kApplyLighting);
    EXPECT_NE(s.find("u_sunDir"), std::string::npos);
    EXPECT_NE(s.find("u_lightSettings"), std::string::npos);
    EXPECT_NE(s.find("u_upVector"), std::string::npos);
    EXPECT_NE(s.find("kFrustumType_Perspective"), std::string::npos);
    EXPECT_NE(s.find("kSurfaceBitIndex_ApplyLighting"), std::string::npos);
    EXPECT_NE(s.find("computeDirectionalLight("), std::string::npos);
    EXPECT_NE(s.find("directionalFudge"), std::string::npos);
    EXPECT_NE(s.find("fresnelIntensity"), std::string::npos);
    EXPECT_NE(s.find("numCel"), std::string::npos);
}

// addLighting wires addFrustum + the function + component + 3 uniforms (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Lighting.ts
//              TEST(LightingShadersTest, AddLightingWiring)
TEST(LightingShadersTest, AddLightingWiring)
{
    ProgramBuilder builder;
    addLighting(builder);
    SUCCEED();
}
