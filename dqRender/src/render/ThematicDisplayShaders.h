// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Thematic display assembly helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Thematic.ts
//
// Wires thematic display (height/slope/hillshade/sensor) into ShaderBuilder.
// Uses GLSL snippets from shader/ThematicShaders.h.
//
// M-S S-d 归位：uniforms 全量真绑定（GraphicUniform 族 + u_discardBetween-
// Isolines ProgramUniform——glsl/Thematic.ts:214-336；原全 nullptr 注册=
// 编译过但值恒默认[G13]）+ v_thematicIndex 归位 addInlineComputedVarying
// 消费链（rawPosition 在域——原 addInitializer 形态的 a_position 对 LUT 几
// 何是 24-bit 索引[G12]）。
#pragma once

#include "ShaderBuilder.h"
#include "shader/ThematicShaders.h"

#include "DrawParams.h"         // DrawParams（GraphicUniform lambda 形参）
#include "ShaderProgramImpl.h"  // ShaderProgram::addGraphicUniform/addProgramUniform
#include "TargetImpl.h"         // 绑定 lambda 的 getUniforms().thematic 消费面

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addThematicDisplay — wire thematic display into a ProgramBuilder
// Ported from: itwinjs-core Thematic.ts addThematicDisplay() (line 214-336)
//（v_thematicIndex 的 inline-computed-varying 段自 Surface.ts:573
//  addTexture 的 isThematic 臂并入——同一逻辑移植单元）。
// ---------------------------------------------------------------------------
inline void addThematicDisplay(ProgramBuilder& builder)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    // Display mode + gradient mode constants（:199-211 的 addDefine 面——
    // DanQing 以 const float 函数串承载[既有形态]）。
    frag.addFunction(std::string(kThematicDisplayModeConstants));
    vert.addFunction(std::string(kThematicDisplayModeConstants));
    frag.addFunction(std::string(kThematicGradientModeConstants));

    // --- Uniforms（:230-327——全 GraphicUniform，唯 u_discardBetweenIsolines
    // 为 ProgramUniform）---

    // u_modelToWorld（vert，:232-237——branch.bindModelToWorldTransform
    // [BranchUniforms.ts:171-173]；DanQing=branch.bindModelMatrix[m_model
    // 承载 _m32 等价面——Uniforms.h:284]）。
    vert.addUniform("u_modelToWorld", VariableType::Mat4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_modelToWorld", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().branch.bindModelMatrix(u);
        });
    });

    // u_thematicRange（:239-243）
    builder.addUniform("u_thematicRange", VariableType::Vec2, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_thematicRange", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().thematic.bindRange(u);
        });
    });

    // u_thematicAxis（:245-249）
    builder.addUniform("u_thematicAxis", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_thematicAxis", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().thematic.bindAxis(u);
        });
    });

    // u_thematicSunDirection（:251-256——非点云臂）
    builder.addUniform("u_thematicSunDirection", VariableType::Vec3, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_thematicSunDirection", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().thematic.bindSunDirection(u);
        });
    });

    // u_thematicDisplayMode（:262-266）
    builder.addUniform("u_thematicDisplayMode", VariableType::Float, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_thematicDisplayMode", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().thematic.bindDisplayMode(u);
        });
    });

    // u_marginColor（:268-272）
    frag.addUniform("u_marginColor", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_marginColor", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().thematic.bindMarginColor(u);
        });
    });

    // u_thematicSettings（:274-279——gradientMode/distanceCutoff/stepCount/
    // multiplyAlpha 四槽）
    builder.addUniform("u_thematicSettings", VariableType::Vec4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_thematicSettings", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().thematic.bindFragSettings(u);
        });
    });

    // u_thematicColorMix（:281-291——非 terrain/pointcloud 恒 0[本函数为
    // surface 面]）。
    frag.addUniform("u_thematicColorMix", VariableType::Float, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_thematicColorMix", [](UniformHandle& u, DrawParams const&) {
            u.setUniform1f(0.0f);
        });
    });

    // u_numSensors（:293-303——全局/逐 batch 分流：wantGlobalSensorTexture→
    // thematic.bindNumSensors；否则 batch.bindNumThematicSensors[BatchUniforms.
    // _setCurrentBatch :74-82 的 m_sensors——drawPass 分流绑定面供给]）。
    frag.addUniform("u_numSensors", VariableType::Int, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_numSensors", [](UniformHandle& u, DrawParams const& dp) {
            auto* t = dp.getTarget();
            if (t && t->wantThematicSensors()) {
                if (t->getUniforms().thematic.wantGlobalSensorTexture())
                    t->getUniforms().thematic.bindNumSensors(u);
                else
                    t->getUniforms().batch.bindNumThematicSensors(u);
            } else {
                u.setUniform1i(0);
            }
        });
    });

    // s_sensorSampler（:305-318——GPU 纹理绑=drawPass 分流绑定面
    // [SceneCompositorImpl 的 flags.isThematic 块；DanQing 绑定架构=绘制环，
    //  EQUIVALENCE 同 s_texture 注]。采样器单元=7[ThematicSensors]）。
    frag.addUniform("s_sensorSampler", VariableType::Sampler2D, [](ShaderProgram& prog) {
        prog.addGraphicUniform("s_sensorSampler", [](UniformHandle& u, DrawParams const&) {
            u.setUniform1i(7);
        });
    });

    // u_discardBetweenIsolines（:320-327——ProgramUniform：readPixels 中置 1，
    // isoline 间片元 discard 以保拾取）。
    frag.addUniform("u_discardBetweenIsolines", VariableType::Boolean, [](ShaderProgram& prog) {
        prog.addProgramUniform("u_discardBetweenIsolines", [](UniformHandle& u, ShaderProgramParams const& p) {
            u.setUniform1i((p.getTarget() && p.getTarget()->isReadPixelsInProgress()) ? 1 : 0);
        });
    });

    // Gradient LUT texture (s_texture——采样器注册；**绑定在绘制环**：
    // SceneCompositorImpl 的 s_texture 绑定位按 isThematic 绑渐变纹理
    // [DanQing 绑定架构=绘制环 params.setInt，EQUIVALENCE 登记——参考
    //  Surface.ts:599-601 为 GraphicUniform 内绑])。
    frag.addUniform("s_texture", VariableType::Sampler2D, nullptr);

    // --- Vertex：findFractionalPositionOnLine + v_thematicIndex ---
    //（Surface.ts:573 addInlineComputedVarying + getComputeThematicIndex
    //  Thematic.ts:172-191——computed-varying 发射位在组件链尾[rawPosition
    //  在域]，instanced 臂读 builder.usesInstancedGeometry[:228-229 同面]；
    //  多语句 if/else 块经 addComputedVarying 原样入列[ShaderBuilder.cpp:758
    //  发射位]）。
    vert.addFunction(std::string(kFindFractionalPositionOnLine));
    builder.addVarying("v_thematicIndex", VariableType::Float);
    {
        // TEMP-DIAG（M-S S-d 取证——DANQING_UV_DEBUG 同族）：5=顶点侧
        // rawPosition.z 直写（读回片元 v_thematicIndex 即 raw z/12 灰度）。
        bool const dbgVert5 = [] {
            if (char const* d = getenv("DANQING_THM_FRAGDBG"))
                return std::string(d) == "5";
            return false;
        }();
        if (dbgVert5)
            vert.addComputedVarying("  v_thematicIndex = rawPosition.z / 12.0;\n");
        else
            vert.addComputedVarying(std::string(vert.usesInstancedGeometry()
                ? kComputeThematicIndexInstanced : kComputeThematicIndex));
    }

    // --- Fragment helper functions ---
    frag.addFunction(std::string(kUniversalFwidth));
    frag.addFunction(std::string(kThematicGetColor));
    frag.addFunction(std::string(kThematicGetSensor));
    frag.addFunction(std::string(kThematicGetIsoLineColor));

    // Fragment slot: applyThematicDisplay（prelude[NDX 计算：Height 直通/
    // IDW 传感器循环/Slope/HillShade——g_normal 消费] + postlude[渐变采样/
    // isolines/delimiter] + return）。**槽体=函数体**（function-call fragment
    // main 约定：buildFragmentMain 将槽体包成 `vec4 applyThematicDisplay(vec4
    // baseColor){...}` 并在 main 调用——has()=非空门，空槽=永不调用
    // [S-d 取证实录：空槽形态致 thematic 零像素效应]）。
    std::string slotBody = std::string(kApplyThematicColorPrelude)
        + std::string(kApplyThematicColorPostlude)
        + "  return baseColor;\n";
    // TEMP-DIAG（M-S S-d 取证——DANQING_UV_DEBUG 同族）：1=g_normal 可视化、
    // 2=ndx 灰度、3=u_thematicAxis 可视化、4=u_thematicRange→rg。
    if (char const* dbg = getenv("DANQING_THM_FRAGDBG")) {
        std::string const mode = dbg;
        if (mode == "1")
            slotBody = "  return vec4(normalize(g_normal) * 0.5 + 0.5, 1.0);\n";
        else if (mode == "2")
            slotBody = std::string(kApplyThematicColorPrelude)
                + "  return vec4(vec3(ndx), 1.0);\n";
        else if (mode == "3")
            slotBody = "  return vec4(u_thematicAxis * 0.5 + 0.5, 1.0);\n";
        else if (mode == "4")
            slotBody = "  return vec4(u_thematicRange.x / 3.14159, u_thematicRange.y / 3.14159, 0.0, 1.0);\n";
        else if (mode == "6")
            slotBody = "  return vec4(vec3(v_thematicIndex), 1.0);\n";
        else if (mode == "7")  // S-e IDW 取证：shader 实读 sensor0 位（|p|/10）
            slotBody = "  return vec4(abs(getSensor(0).xyz) * 0.1, 1.0);\n";
        else if (mode == "8")  // sensor1 位
            slotBody = "  return vec4(abs(getSensor(1).xyz) * 0.1, 1.0);\n";
        else if (mode == "9")  // 双传感器值通道（R=sensor0.w, G=sensor1.w）
            slotBody = "  return vec4(getSensor(0).w, getSensor(1).w, 0.0, 1.0);\n";
        else if (mode == "10")  // S-e IDW 取证：v_eyeSpace 帧判别（×0.02+0.5）
            slotBody = "  return vec4(v_eyeSpace * 0.02 + 0.5, 1.0);\n";
    }
    frag.setFragmentComponent(FragmentShaderComponent::ApplyThematicDisplay, slotBody);
}

END_DQ_RENDER_NAMESPACE
