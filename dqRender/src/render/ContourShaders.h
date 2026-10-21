// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contour display shader module
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Contours.ts
//
// addApplyContours wires contour-line display into a Surface ProgramBuilder:
//   vertex: v_contourNdx (feature→group index via per-batch u_contourLUT) +
//           v_height (world-space z via u_modelToWorldC);
//   fragment: applyContours (line distance via fwidth, major/minor styling,
//             pattern, showGeometry discard).
// Per the reference (Surface.ts:776), the module compiles into EVERY surface
// variant — there is no TechniqueFlags bit; the runtime early-out is
// u_contourLUTWidth==0u (Contours.ts:17-18).
#pragma once

#include "ShaderBuilder.h"
#include "shader/DecodeShaders.h"  // kDecodeUint24（getFeatureIndex 的 decodeUInt24）

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// computeContourNdx（Contours.ts:16-29——LUT width==0→15 早退 + 4-bit nibble
// 解包）。函数体（addFunctionComputedVarying 的 funcBody 段）。
// ---------------------------------------------------------------------------
inline constexpr char const* kComputeContourNdx = R"glsl(
  if (u_contourLUTWidth == 0u)
      return 15.0;
  uint lutIndex = uint(getFeatureIndex());
  bool odd = bool(lutIndex & 1u);
  lutIndex /= 2u;
  uint byteSel = lutIndex & 0x3u;
  lutIndex /= 4u;
  ivec2 coords = ivec2(lutIndex % u_contourLUTWidth, lutIndex / u_contourLUTWidth);
  uvec4 contourNdx4 = uvec4(texelFetch(u_contourLUT, coords, 0) * 255.0 + 0.5);
  uvec2 contourNdx2 = bool(byteSel & 2u) ? contourNdx4.ba : contourNdx4.rg;
  uint contourNdx = bool(byteSel & 1u) ? contourNdx2.g : contourNdx2.r;
  return float(odd ? contourNdx >> 4u : contourNdx & 0xFu);
)glsl";

// ---------------------------------------------------------------------------
// computeWorldHeight（Contours.ts:105-111——instanced 臂 modelPos=
// `(g_instancedRtcMatrix * rawPosition)`；完整函数定义[含形参]——
// addFunctionComputedVaryingWithArgs 的 funcDef 段）。
// TODO: apply ECEF correction to height（参考原注 :108 保留）。
// ---------------------------------------------------------------------------
inline constexpr char const* kComputeWorldHeight = R"glsl(
float computeWorldHeight(vec4 rawPosition) {
  float height = (u_modelToWorldC * rawPosition).z;
  return height;
}
)glsl";

inline constexpr char const* kComputeWorldHeightInstanced = R"glsl(
float computeWorldHeight(vec4 rawPosition) {
  float height = (u_modelToWorldC * (g_instancedRtcMatrix * rawPosition)).z;
  return height;
}
)glsl";

// ---------------------------------------------------------------------------
// unpack2BytesVec4 / unpackAndNormalize2BytesVec4（Contours.ts:31-44）。
// ---------------------------------------------------------------------------
inline constexpr char const* kUnpack2BytesVec4 = R"glsl(
vec4 unpack2BytesVec4(vec4 f, bool upper) {
  f = floor(f + 0.5);
  vec4 outUpper = floor(f / 256.0);
  vec4 outLower = floor(f - outUpper * 256.0);
  return upper ? outUpper : outLower;
}
)glsl";

inline constexpr char const* kUnpackAndNormalize2BytesVec4 = R"glsl(
vec4 unpackAndNormalize2BytesVec4(vec4 f, bool upper) {
  return unpack2BytesVec4(f, upper) / 255.0;
}
)glsl";

// ---------------------------------------------------------------------------
// applyContours（Contours.ts:46-99）——FragmentShaderComponent::ApplyContours
// 槽体（函数体形——buildFragmentMain 包成 `vec4 applyContours(vec4 baseColor)`
// 并在 main 调用[ShaderBuilder.ts:1097-1101——reverseWoW 之后 applyFlash 之前]）。
// maxDefs=ContourDisplay.maxContourGroups=5（:51——"have to change index arrays
// if this changes"）。
// ---------------------------------------------------------------------------
inline constexpr char const* kApplyContours = R"glsl(
  int contourNdx = int(v_contourNdx + 0.5);
  if (contourNdx > 14) // 15 => no contours
    return baseColor;

  const int maxDefs = 5; // max number of contour definitions allowed, have to change index arrays if this changes
  int contourNdxC = clamp(contourNdx, 0, maxDefs - 1);

  bool even = (contourNdxC & 1) == 0;
  vec4 rgbfp = u_contourDefs[even ? contourNdxC * 3 / 2 : (contourNdxC - 1) * 3 / 2 + 2];
  vec4 intervalsPair = u_contourDefs[(contourNdxC / 2) * 3 + 1];
  // intervals.r => minor interval distance, intervals.g => major index count
  vec2 intervals = even ? intervalsPair.rg : intervalsPair.ba;

  float coord = v_height / intervals.r;
  // determine if this is in the vicinity of a major contour line (1/2 an interval either way, which is one limit of drawing width)
  bool maj = (fract((abs(coord) + 0.5) / intervals.g) < (1.0 / intervals.g));
  vec4 rgbf = unpackAndNormalize2BytesVec4(rgbfp, maj);
  // rgbf.a => (4-bit linecode / 4-bit weight) maj/min, where the 4-bit weight is a 3-bit weight value with one fraction bit and a 1.5 offset.
  //    This gives a weight range of 1.5 to 9 in 0.5 increments.
  //    NB: the showGeometry bit flag is stuck in at bit 16 of this, so the major line code has an extra bit in it that needs to be masked out
  int lineCodeWt = int((rgbf.a * 255.0) + 0.5);
  // first * 0.5 is for fractional part of width, then have to add 1.0 for offset, then another 1.0 for actual width bias
  float lineRadius = (float(lineCodeWt & 0xf) * 0.5 + 2.0) * 0.5;

  // abs(fract(coord - 0.5) - 0.5) will produce 0.0 at the contour line, and 0.5 at the mid-point between contour lines
  // fwidth(coord) is sum of absolute diffs in coord in adjacent pixels
  float line = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
  // If line is 0 (like at contour line), contourAlpha = lineRadius, so will use draw in contour color
  // If line >= lineRadius, contourAlpha = 0, so won't show contour
  float contourAlpha = lineRadius - min(line, lineRadius);

  // figure out which direction line is going, to know which screen pattern offset to use
  float dx = dFdx(contourAlpha);
  float dy = dFdy(contourAlpha);

  const float patLength = 32.0;
  uint patterns[10] = uint[](0xffffffffu, 0x80808080u, 0xf8f8f8f8u, 0xffe0ffe0u, 0xfe10fe10u, 0xe0e0e0e0u, 0xf888f888u, 0xff18ff18u, 0xccccccccu, 0x00000001u);

  float offset = trunc((abs(dx) > abs(dy)) ? gl_FragCoord.y : gl_FragCoord.x);
  offset = mod(offset, patLength);
  uint msk = 1u << uint(offset);
  contourAlpha *= (patterns[(lineCodeWt / 16) & 0xf] & msk) > 0u ? 1.0 : 0.0;
  contourAlpha = min(contourAlpha, 1.0);

  if (rgbfp.a / 65536.0 < 0.5) { // showGeometry == 0
    if (contourAlpha < 0.5) // not a contour line
      discard;
    return vec4(rgbf.rgb, 1.0);
  }
  // set contour opaque even if base color is transparent
  float alpha = contourAlpha >= 0.5 ? 1.0 : baseColor.a;
  return vec4(mix(baseColor.rgb, rgbf.rgb, contourAlpha), alpha);
)glsl";

// ---------------------------------------------------------------------------
// addFeatureIndex（FeatureSymbology.ts:131-136——g_featureIndex 全局 +
// decodeUInt24 + getFeatureIndex；Contours.ts:113 从 addApplyContours 调用）。
// getFeatureIndex 体（:79-86 + computeFeatureIndex :63-78）：
//   instanced → g_featureIndex = g_isAreaPattern ? u_patternFeatureId : a_featureId
//   顶点表  → g_featureIndex = g_featureAndMaterialIndex.xyz
//   属性路径 → 空体（g_featureIndex 保持全零初值——参考同形，特征索引 0）。
// ---------------------------------------------------------------------------
inline void addContourFeatureIndex(ShaderBuilder& vert)
{
    vert.addGlobal("g_featureIndex", VariableType::Vec3);
    vert.addFunction(std::string(kDecodeUint24));

    std::string body;
    if (vert.usesInstancedGeometry()) {
        // u_patternFeatureId uniform（FeatureSymbology.ts:65-71）+ a_featureId
        // 声明（参考经 AttributeMap instanced 组恒声明——DanQing 逐变量声明面
        // 的等价补；SVC 特征段同名声明经 addVariable 去重幂等）。
        vert.addUniform("u_patternFeatureId", VariableType::Vec3, nullptr);
        vert.addVariable({"a_featureId", VariableType::Vec3, VariableScope::Attribute, 0});
        body = "  g_featureIndex = g_isAreaPattern ? u_patternFeatureId : a_featureId;\n";
    } else if (vert.usesVertexTable()) {
        body = "  g_featureIndex = g_featureAndMaterialIndex.xyz;\n";
    }
    vert.addFunction("float getFeatureIndex() {\n" + body +
                     "  return decodeUInt24(g_featureIndex);\n}\n");
}

// ---------------------------------------------------------------------------
// addApplyContours — wire contour display into a ProgramBuilder
// Ported from: itwinjs-core glsl/Contours.ts addApplyContours()（:102-151）
// ---------------------------------------------------------------------------
inline void addApplyContours(ProgramBuilder& builder)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    addContourFeatureIndex(vert);

    // v_contourNdx（addFunctionComputedVarying——Contours.ts:115）。
    builder.addFunctionComputedVarying("v_contourNdx", VariableType::Float,
                                       "computeContourNdx", kComputeContourNdx);

    // v_height（addFunctionComputedVaryingWithArgs——Contours.ts:116：
    // funcDef=完整含参函数，调用=`computeWorldHeight(rawPosition)`；DanQing
    // 以 addFunction(完整定义) + addInlineComputedVarying(调用) 承载同形）。
    vert.addFunction(std::string(vert.usesInstancedGeometry()
                                     ? kComputeWorldHeightInstanced
                                     : kComputeWorldHeight));
    builder.addInlineComputedVarying("v_height", VariableType::Float,
                                     "computeWorldHeight(rawPosition)");

    // addInstancedRtcMatrix（Contours.ts:118-119）——DanQing 的等价物
    // addInstancedModelMatrixRTC 已在 createCommon 的 instanced 臂恒跑
    //（SurfaceCommon.h:103——g_instancedRtcMatrix/g_modelMatrixRTC 全域在域），
    // addGlobal/addFunction 去重幂等，此处无需重跑。

    // u_contourLUT（vert sampler——Contours.ts:121-127；sampler 索引恒写
    // [参考注：未绑定时防 loopback] + GPU 纹理绑=绘制环 EQUIVALENCE
    // [BatchUniforms.cpp bindContourLUT 注——s_sensorSampler 同族]）。
    vert.addUniform("u_contourLUT", VariableType::Sampler2D, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_contourLUT", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().batch.bindContourLUT(u);
        });
    });

    // u_contourLUTWidth（vert uint——Contours.ts:129-133）。
    vert.addUniform("u_contourLUTWidth", VariableType::Uint, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_contourLUTWidth", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().batch.bindContourLUTWidth(u);
        });
    });

    // u_modelToWorldC（vert mat4——Contours.ts:135-139；DanQing 承载=
    // branch.bindModelMatrix[m_model=m32 等价面——ThematicDisplayShaders
    // u_modelToWorld 同族注]；m32 的填充门=BranchUniforms.update 的
    // wantContourLines 臂[Uniforms.h:361——BranchUniforms.ts:243]）。
    vert.addUniform("u_modelToWorldC", VariableType::Mat4, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_modelToWorldC", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().branch.bindModelMatrix(u);
        });
    });

    // u_contourDefs（frag vec4[8]——Contours.ts:141-146；contourDefsSize=8=
    // ContourUniforms 的 ceil(maxContourGroups*1.5)[ContourUniforms.ts:19-21]；
    // 注册名带 "[0]" 后缀——数组 location 解析需元素名[u_lightSettings[0]
    // 先例，M-P P-D 实录]）。
    frag.addUniformArray("u_contourDefs", VariableType::Vec4, 8, [](ShaderProgram& prog) {
        prog.addGraphicUniform("u_contourDefs[0]", [](UniformHandle& u, DrawParams const& dp) {
            if (auto* t = dp.getTarget())
                t->getUniforms().contours.bindcontourDefs(u);
        });
    });

    frag.addFunction(std::string(kUnpack2BytesVec4));
    frag.addFunction(std::string(kUnpackAndNormalize2BytesVec4));
    frag.setFragmentComponent(FragmentShaderComponent::ApplyContours, kApplyContours);
}

END_DQ_RENDER_NAMESPACE
