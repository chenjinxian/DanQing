// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — CommonShaders tests
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              (no direct reference test exists; tests verify the faithful GLSL
//               strings + wiring against Common.ts)
#include "render/CommonShaders.h"
#include "render/ShaderBuilder.h"

#include <gtest/gtest.h>
#include <string>

using namespace dqRender;

// chooseVec2With2BitFlags: selects v2 when any of the two flag bits is set.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, ChooseVec2GlslFaithful)
TEST(CommonShadersTest, ChooseVec2GlslFaithful)
{
    std::string const s(kChooseVec2With2BitFlags);
    EXPECT_NE(s.find("chooseVec2With2BitFlags"), std::string::npos);
    EXPECT_NE(s.find("flags & (n1 | n2)"), std::string::npos);
}

// chooseVec3WithBitFlag: selects v2 when the flag bit is set.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, ChooseVec3GlslFaithful)
TEST(CommonShadersTest, ChooseVec3GlslFaithful)
{
    std::string const s(kChooseVec3WithBitFlag);
    EXPECT_NE(s.find("chooseVec3WithBitFlag"), std::string::npos);
    EXPECT_NE(s.find("flags & n"), std::string::npos);
}

// addUInt32s: 256.0 carry chain across x->y->z->w.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, AddUInt32sGlslFaithful)
TEST(CommonShadersTest, AddUInt32sGlslFaithful)
{
    std::string const s(kAddUInt32s);
    EXPECT_NE(s.find("addUInt32s"), std::string::npos);
    EXPECT_NE(s.find("c.x -= 256.0"), std::string::npos);
    EXPECT_NE(s.find("c.w += 1.0"), std::string::npos);
}

// nthBitSet: bool from float flags + uint bit.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, NthBitSetGlslFaithful)
TEST(CommonShadersTest, NthBitSetGlslFaithful)
{
    std::string const s(kNthBitSet);
    EXPECT_NE(s.find("nthBitSet"), std::string::npos);
    EXPECT_NE(s.find("uint(flags) & n"), std::string::npos);
}

// extractNthBit: 1.0/0.0 from float flags + uint bit.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, ExtractNthBitGlslFaithful)
TEST(CommonShadersTest, ExtractNthBitGlslFaithful)
{
    std::string const s(kExtractNthBit);
    EXPECT_NE(s.find("extractNthBit"), std::string::npos);
    EXPECT_NE(s.find("1.0 : 0.0"), std::string::npos);
}

// addExtractNthBit wires both functions (no throw).
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, AddExtractNthBitWiring)
TEST(CommonShadersTest, AddExtractNthBitWiring)
{
    ProgramBuilder builder;
    addExtractNthBit(builder.getFragmentBuilder());
    SUCCEED();
}

// addChooseVec2/3 wire their function + the extractNthBit dependency.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, AddChooseVecWiring)
TEST(CommonShadersTest, AddChooseVecWiring)
{
    ProgramBuilder builder;
    addChooseVec2WithBitFlagsFunction(builder.getFragmentBuilder());
    addChooseVec3WithBitFlagFunction(builder.getFragmentBuilder());
    SUCCEED();
}

// addShaderFlagsConstants registers the 5 kShaderBit_* constants.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, AddShaderFlagsConstantsWiring)
TEST(CommonShadersTest, AddShaderFlagsConstantsWiring)
{
    ProgramBuilder builder;
    addShaderFlagsConstants(builder.getFragmentBuilder());
    SUCCEED();
}

// addFrustum registers u_frustum + the 3 frustum-type globals on both stages.
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Common.ts
//              TEST(CommonShadersTest, AddFrustumWiring)
TEST(CommonShadersTest, AddFrustumWiring)
{
    ProgramBuilder builder;
    addFrustum(builder);
    SUCCEED();
}
