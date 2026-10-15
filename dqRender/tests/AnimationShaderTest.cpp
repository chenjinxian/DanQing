// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — glsl/Animation.ts 归位测试（W-a5）
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Animation.ts
//              + glsl/Surface.ts（:271-272 门/:448-454 getComputeAnimatedNormal/
//              :469-476 getComputeAnimatedTexCoord）+ glsl/Edge.ts（:274-277 门）
//              + TechniqueFlags.ts（:59/:93/:132/:145/:169/:186 isAnimated 链）。
//
// 参考无对应单元测试（shader 变体内容在参考侧由 WebGL 渲染测试覆盖）——本文件为
// Authored 源级锁（§5(f)）：变体键（isAnimated 维）+ shader 源码段出场 + 绑定注册。
// computeAnimParams 的钉值含 :178 运算符优先级怪癖的锁定。
#include <gtest/gtest.h>

#include "../src/render/AuxChannelTable.h"
#include "render/MultiVariantTechnique.h"
#include "render/ShaderBindings.h"
#include "render/ShaderProgramImpl.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/TechniqueImpl.h"

#include <string>

using namespace dqRender;

namespace {

// 构造 Surface 变体源码（GL-free——buildProgram 只设源码+注册绑定）。
void buildSurfaceSources(TechniqueFlags const& flags, std::string& vertOut, std::string& fragOut)
{
    SurfaceVariantCompiler compiler;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    vertOut = prog.getVertSource();
    fragOut = prog.getFragSource();
}

}  // namespace

// Ported from: TechniqueFlags.ts isAnimated 位全链（:59 字段/:93 init 置位/:132 reset/
// :145 setAnimated/:169 equals/:186 buildDescription）+ Technique.ts:415 的
// _kAnimated 变体维（DanQing MultiVariantTechnique::computeShaderIndex bit 6 同位面）。
TEST(AnimationShaderTest, TechniqueFlagsAnimatedBitInVariantKey)
{
    TechniqueFlags base;
    base.reset(FeatureMode::None, false, false, false, PositionType::Quantized);
    TechniqueFlags animated = base;
    animated.setAnimated(true);

    EXPECT_FALSE(base.isAnimated);
    EXPECT_TRUE(animated.isAnimated);
    EXPECT_FALSE(base.equals(animated));  // equals 区分（:169）

    // buildDescription/fromDescription 往返（:186/:210-261）。
    EXPECT_NE(animated.buildDescription().find("Animated"), std::string::npos);
    auto parsed = TechniqueFlags::fromDescription(animated.buildDescription());
    EXPECT_TRUE(parsed.isAnimated);

    // 变体键：isAnimated 入索引（参考 Technique.ts:415 `_kAnimated * flags.isAnimated`
    // 的 DanQing 同位面=MultiVariantTechnique 的 bit 6——经公共 getShader 双旗标取
    // 不同变体槽锁（computeShaderIndex 为 private——公共面等价探测）。
    MultiVariantTechnique technique(std::make_unique<SurfaceVariantCompiler>());
    ShaderProgram* progPlain = technique.getShader(base);
    ShaderProgram* progAnimated = technique.getShader(animated);
    ASSERT_TRUE(progPlain != nullptr);
    ASSERT_TRUE(progAnimated != nullptr);
    EXPECT_TRUE(progPlain != progAnimated) << "isAnimated must select a distinct variant slot";
}

// Ported from: Animation.ts addAnimation（:187-305 全量——isSurface=true 半含
// normals/params 三段）+ Surface.ts:271-272 `if (animated) addAnimation(vert, true)` 门。
// 源级锁：animated 变体出 位移+法线+参数 三段；非 animated 变体全不出场。
TEST(AnimationShaderTest, SurfaceAnimatedVariantHasAllThreeStages)
{
    TechniqueFlags flags;
    flags.reset(FeatureMode::None, false, false, false, PositionType::Quantized);
    flags.setAnimated(true);

    std::string vert, frag;
    buildSurfaceSources(flags, vert, frag);

    // 位移段（Surface/Edge 共享——:218-254）：u_animLUT/u_animLUTParams/
    // u_animDispParams/u_qAnimDispScale/u_qAnimDispOrigin + computeAnimationDisplacement
    // + AdjustRawPosition 槽体。
    EXPECT_NE(vert.find("u_animLUT"), std::string::npos);
    EXPECT_NE(vert.find("u_animLUTParams"), std::string::npos);
    EXPECT_NE(vert.find("u_animDispParams"), std::string::npos);
    EXPECT_NE(vert.find("u_qAnimDispScale"), std::string::npos);
    EXPECT_NE(vert.find("u_qAnimDispOrigin"), std::string::npos);
    EXPECT_NE(vert.find("computeAnimationDisplacement"), std::string::npos);
    EXPECT_NE(vert.find("computeAnimLUTCoords"), std::string::npos);

    // 法线段（isSurface 半 :256-274）：octDecodeNormal + computeAnimationNormal +
    // u_animNormalParams + v_n 的 getComputeAnimatedNormal 体（:448-454 的门）。
    EXPECT_NE(vert.find("computeAnimationNormal"), std::string::npos);
    EXPECT_NE(vert.find("u_animNormalParams"), std::string::npos);
    EXPECT_NE(vert.find("if (u_animNormalParams.x >= 0.0)"), std::string::npos);

    // 参数段（isSurface 半 :276-303）：computeAnimationParam + u_animScalarParams +
    // u_animScalarQParams + v_texCoord 的 getComputeAnimatedTexCoord 体（:469-476 的门）。
    EXPECT_NE(vert.find("computeAnimationParam"), std::string::npos);
    EXPECT_NE(vert.find("u_animScalarParams"), std::string::npos);
    EXPECT_NE(vert.find("u_animScalarQParams"), std::string::npos);
    EXPECT_NE(vert.find("if (u_animScalarQParams.x >= 0.0)"), std::string::npos);

    // 非 animated 变体：三段全不出场（Surface.ts:271 门的否臂）。
    TechniqueFlags plain;
    plain.reset(FeatureMode::None, false, false, false, PositionType::Quantized);
    std::string vertPlain, fragPlain;
    buildSurfaceSources(plain, vertPlain, fragPlain);
    EXPECT_EQ(vertPlain.find("computeAnimationDisplacement"), std::string::npos);
    EXPECT_EQ(vertPlain.find("u_animNormalParams"), std::string::npos);
    EXPECT_EQ(vertPlain.find("u_animScalarQParams"), std::string::npos);
}

// Ported from: Edge.ts:274-277 `if (isAnimated) { addAnimation(vert, false); ... }` 门——
// **TD-28④ 清偿**：DanQing 存量恒调（且在 isIndexed 分支内——参考门在分支外）。
// 双向锁：indexed 边无 animated → 位移段不出场；非 indexed 边有 animated → 出场。
TEST(AnimationShaderTest, EdgeAnimatedGateRespected)
{
    TechniqueFlags flags;
    flags.reset(FeatureMode::None, false, false, false, PositionType::Quantized);
    flags.setAnimated(true);

    // animated + SegmentEdge（非 indexed）→ 位移段出场（参考门在 isIndexed 分支外）。
    {
        EdgeVariantCompiler compiler(EdgeBuilderType::SegmentEdge);
        ShaderProgram prog;
        compiler.buildProgram(prog, flags);
        EXPECT_NE(prog.getVertSource().find("computeAnimationDisplacement"), std::string::npos)
            << "animated SegmentEdge must carry the displacement stage";
        EXPECT_NE(prog.getVertSource().find("g_otherPos.xyz += computeAnimationDisplacement"),
                  std::string::npos)
            << "animateEndPoint initializer（Edge.ts:38）";
    }

    // 非 animated + IndexedEdge → 位移段不出场（TD-28④ 原缺陷形态：恒调）。
    {
        TechniqueFlags plain;
        plain.reset(FeatureMode::None, false, false, false, PositionType::Quantized);
        EdgeVariantCompiler compiler(EdgeBuilderType::IndexedEdge);
        ShaderProgram prog;
        compiler.buildProgram(prog, plain);
        EXPECT_EQ(prog.getVertSource().find("computeAnimationDisplacement"), std::string::npos)
            << "non-animated IndexedEdge must NOT carry the displacement stage";
        EXPECT_EQ(prog.getVertSource().find("u_animDispParams"), std::string::npos);
    }
}

// Ported from: Animation.ts computeAnimParams（:171-184——inputValue=fraction×末 input
// 线性插值索引对+fraction；**:178 `inputValue - inputs[i] / (inputs[i+1] - inputs[i])`
// 运算符优先级怪癖原样**——除法先算（参考疑似 bug 照抄锁定）。
// 绑定 lambda 的位移/法线/参数三出口共用本函数（detail 面供测试）。
TEST(AnimationShaderTest, ComputeAnimParamsLocksReferenceSemantics)
{
    // 三帧 inputs=[0,1,2]、indices=[0,2,4]（2B 槽位——打包通道形态，dqRender::AuxChannel）。
    AuxChannel channel("s", {0.0f, 1.0f, 2.0f}, {0, 2, 4});
    float params[3] = {-1.0f, -1.0f, -1.0f};

    // fraction=0.25 → inputValue=0.5 ∈ [0,1) → params={0,2, 0.5 - 0/(1-0)}={0,2,0.5}。
    dqRender::detail::computeAnimParams(params, channel, 0.25f);
    EXPECT_FLOAT_EQ(params[0], 0.0f);
    EXPECT_FLOAT_EQ(params[1], 2.0f);
    EXPECT_FLOAT_EQ(params[2], 0.5f);

    // fraction=0.75 → inputValue=1.5 ∈ [1,2) → params={2,4, 1.5 - 1/(2-1)=0.5}。
    dqRender::detail::computeAnimParams(params, channel, 0.75f);
    EXPECT_FLOAT_EQ(params[0], 2.0f);
    EXPECT_FLOAT_EQ(params[1], 4.0f);
    EXPECT_FLOAT_EQ(params[2], 0.5f);

    // fraction=1.0 → inputValue=2.0 越界 → 尾帧对 + 0（:182-183）。
    dqRender::detail::computeAnimParams(params, channel, 1.0f);
    EXPECT_FLOAT_EQ(params[0], 4.0f);
    EXPECT_FLOAT_EQ(params[1], 4.0f);
    EXPECT_FLOAT_EQ(params[2], 0.0f);
}
