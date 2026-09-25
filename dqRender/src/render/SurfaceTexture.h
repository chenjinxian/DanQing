// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface texture builder helper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addTexture() (line 571-687)
//
// Provides the addTexture() function that wires the surface texture system
// into a ProgramBuilder: s_texture sampler, v_texCoord varying, g_surfaceTexel
// global, sampleSurfaceTexture + getSurfaceColor + ComputeBaseColor.
//
// §3.4 deviation (non-quantized only): itwinjs reads quantized UVs from the
// VertexLUT (g_vertLutData + decodeUInt16 + unquantize2d); DanQing's
// non-quantized geometry path uses a pre-decoded a_texCoord attribute. The
// quantized path reads the LUT verbatim (getComputeTexCoord(true) :460-467).
// Simplified: no glyph/whiteOnWhite, no constantLod (TODO follow-up).
#pragma once

#include "ShaderBuilder.h"
#include "CommonShaders.h"  // addChooseVec2WithBitFlagsFunction
#include <cstdlib>
#include "shader/SurfaceTextureShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addTexture — wire the surface texture system into a ProgramBuilder
// Ported from: itwinjs-core Surface.ts addTexture() (line 571-687), core only.
//
// Parameters:
//   builder   — MUST have surface flags already added via addSurfaceFlags()
//               (sampleSurfaceTexture reads u_surfaceFlags[HasTexture]).
//   quantized — true for LUT geometry: v_texCoord is decoded from
//               g_vertLutData3 (u16 qUV pairs) + unquantize2d(u_qTexCoordParams)
//               (Surface.ts getComputeTexCoord(true) :460-467), gated by
//               chooseVec2With2BitFlags on kSurfaceBit_HasTexture |
//               kSurfaceBit_HasNormalMap — matching the reference addTexture
//               (:578-593), which also declares u_qTexCoordParams on the
//               VERTEX stage. No a_texCoord attribute is declared.
//
// §3.4 deviation (non-quantized): computeTexCoord uses a_texCoord attribute
// instead of itwinjs's VertexLUT path. TODO Step 3 VertexLUT.
// Simplified: sampleSurfaceTexture omits constantLod branch; computeBaseColor
// omits glyph/whiteOnWhite. TODO follow-up.
// ---------------------------------------------------------------------------
inline void addTexture(ProgramBuilder& builder, bool quantized = false)
{
    auto& vert = builder.getVertexBuilder();
    auto& frag = builder.getFragmentBuilder();

    if (quantized) {
        // Quantized LUT path — Ported from: itwinjs-core Surface.ts
        // addTexture() (line 578-582) + getComputeTexCoord(true)
        // (line 460-467): the quantized UVs are two u16 values packed in
        // g_vertLutData3 (byte pairs decoded by decodeUInt16, supplied by
        // addVertexTable), unquantized by u_qTexCoordParams.
        // The reference adds unquantize2d + chooseVec2With2BitFlags to the
        // VERTEX stage here (line 579-580).
        vert.addFunction(std::string(kUnquantize2d));
        addChooseVec2WithBitFlagsFunction(vert);
        builder.addFunctionComputedVarying("v_texCoord", VariableType::Vec2,
                                            "computeTexCoord",
                                            std::string(kComputeTexCoordQuantized));

        // u_qTexCoordParams uniform — VERTEX stage per the reference
        // (Surface.ts:583-594). (DanQing previously declared it on the
        // fragment; the fragment never references it — moved to the vertex
        // to match the reference and serve the LUT decode.)
        vert.addUniform("u_qTexCoordParams", VariableType::Vec4, nullptr);
    } else {
        // a_texCoord attribute (attribute path, §3.4 deviation from itwinjs LUT).
        vert.addVariable({"a_texCoord", VariableType::Vec2, VariableScope::Attribute, 0});

        // v_texCoord computed varying — attribute path (a_texCoord → v_texCoord).
        // Ported from: itwinjs-core Surface.ts getComputeTexCoord() (line 460-467)
        // itwinjs reads quantized UVs from g_vertLutData3/4 + decodeUInt16 +
        // unquantize2d; DanQing reads pre-decoded vec2 from a_texCoord. TODO Step 3.
        builder.addFunctionComputedVarying("v_texCoord", VariableType::Vec2,
                                            "computeTexCoord",
                                            "  return a_texCoord;");
    }

    // s_texture sampler (binding wired by draw loop bindTexture + params.setInt,
    // matching the SceneCompositorImpl L869 hiliteLUT pattern).
    // Ported from: itwinjs-core Surface.ts addTexture() (line 596-609)
    frag.addUniform("s_texture", VariableType::Sampler2D, nullptr);

    // u_applyGlyphTex uniform — controls glyph texture mixing.
    // Ported from: itwinjs-core Surface.ts createSurfaceBuilder() (line 762-767)
    frag.addUniform("u_applyGlyphTex", VariableType::Boolean, nullptr);

    // u_reverseWhiteOnWhite uniform — white-on-white reversal flag.
    // Ported from: itwinjs-core Fragment.ts addWhiteOnWhiteReversal() (line 26-33)
    frag.addUniform("u_reverseWhiteOnWhite", VariableType::Boolean, nullptr);

    // (u_qTexCoordParams is declared on the VERTEX stage above — the reference
    // registers it on the vertex (Surface.ts:583-594) since only the vertex
    // computeTexCoord consumes it.)

    // g_surfaceTexel global.
    frag.addGlobal("g_surfaceTexel", VariableType::Vec4);

    // --- Constant-LOD texture mapping support ---
    // Ported from: itwinjs-core Surface.ts createSurfaceBuilder() (line 798-818).
    // itwinjs wires the constantLod pipeline in createSurfaceBuilder
    // (Surface.ts:798-818), not in addTexture; here we consolidate it into
    // addTexture so the helper is self-contained and SurfaceVariantCompiler no
    // longer re-adds these.
    //
    // v_uvCustom varying: xy = world-space UV, z = depth for LOD computation.
    builder.addVarying("v_uvCustom", VariableType::Vec3);

    // u_constantLodVParams — vertex shader constant-LOD parameters.
    // Ported from: itwinjs-core Surface.ts (line 800-808)
    vert.addUniform("u_constantLodVParams", VariableType::Vec3, nullptr);

    // u_modelToWorld — for computing world-space UVs in the constant-LOD path.
    // Ported from: itwinjs-core Surface.ts (line 739-744)
    vert.addUniform("u_modelToWorld", VariableType::Mat4, nullptr);

    // u_constantLodFParams — fragment shader constant-LOD parameters.
    // Ported from: itwinjs-core Surface.ts (line 810-816)
    frag.addUniform("u_constantLodFParams", VariableType::Vec3, nullptr);

    // v_uvCustom vertex write — trivial placeholder.
    // TODO(Plan 2 — baseColor texture): the real computation
    // (computeConstantLodUvCustom, Surface.ts:256-259) needs `rawPosition` AND
    // `v_eyeSpace.z` (set by computePosition), both only available AFTER
    // computePosition runs in main(). addInitializer emits at the TOP of main()
    // (before rawPosition is declared), so the real computation must move to a
    // post-position vertex slot in Plan 2. The GLSL linker requires the vertex
    // to WRITE every varying the fragment declares as `in`, so emit a trivial
    // zero now; Plan 2 replaces it. (At runtime sampleSurfaceTexture()
    // short-circuits to vec4(1) when kSurfaceBit_HasTexture is off, so the
    // value is unused today.)
    vert.addInitializer("v_uvCustom = vec3(0.0);");

    // constantLodTextureLookup + sampleSurfaceTexture (full version, with the
    // constantLod branch). Ported from: itwinjs-core Surface.ts (line 49-89).
    // addFunction dedups by exact string, so SVC re-adding these is a no-op.
    frag.addFunction(std::string(kConstantLodTextureLookup));
    // TEMP-DIAG（U 翻转 saga）：env 门控——输出 v_texCoord 为颜色（R=u,G=v）。
    if (getenv("DANQING_UV_DEBUG")) {
        frag.addFunction(std::string(kUvDebugSampleSurfaceTexture));
    } else {
        frag.addFunction(std::string(kSampleSurfaceTexture));
    }

    // getSurfaceColor — return vertex color (v_color, declared by addColor).
    // Ported from: itwinjs-core Surface.ts getSurfaceColor (line 478-480)
    frag.addFunction(std::string(kGetSurfaceColor));

    // ComputeBaseColor — function BODY (ends in `return`). buildFragmentMain wraps
    // it as `vec4 computeBaseColor()`; main does `vec4 baseColor = computeBaseColor();`.
    // Ported from: itwinjs-core Surface.ts computeBaseColor (line 486-502).
    frag.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor,
                              std::string(kComputeBaseColor));
}

END_DQ_RENDER_NAMESPACE
