// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface common builder helpers
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              createCommon() (line 265-298)
//              Color.ts addColor() (line 51-70)
//              Common.ts addFragColorWithPreMultipliedAlpha()
//
// Provides the foundation layer for Surface shader composition:
// - createCommon(): position pipeline (u_mvp/u_proj, u_mv, addFrustum,
//   u_renderOrder, v_eyeSpace, ComputePosition)
// - addColor(): v_color varying + a_color attribute + ComputeBaseColor
// - addFragData(): fragColor output + assignFragData function
#pragma once

#include "CommonShaders.h"   // addFrustum
#include "InstancingShaders.h"  // addInstancedModelMatrixRTC / addInstanceColor（TD-25）
#include "FeatureSymbologyShaders.h"  // kComputeLinearDepth/kEncodeDepthRgb（M-T T-e AO 深度写）
#include "shader/ColorShaders.h"  // kColorComputeVertexColorQuantized[Instanced]（Color.ts getComputeColor 全文）
#include "RenderPassShaders.h"  // addRenderPass (authoritative u_renderPass + kRenderPass_*)
#include "ShaderBindings.h"  // wireProjectionMatrix, wireModelViewMatrix
#include "ShaderBuilder.h"
#include "VertexTable.h"     // addVertexTable (full LUT path)

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RenderOrder constants (GLSL side)
// Ported from: itwinjs-core FeatureSymbology.ts addRenderOrderConstants()
// ---------------------------------------------------------------------------

inline constexpr float kRenderOrder_BlankingRegion_f = 2.0f;

// ---------------------------------------------------------------------------
// createCommon — foundation layer for Surface shaders
// Ported from: itwinjs-core Surface.ts createCommon() (line 265-298)
//              + Vertex.ts addProjectionMatrix (:8-14) / addModelViewMatrix
//              (:142-160) / ShaderBuilder.ts VertexShaderBuilder ctor
//              (:712-723 —— usesInstancedGeometry 时 addInstancedModelMatrixRTC
//              + MAT_MV=g_mv / MAT_MVP=g_mvp 定义)
//
// Creates the vertex-side position pipeline:
// - Non-instanced: u_mvp (legacy upload) + u_mv (GraphicUniform binding)
// - Instanced:     u_proj (ProgramUniform binding) + per-instance
//                  a_instanceMatrixRow0/1/2 (divisor 1) → g_modelMatrixRTC
//                  (Instancing.ts addInstancedModelMatrixRTC) →
//                  g_mv = u_instanced_modelView * g_modelMatrixRTC
//                  (Vertex.ts addModelViewMatrix 实例分支 :147-154)
// - Quantized LUT: delegates to addVertexTable (full texture decode path)
// - Non-quantized: a_position attribute (simplified, §3.4 deviation)
// - addFrustum (u_frustum ProgramUniform binding)
// - u_renderOrder uniform + kRenderOrder_BlankingRegion constant
// - v_eyeSpace varying (eye-space position for lighting / blanking offset)
// - ComputePosition: model-view transform + blanking offset + projection
//
// Animation/shadow-map branches omitted — TODO follow-up.
// ---------------------------------------------------------------------------
inline void createCommon(ProgramBuilder& builder, bool instanced, bool quantized = false,
                         bool lutUnquant = false)
{
    auto& vert = builder.getVertexBuilder();
    (void)builder.getFragmentBuilder();  // frag used below for varying/uniform registration

    // TEXTURE/TEXTURE_CUBE/TEXTURE_PROJ macros are now added automatically
    // by the ShaderBuilder constructor (ShaderBuilder.h addDefaultMacros()).

    // TD-27（M-M(4)）：imdl unquantized-LUT（numRgba=5）走 addVertexTable 的
    // 非量化分支（kComputeUnquantizedPositionFromLUT + kPreReadVertexData
    // Unquantized——Vertex.ts:43-56/:206-212 预移植）；canvas 属性路径
    //（§3.4 a_position）保持 lutUnquant=false。
    (void)lutUnquant;

    if (instanced) {
        // Instanced attribute declarations（AttributeMap.ts:36-51 instanced 追加组——
        // a_pos@0 之后 a_instanceMatrixRow0/1/2、a_instanceOverrides、a_instanceRgba、
        // a_featureId、a_patternX/Y；位置由 prog.setAttributeMap(glBindAttribLocation)
        // 固定，此处只声明，未绑定缓冲的属性读取常量默认（参考行为一致））。
        // a_instanceOverrides/a_instanceRgba 的声明归 addInstanceColor
        // （InstancingShaders.h——其 addInstanceOverrides 幂等检查以
        // a_instanceOverrides 名为键，检查必须在声明之前发生）；
        // a_featureId 声明归 SurfaceVariantCompiler 的 feature 段（FeatureMode
        // 门控）；a_patternX/Y 供 addInstancedModelMatrixRTC 的 area-pattern
        // 分支引用（Instancing.ts:25-31——本资产恒 g_isAreaPattern=0 走
        // 实例矩阵分支，声明仅为编译通过，参考 attrMap 亦恒声明）。
        vert.addVariable({"a_instanceMatrixRow0", VariableType::Vec4, VariableScope::Attribute, 0});
        vert.addVariable({"a_instanceMatrixRow1", VariableType::Vec4, VariableScope::Attribute, 0});
        vert.addVariable({"a_instanceMatrixRow2", VariableType::Vec4, VariableScope::Attribute, 0});
        vert.addVariable({"a_patternX", VariableType::Float, VariableScope::Attribute, 0});
        vert.addVariable({"a_patternY", VariableType::Float, VariableScope::Attribute, 0});

        // g_modelMatrixRTC = mat4(a_instanceMatrixRow0/1/2) 逐实例模型矩阵（RTC——
        // 平移列相对 transformCenter，渲染时由 u_instanced_modelView 侧 RTC 加回：
        // BranchUniforms.ts:217-227 mv = view * getRtcModelTransform(model)）。
        // Ported from: Instancing.ts addInstancedModelMatrixRTC()
        //（VertexShaderBuilder ctor 对 usesInstancedGeometry 无条件调用，
        //  ShaderBuilder.ts:716-717）。
        addInstancedModelMatrixRTC(vert);

        // Projection matrix（ProgramUniform，绑定在 use() 时从 target 上传）。
        // Ported from: itwinjs-core Vertex.ts addProjectionMatrix() line 8-14
        wireProjectionMatrix(vert);

        // Instanced model-view：参考 bindModelViewMatrix 对 instanced 几何
        // 绑 view * model * rtcOnly（BranchUniforms.ts:217-227）；着色器侧乘
        // 逐实例 g_modelMatrixRTC 得完整 eye 变换（Vertex.ts:147-154）。
        vert.addUniform("u_instanced_modelView", VariableType::Mat4, nullptr);
        vert.addGlobal("g_mv", VariableType::Mat4);
        vert.addInitializer("g_mv = u_instanced_modelView * g_modelMatrixRTC;");
    } else {
        // Non-instanced path: u_mvp (legacy upload via params.setMatrix4).
        // No binding — the draw loop uploads it directly.
        vert.addUniform("u_mvp", VariableType::Mat4, nullptr);

        // Model-view matrix (non-instanced: GraphicUniform from DrawParams).
        // Ported from: itwinjs-core Vertex.ts addModelViewMatrix() line 38-41
        wireModelViewMatrix(vert);
    }

    // Position attribute(s) — quantized LUT / unquantized LUT / attribute path.
    // Ported from: itwinjs-core Vertex.ts addPosition() (line 258-280)
    if (quantized || lutUnquant) {
        // Full LUT path: delegates to addVertexTable which sets up all
        // LUT globals, coordinate computation, position decode, texture
        // bindings, and pre-read initializers. quantized=false + lutUnquant=
        // true → the 20B/vertex unquantized table decode (TD-27).
        addVertexTable(builder, /*quantized*/quantized);
    } else {
        // §3.4 deviation: simplified attribute path (a_position → rawPos).
        // itwinjs uses VertexLUT for all paths; DanQing uses direct attributes
        // for non-quantized geometry. TODO: full LUT for unquantized too.
        vert.addVariable({"a_position", VariableType::Vec3, VariableScope::Attribute, 0});
        vert.setVertexComponent(VertexShaderComponent::AdjustRawPosition,
            "    return vec4(a_position, 1.0);\n");
    }

    // Frustum uniform (u_frustum near/far/type) + binding.
    // Ported from: itwinjs-core Common.ts addFrustum()
    addFrustum(builder);

    // Render order uniform + blanking region constant.
    // Ported from: itwinjs-core FeatureSymbology.ts addRenderOrder() +
    //              addRenderOrderConstants() (partial — only BlankingRegion).
    vert.addUniform("u_renderOrder", VariableType::Float, nullptr);
    vert.addConstant("kRenderOrder_BlankingRegion", VariableType::Float,
                     std::to_string(static_cast<int>(kRenderOrder_BlankingRegion_f)));

    // Eye-space position varying (used by lighting, blanking, classification).
    // Ported from: itwinjs-core Surface.ts createCommon() line 282
    builder.addVarying("v_eyeSpace", VariableType::Vec3);

    // --- ComputePosition: model-view + blanking offset + projection ---
    // Ported from: itwinjs-core Surface.ts computePositionPrelude + adjustEyeSpace
    //              + computePositionPostlude (line 240-263)；MAT_MV 定义见
    //              ShaderBuilder.ts:716-723（instanced=g_mv，非 instanced=u_mv）。
    if (instanced) {
        vert.setVertexComponent(VertexShaderComponent::ComputePosition,
            "    vec4 pos = g_mv * rawPos;\n"
            "    v_eyeSpace = pos.xyz;\n"
            "    if (kRenderOrder_BlankingRegion == u_renderOrder)\n"
            "        v_eyeSpace.z -= 2.0 / 65536.0 * (u_frustum.y - u_frustum.x);\n"
            "    return u_proj * pos;\n");
    } else {
        vert.setVertexComponent(VertexShaderComponent::ComputePosition,
            "    vec4 pos = u_mv * rawPos;\n"
            "    v_eyeSpace = pos.xyz;\n"
            "    if (kRenderOrder_BlankingRegion == u_renderOrder)\n"
            "        v_eyeSpace.z -= 2.0 / 65536.0 * (u_frustum.y - u_frustum.x);\n"
            "    return u_mvp * rawPos;\n");
    }
}

// ---------------------------------------------------------------------------
// addColor — vertex color varying
// Ported from: itwinjs-core Color.ts addColor() (line 51-70)
//              + addVaryingColor() (line 65-70)
//              + getComputeElementColor() (line 16-26)
//              + getComputeColor() (line 37-45 —— instanced 时
//              applyInstanceColor 混入 a_instanceRgba 逐实例色)
//
// Non-quantized (§3.4 deviation): reads color from a_color attribute instead
// of the color LUT texture appended to the vertex data.
// Quantized: full getComputeElementColor() — colorIndex =
// decodeUInt16(g_vertLutData1.zw)，colorTableStart = u_vertParams.z ×
// u_vertParams.w（顶点表字节尾部的色表——VertexTableBuilder.appendColorTable
// 追加段，随 u_vertLUT 直传已在纹理内），computeLUTCoords 定位采样 + 预乘
// 还原（rgb /= alpha），u_shaderFlags[kShaderBit_NonUniformColor] 位选
// lutColor : u_color（Color.ts:24）。均匀色（flag=false）时即 u_color——
// 与旧"仅 u_color"路径逐位同结果（既有均匀色资产零行为变化）。
//
// Adds:
//   Vertex:  (non-quantized: a_color attribute) / (quantized: u_color uniform)
//            + ComputeBaseColor slot
//   Varying: v_color (vec4)
//   Fragment: ComputeBaseColor slot (return v_color)
// ---------------------------------------------------------------------------
inline void addColor(ProgramBuilder& builder, bool quantized = false, bool instanced = false,
                     bool lutUnquant = false)
{
    auto& vert = builder.getVertexBuilder();

    // v_color varying
    builder.addVarying("v_color", VariableType::Vec4);

    if (quantized || lutUnquant) {
        // LUT path — Ported from: itwinjs-core Color.ts addColor()
        // (line 51-62)。u_color uniform：均匀元素色（非均匀时 dispatch 侧
        // 不绑——Color.ts:56 仅 color.isUniform 才 bind；flag 位选权在
        // u_shaderFlags[kShaderBit_NonUniformColor]，见 getComputeElementColor）。
        // 色源随表形态：量化 g_vertLutData1.zw / 非量化 g_vertLutData4.xy
        //（Color.ts:16-26 vertData 选择器）。
        vert.addUniform("u_color", VariableType::Vec4, nullptr);

        if (instanced) {
            // 逐实例 symbology 色（Color.ts getComputeColor :37-45——instanced
            // 时 addInstanceColor 接线 a_instanceOverrides/a_instanceRgba +
            // u_applyInstanceColor + extractInstanceBit；applyInstanceColor
            // :32-35 在元素色之上 mix 逐实例 rgb/alpha）。
            addInstanceColor(vert);
            vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
                std::string(lutUnquant ? kColorComputeVertexColorUnquantizedInstanced
                                       : kColorComputeVertexColorQuantizedInstanced));
        } else {
            // Vertex ComputeBaseColor: getComputeColor 全文
            //（getComputeElementColor + returnColor——ColorShaders.h 的常量
            // 即 Color.ts:16-26/:28-30 的逐行 GLSL）。
            vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
                                    std::string(lutUnquant ? kColorComputeVertexColorUnquantized
                                                           : kColorComputeVertexColorQuantized));
        }
    } else {
        // a_color attribute
        vert.addVariable({"a_color", VariableType::Vec4, VariableScope::Attribute, 0});

        // Vertex ComputeBaseColor: return the per-vertex color.
        // Ported from: itwinjs-core Color.ts getComputeColor() (simplified —
        // no LUT, no instance color; TODO follow-up with VertexLUT).
        vert.setVertexComponent(VertexShaderComponent::ComputeBaseColor,
                                "    return a_color;\n");
    }

    // Fragment ComputeBaseColor is owned by addTexture (kComputeBaseColor calls
    // sampleSurfaceTexture + getSurfaceColor, the latter returning v_color).
    // Ported from: itwinjs-core Color.ts addColor (vertex-only for ComputeBaseColor).
}

// ---------------------------------------------------------------------------
// addFragData — fragment output declaration + assignFragData function
// Ported from: itwinjs-core Common.ts addFragColorWithPreMultipliedAlpha()
//               + Fragment.ts addPickBufferOutputs (:77-107 — pick pass 换掉
//               整个输出：参考写 MRT FragColor1=RGBA 打包 feature_id；DanQing
//               的 pick 附件是单 R32UI，等价为单 uint 输出)
//
// Declares the fragment output variable and defines the assignFragData()
// function that buildFragmentMain calls at the end of the fragment chain.
// pickOutput=true declares a uint output for the R32UI pick attachment; the
// AssignFragData slot is then owned by the caller (Pick branch of
// SurfaceVariantCompiler).
// ---------------------------------------------------------------------------
inline void addFragData(ProgramBuilder& builder, bool pickOutput = false,
                        bool writeDepthOrder = false)
{
    auto& frag = builder.getFragmentBuilder();

    if (pickOutput) {
        // Pick pass: attachment 0 (R32UI) takes the uint feature id; attachment 1
        // (RGBA8) takes depthAndOrder（参考 Fragment.ts addPickBufferOutputs 的
        // MRT 双写：feature_id + renderOrder/encodedDepth——DanQing 拆为独立附件）。
        // 两个输出必须显式 layout location（MRT 无默认序）。
        frag.addCode("layout(location = 0) out uint fragColor;\n"
                     "layout(location = 1) out vec4 fragDepthOrder;\n");
        addRenderPass(frag);
        return;
    }

    // Fragment output declaration.
    // M-T T-e：AO 的 depthAndOrder 供给——writeDepthOrder 开（Surface 面）时加
    // FragColor2 输出（layout 2=MRT FBO 的 depthAndOrder 附件位；参考
    // Fragment.ts addPickBufferOutputs 的 output2 语义[order*0.0625 + RGB 打包
    // 深度]——DanQing 的变体分裂[Pick 变体整换输出]使常态帧从不写 pick 附件，
    // AO 的 PB 臂需要逐像素深度+order 通道 → 常态写[屏绘时附件缺席被 GL 丢弃，
    // 零像素效应]）。**PointString/Polyline 等共享本函数而无 u_frustum/
    // v_eyeSpace 的面恒 false**（AO 不作用于线/点——参考的 order 门[Linear 等
    // 恒跳]同效；实测误开编译错[u_frustum/v_eyeSpace 未声明]实锤共享面）。
    // EQUIVALENCE（§11.10，E7）：order 通道恒写 LitSurface×0.0625（参考的
    // u_renderOrder 逐几何上传在 DanQing 恒 0[SurfaceVariantCompiler.cpp:266-267
    // 既有登记]；写 4.0=LitSurface 使 AO 的 order 门[≥LitSurface 才遮蔽]对本
    // shader 族恒过——本族即 LitSurface；平面位族[PlanarBit]DanQing 面不产，
    // 登记）。featureId 附件（location 1）本臂不写——AO 不读、参考的
    // featureId 写随 RGBA 打包拾取链统一时归位（R32UI 分歧既有登记 TD-28②族）。
    if (writeDepthOrder)
        frag.addCode("layout(location = 0) out vec4 fragColor;\n"
                     "layout(location = 2) out vec4 fragDepthOrder;\n");
    else
        frag.addCode("out vec4 fragColor;\n");

    // Render pass uniform + the kRenderPass_* constants (OpaqueLinear=2, OpaquePlanar=3,
    // OpaqueGeneral=5, WorldOverlay=12, ...). Use the AUTHORITATIVE addRenderPass() — do
    // NOT re-roll local constants: deviant values (the prior 0/1/2) made the assignFragData
    // check below treat the default u_renderPass=0 (never uploaded) as an opaque pass and
    // force baseColor.a=1.0, which rendered every translucent overlay fill — e.g. the ACS
    // triad's arrows + origin disc (alpha ~0.22) — as fully opaque. With the faithful
    // constants, 0 falls outside [2..5] so the else branch premultiplies (faint fill).
    // Ported from: itwinjs-core RenderPass.ts addRenderPass().
    addRenderPass(frag);

    // assignFragData — function body in the AssignFragData slot; buildFragmentMain
    // wraps it as `void assignFragData(vec4 baseColor)` and emits the call.
    // Ported from: itwinjs-core Fragment.ts addFragColorWithPreMultipliedAlpha()
    //              + multiplyAlpha (line 45-50).
    // M-T T-e：+ depthAndOrder 写（encodeDepthRgb/computeLinearDepth——
    // FeatureSymbologyShaders.h 同源；仅 opaque pass 域写[参考
    // addPickBufferOutputs 的 pass 门]——其余 pass 写恒等占位防未初始化读）。
    if (writeDepthOrder) {
        frag.addFunction(std::string(kComputeLinearDepth.data(), kComputeLinearDepth.size()));
        frag.addFunction(std::string(kEncodeDepthRgb.data(), kEncodeDepthRgb.size()));
        frag.setFragmentComponent(FragmentShaderComponent::AssignFragData,
            "    if (u_renderPass >= kRenderPass_OpaqueLinear && u_renderPass <= kRenderPass_OpaqueGeneral)\n"
            "        baseColor.a = 1.0;\n"
            "    else\n"
            "        baseColor = vec4(baseColor.rgb * baseColor.a, baseColor.a);\n"
            "    fragColor = baseColor;\n"
            "    if (u_renderPass >= kRenderPass_OpaqueLinear && u_renderPass <= kRenderPass_OpaqueGeneral)\n"
            "        fragDepthOrder = vec4(0.25, encodeDepthRgb(computeLinearDepth(v_eyeSpace.z)));\n"
            "    else\n"
            "        fragDepthOrder = vec4(0.0);\n");
    } else {
        frag.setFragmentComponent(FragmentShaderComponent::AssignFragData,
            "    if (u_renderPass >= kRenderPass_OpaqueLinear && u_renderPass <= kRenderPass_OpaqueGeneral)\n"
            "        baseColor.a = 1.0;\n"
            "    else\n"
            "        baseColor = vec4(baseColor.rgb * baseColor.a, baseColor.a);\n"
            "    fragColor = baseColor;\n");
    }
}

END_DQ_RENDER_NAMESPACE
