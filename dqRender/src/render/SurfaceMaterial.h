// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface material builder helper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Surface.ts
//              addMaterial() (line 183-238)
//
// Provides the addMaterial() function that wires the material system
// into a ProgramBuilder: adds uniforms, globals, functions, and component slots.
#pragma once

#include "ShaderBindings.h"  // wireMaterialColor
#include "ShaderBuilder.h"
#include "CommonShaders.h"   // kChooseVec3WithBitFlag (canonical home)
#include "shader/SurfaceMaterialShaders.h"
#include "shader/SurfaceFlagsShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addMaterial — wire the material system into a ProgramBuilder
// Ported from: itwinjs-core Surface.ts addMaterial() (line 183-238)
//
// Parameters:
//   builder — the ProgramBuilder to configure (must have surface flags already added)
//   instanced — true for instanced geometry (no material atlas support)
//   quantized — true when the VertexLUT (quantized) path is active; gates the
//               material-atlas additions (readMaterialAtlas + u_numColors).
//
// Adds:
//   Fragment: mat_texture_weight, mat_weights, mat_specular globals
//   Fragment: decodeMaterialParams initializer, applyTextureWeight component
//   Vertex: mat_rgb, mat_alpha, use_material, g_materialParams globals
//   Vertex: decodeMaterialColor function, u_materialColor/u_materialParams uniforms
//   Vertex: computeMaterial component, applyMaterialColor component
//   Varying: v_materialParams (vec4) computed varying
//   If non-instanced + quantized: readMaterialAtlas, unpackFloat, u_numColors
// ---------------------------------------------------------------------------
inline void addMaterial(ProgramBuilder& builder, bool instanced, bool quantized)
{
    auto& frag = builder.getFragmentBuilder();
    auto& vert = builder.getVertexBuilder();

    // Fragment globals
    frag.addGlobal("mat_texture_weight", VariableType::Float);
    frag.addGlobal("mat_weights", VariableType::Vec2);
    frag.addGlobal("mat_specular", VariableType::Vec4);

    // Fragment helper functions
    frag.addFunction(std::string(kUnpack2Bytes));
    frag.addFunction(std::string(kUnpackAndNormalize2Bytes));
    frag.addFunction(std::string(kDecodeFragMaterialParams));
    frag.addInitializer("decodeMaterialParams(v_materialParams);");

    // u_applyGlyphTex — referenced by applyMaterialOverrides (kApplyTextureWeight).
    // Value wired in the glyph follow-up; default false here so the GLSL compiles.
    // Ported from: itwinjs-core Surface.ts createSurfaceBuilder (line 787-793).
    frag.addUniform("u_applyGlyphTex", VariableType::Boolean, nullptr);

    // Fragment ApplyMaterialOverrides component.
    // Function-call convention: the slot holds the function BODY (ends in
    // `return`); buildFragmentMain wraps it as `vec4 applyMaterialOverrides(vec4
    // baseColor)` and emits `baseColor = applyMaterialOverrides(baseColor);`.
    // Ported from: itwinjs-core Surface.ts applyTextureWeight (line 103-111).
    frag.addFunction(std::string(kChooseVec3WithBitFlag));
    frag.setFragmentComponent(FragmentShaderComponent::ApplyMaterialOverrides,
                              std::string(kApplyTextureWeight));

    // Vertex globals
    vert.addGlobal("mat_rgb", VariableType::Vec4);
    vert.addGlobal("mat_alpha", VariableType::Vec2);
    vert.addGlobal("use_material", VariableType::Boolean);
    vert.addInitializer("use_material = !u_surfaceFlags[kSurfaceBitIndex_IgnoreMaterial];");

    // Vertex helper functions
    vert.addFunction(std::string(kDecodeMaterialColor));

    // Vertex uniforms
    wireMaterialColor(vert);
    wireMaterialParams(vert);

    // Material atlas: only when the VertexLUT (quantized) path is active and
    // geometry is non-instanced. Non-quantized geometry (e.g. glTF polyface) has
    // no atlas; its material comes from u_materialColor via decodeMaterialColor.
    // Ported from: itwinjs-core Surface.ts addMaterial (line 140-180) — itwinjs
    // always has the LUT; this gate is the §3.4 non-LUT-attribute deviation.
    if (!instanced && quantized) {
        vert.addFunction(std::string(kUnpackFloat));
        vert.addFunction(std::string(kReadMaterialAtlas));
        vert.addUniform("u_numColors", VariableType::Float, nullptr);
    }

    // Vertex globals
    vert.addGlobal("g_materialParams", VariableType::Vec4);

    // Vertex ComputeMaterial component — atlas branch only valid when the LUT
    // is present (else readMaterialAtlas is undefined). kComputeMaterialInstanced
    // is the no-atlas body (decodeMaterialColor + g_materialParams = u_materialParams).
    vert.setVertexComponent(VertexShaderComponent::ComputeMaterial,
        std::string((!instanced && quantized) ? kComputeMaterial : kComputeMaterialInstanced));

    // Vertex ApplyMaterialColor component
    vert.setVertexComponent(VertexShaderComponent::ApplyMaterialColor,
                             std::string(kApplyMaterialColor));

    // Computed varying: v_materialParams
    builder.addFunctionComputedVarying("v_materialParams", VariableType::Vec4,
                                        "computeMaterialParams",
                                        std::string(kComputeMaterialParams));
}

END_DQ_RENDER_NAMESPACE
