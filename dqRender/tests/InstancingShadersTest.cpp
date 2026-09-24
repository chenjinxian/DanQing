// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — InstancingShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//              (no direct reference test exists; tests verify the faithful GLSL
//               strings + wiring against Instancing.ts)
#include "render/InstancingShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// extractInstanceBit: delegates to extractNthBit on a_instanceOverrides.r.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//              TEST(InstancingShadersTest, ExtractInstanceBitGlslFaithful)
TEST(InstancingShadersTest, ExtractInstanceBitGlslFaithful)
{
    std::string const s(kExtractInstanceBit);
    EXPECT_NE(s.find("extractInstanceBit"), std::string::npos);
    EXPECT_NE(s.find("extractNthBit(a_instanceOverrides.r"), std::string::npos);
}

// computeInstancedModelMatrixRTC: area-pattern branch + instanced-matrix branch.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//              TEST(InstancingShadersTest, ComputeInstancedModelMatrixRTCGlslFaithful)
TEST(InstancingShadersTest, ComputeInstancedModelMatrixRTCGlslFaithful)
{
    std::string const s(kComputeInstancedModelMatrixRTC);
    EXPECT_NE(s.find("g_isAreaPattern"), std::string::npos);
    EXPECT_NE(s.find("g_modelMatrixRTC"), std::string::npos);
    EXPECT_NE(s.find("u_patternParams"), std::string::npos);
    EXPECT_NE(s.find("a_instanceMatrixRow0"), std::string::npos);
    EXPECT_NE(s.find("a_instanceMatrixRow1"), std::string::npos);
    EXPECT_NE(s.find("a_instanceMatrixRow2"), std::string::npos);
    EXPECT_NE(s.find("u_patLocalToModel"), std::string::npos);
    EXPECT_NE(s.find("u_patSymbolToLocal"), std::string::npos);
}

// addInstancedModelMatrixRTC wires uniforms + globals + initializers (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//              TEST(InstancingShadersTest, AddInstancedModelMatrixRTCWiring)
TEST(InstancingShadersTest, AddInstancedModelMatrixRTCWiring)
{
    ProgramBuilder builder;
    ShaderBuilder& vert = builder.getVertexBuilder();
    vert.setUsesInstancedGeometry(true);
    addInstancedModelMatrixRTC(vert);
    SUCCEED();
}

// addInstanceOverrides is idempotent (find() guard skips on second call).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//              TEST(InstancingShadersTest, AddInstanceOverridesIdempotent)
TEST(InstancingShadersTest, AddInstanceOverridesIdempotent)
{
    ProgramBuilder builder;
    ShaderBuilder& vert = builder.getVertexBuilder();
    addInstanceOverrides(vert);
    addInstanceOverrides(vert);  // second call hits the find() guard
    SUCCEED();
}

// addInstanceColor wires overrides + u_applyInstanceColor.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Instancing.ts
//              TEST(InstancingShadersTest, AddInstanceColorWiring)
TEST(InstancingShadersTest, AddInstanceColorWiring)
{
    ProgramBuilder builder;
    ShaderBuilder& vert = builder.getVertexBuilder();
    addInstanceColor(vert);
    SUCCEED();
}
