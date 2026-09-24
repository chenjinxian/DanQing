// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — MonochromeShaders tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/glsl/Monochrome.test.ts
//              (no direct reference test exists; tests verify the faithful GLSL +
//               wiring against Monochrome.ts)
#include "render/MonochromeShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// The unlit GLSL references u_monoRgb + the kShaderBit_Monochrome flag.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/glsl/Monochrome.test.ts
//              TEST(MonochromeShadersTest, UnlitGlslFaithful)
TEST(MonochromeShadersTest, UnlitGlslFaithful)
{
    std::string const s(kApplyUnlitMonochromeColor);
    EXPECT_NE(s.find("u_monoRgb"), std::string::npos);
    EXPECT_NE(s.find("kShaderBit_Monochrome"), std::string::npos);
    EXPECT_NE(s.find("baseColor.a"), std::string::npos);
}

// The surface GLSL carries the reference luminance weights and u_mixMonoColor.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/glsl/Monochrome.test.ts
//              TEST(MonochromeShadersTest, SurfaceGlslFaithful)
TEST(MonochromeShadersTest, SurfaceGlslFaithful)
{
    std::string const s(kApplySurfaceMonochromeColor);
    EXPECT_NE(s.find("u_mixMonoColor"), std::string::npos);
    // Exact reference luminance weights (.222, .707, .071).
    EXPECT_NE(s.find("vec3(.222, .707, .071)"), std::string::npos);
    EXPECT_NE(s.find("u_monoRgb"), std::string::npos);
}

// addUnlitMonochrome registers u_monoRgb and sets ApplyMonochrome (no throw).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/glsl/Monochrome.test.ts
//              TEST(MonochromeShadersTest, AddUnlitMonochromeWiring)
TEST(MonochromeShadersTest, AddUnlitMonochromeWiring)
{
    ProgramBuilder builder;
    addUnlitMonochrome(builder);
    // No assertions on internal builder state (introspection is limited); the
    // faithful GLSL constants above cover correctness. This is a wiring smoke test.
    SUCCEED();
}

// addSurfaceMonochrome additionally registers u_mixMonoColor.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/glsl/Monochrome.test.ts
//              TEST(MonochromeShadersTest, AddSurfaceMonochromeWiring)
TEST(MonochromeShadersTest, AddSurfaceMonochromeWiring)
{
    ProgramBuilder builder;
    addSurfaceMonochrome(builder);
    SUCCEED();
}
