// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display shader assembly tests
//
// Authored: no reference test exists in itwinjs-core for thematic shader
//           assembly（行为锚定 glsl/Thematic.ts:214-336 的注册面 +
//           Surface.ts:573/599-601/821——M-S S-d）。
#include "render/ThematicDisplayShaders.h"

#include "NullDriver.h"
#include "render/RenderSystemImpl.h"
#include "render/TargetImpl.h"
#include "render/TechniqueImpl.h"
#include "render/SurfaceCommon.h"
#include "render/SurfaceFlags.h"
#include "render/SurfaceNormal.h"
#include "render/SurfaceTexture.h"
#include "render/SurfaceVariantCompiler.h"

#include <dqGeom/Range3d.h>

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

namespace {

// NullDriver 直接持 TargetImpl 栈（S-c ThematicUniformsTest 同型——
// ctor 链 GL-free）。
struct ThematicShaderFixture {
    std::unique_ptr<RenderSystemImpl> system;
    Techniques techniques;
    std::unique_ptr<TargetImpl> target;

    ThematicShaderFixture()
    {
        system = std::make_unique<RenderSystemImpl>(std::make_unique<rhi::NullDriver>());
        target = std::make_unique<TargetImpl>(*system, techniques, ViewRect(0, 0, 100, 100));
    }
};

void PushThematic(TargetImpl& t, ThematicDisplay const* td)
{
    ViewFlagsProperties vf;
    vf.thematicDisplay = true;
    t.changeRenderPlan(vf, true, nullptr, td);
    t.getUniforms().thematic.update(t);
}

} // namespace

// 注册面：全部 uniforms 以 GraphicUniform 注册（u_discardBetweenIsolines 为
// ProgramUniform——glsl/Thematic.ts:320-327）；s_texture 采样器声明在
//（绑定位=绘制环，EQUIVALENCE 登记在 SceneCompositorImpl）。
TEST(ThematicDisplayShaderTest, RegistersAllUniformsWithBindings)
{
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    createCommon(pb, false, false);
    addColor(pb);
    addSurfaceFlags(pb, false, false);
    addNormal(pb);
    addTexture(pb);
    addThematicDisplay(pb);

    ShaderProgram prog;
    pb.getVertexBuilder().addBindings(prog);
    pb.getFragmentBuilder().addBindings(prog);

    EXPECT_TRUE(prog.hasGraphicUniform("u_modelToWorld"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_thematicRange"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_thematicAxis"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_thematicSunDirection"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_thematicDisplayMode"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_marginColor"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_thematicSettings"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_thematicColorMix"));
    EXPECT_TRUE(prog.hasGraphicUniform("u_numSensors"));
    EXPECT_TRUE(prog.hasGraphicUniform("s_sensorSampler"));
    EXPECT_TRUE(prog.hasProgramUniform("u_discardBetweenIsolines"));
}

// 顶点索引用解码后模型位 rawPosition（G12——a_position 对 LUT 几何是
// 24-bit 索引），实例化臂前乘 g_modelMatrixRTC（Thematic.ts:173）。
TEST(ThematicDisplayShaderTest, ThematicIndexUsesRawPosition)
{
    auto build = [](bool instanced) {
        ProgramBuilder pb;
        pb.getVertexBuilder().setVersion("410 core");
        pb.getFragmentBuilder().setVersion("410 core");
        pb.enableFunctionCallVertexMain();
        pb.enableFunctionCallFragmentMain();
        // 全表面链（createCommon 落 ComputePosition 槽——无组件槽则 main 不
        // 发射、computed-varying 不出[buildSourceWithComponents 的
        // hasVertexComponents 门]；computeSurfaceNormal 由 addNormal 供）。
        createCommon(pb, instanced, false);
        addNormal(pb, /*quantized*/false, instanced);
        addTexture(pb);
        pb.getVertexBuilder().setUsesInstancedGeometry(instanced);
        addThematicDisplay(pb);
        return pb.getVertexBuilder().buildSourceWithComponents();
    };

    std::string const plain = build(false);
    EXPECT_NE(plain.find("u_modelToWorld * rawPosition"), std::string::npos);
    EXPECT_EQ(plain.find("g_modelMatrixRTC * rawPosition"), std::string::npos);
    // v_thematicIndex varying 声明 + findFractionalPositionOnLine 在源。
    EXPECT_NE(plain.find("v_thematicIndex"), std::string::npos);
    EXPECT_NE(plain.find("findFractionalPositionOnLine"), std::string::npos);

    std::string const inst = build(true);
    EXPECT_NE(inst.find("g_modelMatrixRTC * rawPosition"), std::string::npos);
}

// 绑定喂值：target uniforms.themetic 的 update 态经 GraphicUniform 落
// UniformHandle（Slope 弧度 range / 归一化轴 / fragSettings 四槽）。
TEST(ThematicDisplayShaderTest, BindingsFeedThematicUniformValues)
{
    ThematicShaderFixture f;

    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();
    addThematicDisplay(pb);

    ShaderProgram prog;
    pb.getVertexBuilder().addBindings(prog);
    pb.getFragmentBuilder().addBindings(prog);

    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Slope;
    td.range = dqGeom::Range1d(0.0, 90.0);
    td.axis = dqGeom::Vector3d::From(0, 0, 2);
    td.gradientSettings.mode = ThematicGradientMode::Stepped;
    td.gradientSettings.stepCount = 6;
    PushThematic(*f.target, &td);

    DrawParams dp;
    dp.setTarget(f.target.get());

    UniformHandle h;
    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_thematicRange", h, dp));
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], static_cast<float>(dqGeom::Angle::kPi / 2.0));

    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_thematicAxis", h, dp));
    EXPECT_NEAR(h.getData()[2], 1.0f, 1e-6f);  // normalize

    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_thematicSettings", h, dp));
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);  // Stepped
    EXPECT_FLOAT_EQ(h.getData()[2], 6.0f);  // stepCount

    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_thematicDisplayMode", h, dp));
    EXPECT_FLOAT_EQ(h.getData()[0], 2.0f);  // Slope

    // 非 IDW：u_numSensors=0（参考 :293-303 的否臂）。
    ASSERT_TRUE(prog.invokeGraphicUniformForTest("u_numSensors", h, dp));
    EXPECT_EQ(h.getIntData()[0], 0);
}

// u_discardBetweenIsolines ProgramUniform 随 isReadPixelsInProgress
//（glsl/Thematic.ts:320-327）。
TEST(ThematicDisplayShaderTest, DiscardBetweenIsolinesFollowsReadPixels)
{
    ThematicShaderFixture f;

    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    addThematicDisplay(pb);

    ShaderProgram prog;
    pb.getVertexBuilder().addBindings(prog);
    pb.getFragmentBuilder().addBindings(prog);

    UniformHandle h;
    // 非拾取 → 0。ProgramUniform 的 invoke 测试面用 ShaderProgramParams。
    ShaderProgramParams pp;
    pp.setTarget(f.target.get());
    ASSERT_TRUE(prog.invokeProgramUniformForTest("u_discardBetweenIsolines", h, pp));
    EXPECT_EQ(h.getIntData()[0], 0);
}

// TEMP-DIAG（M-S S-d height saga）：量化 LUT 变体顶点 main 的 thematic 段
// 结构取证——真实变体经 SurfaceVariantCompiler::buildProgram 直建（GL-free）。
TEST(ThematicDisplayShaderTest, QuantizedVariantVertexMainProbe)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Quantized;  // LUT（imdl 瓦形态）
    flags.isThematic = true;
    flags.featureMode = FeatureMode::None;

    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    fprintf(stderr, "=== QUANTIZED THEMATIC VERTEX ===\n%s\n=== END ===\n",
            vert.c_str());
    EXPECT_NE(vert.find("v_thematicIndex"), std::string::npos);
}
